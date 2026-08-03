#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#include "compositor/window.h"
#include "compositor/scene.h"

#include "output/output.h"
#include "output/cursor.h"

#include "seat/session/session.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/signal.h"
#include "util/log.h"

#include "config.h"

#define LAYOUT(output)                                                         \
  {                                                                            \
    c_scene_clear(cuts.scene, output);                                         \
    cuts.layout.func();                                                        \
    c_output_damage(cuts.mgr, output);                                         \
  }

#define clients_for_each_in_tag(client) \
  c_list_for_each(cuts.clients, (client)) \
    if ((client->tag & cuts.focused_tag))

#define check_init(t, ret, out_label)                                          \
  if ((t) == NULL) {                                                           \
    (ret) = 1;                                                                 \
    c_log(C_LOG_ERROR, "failed to initialize " #t);                            \
    goto out_label;                                                            \
  }

#define pointer_x cuts.pointer.x[cuts.pointer.coords]
#define pointer_y cuts.pointer.y[cuts.pointer.coords]
#define pointer_x_prev cuts.pointer.x[cuts.pointer.coords ^ 1]
#define pointer_y_prev cuts.pointer.y[cuts.pointer.coords ^ 1]

typedef enum bar_pos {
  BAR_TOP = 1,
  BAR_BOTTOM,
  BAR_RIGHT,
  BAR_LEFT,
} bar_pos;

struct bar {
  uint32_t width, height;
  bar_pos pos;
};

struct client {
  uint32_t tag;
  struct c_wl_connection *connection;
  struct c_output *output;
  struct c_window *window;
};

struct {
  struct c_event_loop *loop;
  struct c_wl_display *display;
  struct c_session *session;
  struct c_output_manager *mgr;
  struct c_scene *scene;

  struct {
    float x[2];
    float y[2];
    int coords;
    int is_dragging;
  } pointer;

  c_list *clients;
  struct client *focused_client;
  struct c_output *focused_output;

  uint32_t focused_tag;
  int focused_client_idx;
	struct layout layout;
  struct bar bar;

} cuts = {0};

struct tile_layout {
  uint32_t x, y, height, width;

  struct {
    uint32_t x, width;
  } master;

  struct {
    uint32_t x, width;
  } stack;

};

struct client *client_new();
void client_free(struct client *client);
void client_change_focus(struct client *client, double hotspot_x, double hotspot_y);
void client_close(struct client *client);
void client_toggle_floating(struct client *client);

int count_tiled();
void calc_tile_layout(struct c_output *output, struct tile_layout *layout);

int wl_seat_get_keyboard(struct c_wl_connection *conn, union c_wl_arg *args);
int wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args);

void on_mouse_movement(struct c_input_mouse_event *event, void *userdata);
void on_mouse_scroll(struct c_input_mouse_event *event, void *userdata);
void on_mouse_button(struct c_input_mouse_event *event, void *userdata);
void on_keyboard_key(struct c_input_keyboard_event *event, void *userdata);
void on_window_new(struct c_xdg_surface *surface, void *userdata);
void on_window_close(struct c_xdg_surface *surface, void *userdata);

void tile();
void monocle();

void quit(bind_args *args);
void spawn(bind_args *args);
void move_focus(bind_args *args);
void switch_tag(bind_args *args);
void window_kill(bind_args *args);
void window_toggle_floating(bind_args *args);
void window_move(int done, bind_args *args);

void cleanup(int err, void *userdata);

struct client *client_move_focus(int direction) {
  uint32_t clients = 0;

  struct client *client;
  clients_for_each_in_tag(client)
    clients++;

  if (!clients) return NULL;

  cuts.focused_client_idx =
      (cuts.focused_client_idx + direction + clients) % clients;

  int c = 0;
  clients_for_each_in_tag(client) {
    if (cuts.focused_client_idx == c++) {
      return client;
    }
  }

  return NULL;
}


struct client *client_new() {
  struct c_window *window = calloc(1, sizeof(*window));
  if (!window) {
    c_log_errno(C_LOG_ERROR, "failed to allocate window for a new client");
    return NULL;
  }

  struct client *client = calloc(1, sizeof(*client));
  if (!window) {
    c_log_errno(C_LOG_ERROR, "failed to allocate a new client");
    free(window);
    return NULL;
  }

  client->window = window;
  client->tag = cuts.focused_tag;
  return client;
}

void client_free(struct client *client) {
  free(client->window);
  free(client);
}


void client_change_focus(struct client *client, double hotspot_x, double hotspot_y) {
  if (client == cuts.focused_client) return;

  if (cuts.focused_client) {
    c_window_deactivate(cuts.focused_client->window);
    c_window_unfocus(cuts.focused_client->window);
    cuts.focused_client->window->border_color = border.c_default;
  }

  c_window_activate(client->window);
  c_window_focus(client->window, hotspot_x, hotspot_y);
  cuts.focused_client = client;
  cuts.focused_client->window->border_color = border.c_focus;
}

void client_close(struct client *client) {
  int is_focused = cuts.focused_client == client;

  if (is_focused) {
    c_window_unfocus(client->window);
    cuts.focused_client = NULL;
  }
  
  c_scene_remove_window(cuts.scene, client->window);
  client_free(client);
  c_list_remove(&cuts.clients, client);

  if (is_focused) {
    struct client *prev = client_move_focus(-1);

    if (prev)
      client_change_focus(prev, pointer_x, pointer_y);

    else
      cuts.focused_client = NULL;
  }
}

static void *on_wl_output_bind(struct c_wl_connection *conn,
                               struct c_wl_object *wl_output, void *userdata) {
  struct c_output *output = userdata;

  if (wl_output->version >= 4)
    wl_output_name(conn, wl_output->id, output->name);

  wl_output_scale(conn, wl_output->id, 1);
  wl_output_geometry(conn, wl_output->id, 0, 0, output->mm_width,
                     output->mm_height, output->subpixel - 1, "unknown",
                     "unknown", WL_OUTPUT_TRANSFORM_NORMAL);

  struct c_output_mode *mode;
  c_list_for_each(output->modes, mode) {
    int flags = 0;
    if (mode->preferred)
      flags |= WL_OUTPUT_MODE_PREFERRED;

    if (mode == output->current_mode)
      flags |= WL_OUTPUT_MODE_CURRENT;

    wl_output_mode(conn, wl_output->id, flags, mode->width, mode->height, mode->refresh_rate * 1000);

  }

  wl_output_done(conn, wl_output->id);
  return NULL;
}

int wl_seat_get_keyboard(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id wl_keyboard_id = args[1].o;
  struct c_wl_object *wl_keyboard;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_keyboard_id, wl_keyboard);

  struct c_input *input = cuts.session->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_KEYBOARD))
    c_wl_error_set_and_return(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  c_wl_object_add(conn, wl_keyboard_id, self->version, c_wl_interface_get("wl_keyboard"), NULL);

  int keymap_fd;
  int keymap_len = c_input_get_xkb_keymap_fd(input, &keymap_fd);
  if (keymap_len < 0)
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed ot get xkb_keymap");

  wl_keyboard_keymap(conn, wl_keyboard_id, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymap_fd, keymap_len);
  wl_keyboard_repeat_info(conn, wl_keyboard_id, keyboard_repeat_rate, keyboard_repeat_delay);

  return 0;
}

int wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id wl_pointer_id = args[1].o;
  struct c_wl_object *wl_pointer;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_pointer_id, wl_pointer);

  struct c_input *input = cuts.session->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_POINTER))
    c_wl_error_set_and_return(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  c_wl_object_add(conn, wl_pointer_id, self->version, c_wl_interface_get("wl_pointer"), NULL);

  return 0;
}

void on_mouse_movement(struct c_input_mouse_event *event, void *userdata) {
  cuts.pointer.coords ^= 1;
  pointer_x = event->x;
  pointer_y = event->y;

  if (cuts.pointer.is_dragging || cuts.clients->size == 0) return;
  struct client *focused = NULL;

  struct client *client;
  clients_for_each_in_tag(client) {
    struct c_window *window = client->window;
    if (CURSOR_INSIDE(event->x, event->y, window))
        focused = client;
  }

  if (focused == cuts.focused_client) {
    c_window_pointer_move(focused->window, event->x, event->y);
  } else if (focused) {
    client_change_focus(focused, event->x, event->y);
  }
}

void on_mouse_scroll(struct c_input_mouse_event *event, void *userdata) {
  if (!cuts.focused_client) return;
  c_window_pointer_scroll(cuts.focused_client->window, event->axis,
                          event->axis120,
                          (enum wl_pointer_axis_source_enum)event->axis_source,
                          event->axis_discrete);
}

void on_mouse_button(struct c_input_mouse_event *event, void *userdata) {
  if (!cuts.focused_client) return;
  c_log_value(event->button, "%d");
  c_window_pointer_button(cuts.focused_client->window, event->button, event->is_pressed);
}

void on_keyboard_key(struct c_input_keyboard_event *event, void *userdata) {
  if (!cuts.focused_client) return;

  c_window_keyboard_key(cuts.focused_client->window, event->key, event->pressed,
                        event->mods_depressed, event->mods_latched,
                        event->mods_locked, event->group, event->changed);
}

static struct c_wl_surface *find_root_surface(struct c_wl_surface *surface) {
 while (surface->sub.surface && surface->sub.surface->parent) {
   surface = surface->sub.surface->parent;
 }

 while (surface->xdg_surface && surface->xdg_surface->parent) {
   surface = surface->xdg_surface->parent->surface;
 }
 return surface;
}

static void damage_from_surface(struct c_wl_surface *surface) {
  if (!cuts.clients) return;

  struct c_wl_surface *root_surface = find_root_surface(surface);
  struct client *client;
  c_list_for_each(cuts.clients, client) {
    if (client->window->surface->surface == root_surface) {
      c_output_damage(cuts.mgr, client->output);
      break;
    }
  }
}

void on_surface_commit_cb(struct c_wl_surface *surface, void *userdata) {
  damage_from_surface(surface);
}

void on_surface_destroy_cb(struct c_wl_surface *surface, void *userdata) {
  damage_from_surface(surface);
}

void on_window_new(struct c_xdg_surface *surface, void *userdata) {
  struct client *client = client_new();
  if (!client) {
    cleanup(1, NULL);
    return;
  }

  client->output = cuts.focused_output;
  client->connection = surface->surface->conn;
  client->window->surface = surface;
  client->window->border_width = border.width;
  client->window->title = &surface->toplevel.title;
  client->window->app_id = &surface->toplevel.app_id;

  c_list_insert(&cuts.clients, 0, client, 0);
  client_change_focus(client, pointer_x, pointer_y);

  LAYOUT(client->output);
}

void on_window_close(struct c_xdg_surface *surface, void *userdata) {
  struct client *client;
  c_list_for_each(cuts.clients, client) {
    if (client->window->surface == surface) {
      int is_visible = client->tag & cuts.focused_tag;
      struct c_output *output = client->output;

      client_close(client);
      if (is_visible) {
        LAYOUT(output)
      }
      break;
    }
  }
}

void on_connection_gone(struct c_wl_connection *conn, void *userdata) {
  struct client *client;
  c_list_for_each(cuts.clients, client) {
    if (client->connection == conn) {
      int is_visible = client->tag & cuts.focused_tag;
      struct c_output *output = client->output;

      client_close(client);
      if (is_visible) {
        LAYOUT(output)
      }
      break;
    }
  }
}

void quit(bind_args *args) {
  raise(SIGTERM);
}

void spawn(bind_args *args) {
  if (fork() == 0) {
    close(STDIN_FILENO);
    open("/dev/null", O_RDWR);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    setsid();
    execvp("/bin/sh", (char *const []){"/bin/sh", "-c", args->s, NULL});
  }
}

void window_kill(bind_args *args) {
  if (!cuts.focused_client) return;
  c_window_close(cuts.focused_client->window);
}

void move_focus(bind_args *args) {
  struct client *next = client_move_focus(-1);

  if (next)
    client_change_focus(next, pointer_x, pointer_y);

}

void switch_tag(bind_args *args) {
  cuts.focused_tag = args->u;
  LAYOUT(cuts.focused_output);

  struct client *c;
  clients_for_each_in_tag(c) {
    client_change_focus(c, 0, 0);
    break;
  }
}

int count_tiled() {
  struct client *client;
  int i = 0;
  clients_for_each_in_tag(client) {
    if (!(client->window->state & C_WINDOW_FLOAT)) i++;
  }
  return i;
}

void calc_tile_layout(struct c_output *output, struct tile_layout *layout) {
  struct c_output_mode *mode = cuts.focused_output->current_mode;

  layout->width = mode->width - gap * 2;
  layout->height = mode->height - gap * 2;
  layout->y = layout->x = gap;

  if (cuts.bar.pos == BAR_TOP) {
    layout->height -= cuts.bar.height;
    layout->y += cuts.bar.height;
  }

  if (cuts.bar.pos == BAR_BOTTOM) {
    layout->height -= cuts.bar.height;
  }

  if (cuts.bar.pos == BAR_RIGHT) {
    layout->width -= cuts.bar.width;
  }

  if (cuts.bar.pos == BAR_LEFT) {
    layout->width -= cuts.bar.width;
    layout->x += cuts.bar.width;

  }

  layout->master.x = layout->x;
  layout->master.width = mode->width * mfact - gap * 2;

  layout->stack.x = layout->master.width + gap * 2;
  layout->stack.width = mode->width - layout->master.width - gap * 3;
}

void tile() {
  // FIXME: map tags to outputs and get output from current tag or something
  struct tile_layout layout;
  calc_tile_layout(cuts.focused_output, &layout);

  if (cuts.clients->size == 0) return;

  uint32_t tiled_clients = count_tiled();
  uint32_t stack_clients, master_clients, master_client_height, stack_client_height;

  if (tiled_clients) {
    stack_clients = (tiled_clients - nmaster) & -(tiled_clients >= nmaster);
    master_clients = tiled_clients - stack_clients;

    master_client_height = (layout.height - gap * (master_clients -1)) / master_clients;
    stack_client_height = stack_clients ? (layout.height - gap * (stack_clients - 1)) / stack_clients : 0;

  } else {
    stack_clients = master_clients = master_client_height = stack_client_height = 0;
  }

  struct client *client;
  size_t i = 0;
  clients_for_each_in_tag(client) {
    struct c_window *window = client->window;
    c_scene_add_window(cuts.scene, window);

    if (window->state & C_WINDOW_FLOAT) continue;

    if (i < nmaster) {
      window->x = layout.master.x;
      window->width = stack_clients > 0 ? layout.master.width : layout.width;

      window->height = master_client_height;
      window->y = layout.y + master_client_height * i + gap * i;
        
    } else {
      window->x = layout.stack.x;
      window->width = layout.stack.width;

      window->height = stack_client_height;
      window->y = layout.y + stack_client_height * (i - master_clients) + gap * (i - master_clients);
    }

    if (cuts.focused_client == client)
      c_window_activate(window);
    else
      c_window_deactivate(window);

    i++;
  }
}

void client_push(struct client *client) {
  c_list_remove(&cuts.clients, client);
  c_list_push(cuts.clients, client, 0);
}

void client_toggle_floating(struct client *client) {
  cuts.focused_client->window->state ^= C_WINDOW_FLOAT;
  LAYOUT(client->output);
}

void toggle_floating(bind_args *args) {
  if (!cuts.focused_client) return;

  client_push(cuts.focused_client);
  client_toggle_floating(cuts.focused_client);
}

void window_move(int done, bind_args *args) {
  if (!cuts.focused_client) return;
  struct client *focused = cuts.focused_client;

  client_push(focused);
  if (!(focused->window->state & C_WINDOW_FLOAT))
    client_toggle_floating(focused);
    
  double dist_x = pointer_x - pointer_x_prev;
  double dist_y = pointer_y - pointer_y_prev;

  focused->window->x+=dist_x;
  focused->window->y+=dist_y;

  cuts.pointer.is_dragging = !done;

  LAYOUT(focused->output);
}

void window_move_to_workspace(bind_args *args) {
  if (!cuts.focused_client) return;

  cuts.focused_client->tag = args->u;
  client_push(cuts.focused_client);

  LAYOUT(cuts.focused_client->output);
}

void window_resize(int done, bind_args *args) {
  if (!cuts.focused_client) return;
  struct client *focused = cuts.focused_client;

  client_push(focused);
  if (!(focused->window->state & C_WINDOW_FLOAT))
    client_toggle_floating(focused);
    
  float dist_x = pointer_x - pointer_x_prev;
  float dist_y = pointer_y - pointer_y_prev;

  focused->window->width+=dist_x;
  focused->window->height+=dist_y;

  c_window_activate(focused->window);

  cuts.pointer.is_dragging = !done;


  LAYOUT(focused->output);
}


void create_bar() {}

void set_cursor(struct c_output *output, int size) {
  struct c_cursor *cursor = output->cursor;
  uint32_t buffer[cursor->width * cursor->height];
  memset(buffer, 0, sizeof(buffer));
  int b = 3;

  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      int dist_x = MIN(x, size - x - 1);
      int dist_y = MIN(y, size - y - 1);
      if (dist_x < b || dist_y < b) {
        buffer[y * cursor->height + x] = 0xffffffff;
      } else {
        buffer[y * cursor->height + x] = 0xff000000;
      }
    } 
  }
  c_cursor_update(cuts.mgr, output, buffer, sizeof(buffer));
}

void cleanup(int err, void *userdata) {
  if (cuts.session) {
    c_session_free(cuts.session);
    cuts.session = NULL;
  }

  if (cuts.clients) {
    struct client *client;
    c_list_for_each(cuts.clients, client)
      client_free(client);
    c_list_destroy(cuts.clients);
    cuts.clients = NULL;
  }

  if (cuts.display) {
    c_wl_display_free(cuts.display);
    cuts.display = NULL;
  }

  if (cuts.scene) {
    c_scene_free(cuts.scene);
    cuts.scene = NULL;
  }

  if (cuts.loop) {
    c_event_loop_free(cuts.loop);
    cuts.loop = NULL;
  }

  if (cuts.mgr) {
    c_output_manager_free(cuts.mgr);
    cuts.mgr = NULL;
  }

  exit(err);
}

int main() {
  int ret = 0;
  cuts.focused_tag = 1 << 0;

  c_signal_handler_add(SIGTERM, cleanup, NULL);
  c_signal_handler_add(SIGINT, cleanup, NULL);
  c_signal_handler_add(SIGABRT, cleanup, NULL);

  struct c_log_config cfg;
  cfg.level_mask = C_LOG_INFO | C_LOG_DEBUG | C_LOG_ERROR | C_LOG_WARNING;
  cfg.level_mask |= C_LOG_WAYLAND;
  cfg.color = 1;
  c_log_setup(&cfg);

  struct c_event_loop *loop = c_event_loop_init();
  check_init(loop, ret, out);
  cuts.loop = loop;

  struct c_wl_display *display = c_wl_display_init(loop);
  check_init(display, ret, out);
  cuts.display = display;

  struct c_input_config input_config = {
    .accel_profile = accel_profile,
  };
  struct c_session *session = c_session_init(loop, display, &input_config);
  check_init(session, ret, out);
  cuts.session = session;

  struct c_output_manager *mgr = c_output_manager_init(session, loop, display);
  check_init(mgr, ret, out);
  cuts.mgr = mgr;

  struct c_scene *scene = c_scene_init(mgr);
  check_init(scene, ret, out);
  cuts.scene = scene;

  float background4[4] = HEX_TO_VEC4(background);
  c_scene_set_background(scene, background4);

  cuts.clients = c_list_new();
  cuts.layout = layouts[0];

  struct c_output *output;
  c_list_for_each(mgr->outputs, output) {
    set_cursor(output, 20);

    c_log(C_LOG_INFO, "Monitor %s:", output->name);

    struct c_output_mode *preferred;
    struct c_output_mode *mode;
    c_list_for_each(output->modes, mode) {
      c_log(C_LOG_INFO, "   %s%dx%d@%.3fHz", mode->preferred ? "*" : " ",
            mode->width, mode->height, mode->refresh_rate);
      if (mode->preferred)
        preferred = mode;

      for (size_t i = 0; i < LENGTH(monitors); i++) {
        struct monitor m = monitors[i];
        if (STREQ(m.name, output->name) && m.width == mode->width &&
            m.height == mode->height && m.refresh_rate == (uint32_t)mode->refresh_rate) {
          c_output_set_mode(mgr, output, mode);
          goto mode_iter_end;
        }
      }
    }

    c_output_set_mode(mgr, output, preferred);

mode_iter_end:
    c_wl_display_add_supported_interface(display, "wl_output", on_wl_output_bind, output);

    if (!cuts.focused_output)
      cuts.focused_output = output;
  }

  if (c_input_init_xkb_state(session->input, &xkb_rules) < 0) goto out;

  struct c_input_event_listener_mouse mouse_listener = {
    .on_mouse_movement = on_mouse_movement,
    .on_mouse_button = on_mouse_button,
    .on_mouse_scroll = on_mouse_scroll,
  };
  c_input_add_event_listener_mouse(session->input, &mouse_listener, NULL);

  struct c_input_event_listener_keyboard listener_keyboard = {
    .on_keyboard_key = on_keyboard_key,
  };
  c_input_add_event_listener_keyboard(session->input, &listener_keyboard, NULL);

  for(size_t i = 0; i < LENGTH(keys); i++) {
    struct key_bind *b = &keys[i];
    c_input_add_combo_handler(session->input, b->modmask, b->keysym, 0, (void (*)(void *))b->handler, &b->args);
  }

  for(size_t i = 0; i < LENGTH(mouse); i++) {
    struct mouse_bind *b = &mouse[i];
    if (b->drag)
      c_input_add_drag_combo_handler(
          session->input, b->modmask, b->keysym, b->btn,
          (void (*)(int, void *))b->drag_handler, &b->args);

    else
      c_input_add_combo_handler(session->input, b->modmask, b->keysym, b->btn,
                                (void (*)(void *))b->handler, &b->args);
  }

  struct c_wl_display_listener dpy_listener = {
    .on_connection_gone = on_connection_gone,
    .on_surface_commit = on_surface_commit_cb,
    .on_surface_destroy = on_surface_destroy_cb,
    .on_toplevel_new = on_window_new,
    .on_toplevel_destroy = on_window_close,
  };

  c_wl_display_add_listener(display, &dpy_listener, NULL);

  ret = c_event_loop_run(loop);

out:
  cleanup(ret, &cuts);
}
