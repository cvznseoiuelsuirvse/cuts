#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#include "wayland/display.h"
#include "wayland/server.h"
#include "wayland/types.h"
#include "wayland/proto/wayland.h"

#include "compositor/window.h"
#include "compositor/scene.h"

#include "output/output.h"
#include "output/cursor.h"

#include "seat/session/session.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/signal.h"
#include "util/log.h"
#include "util/malloc.h"

#include "config.h"

#define LAYOUT(output)                                                         \
  {                                                                            \
    c_scene_clear(cuts.scene, output->o);                                      \
    cuts.layout.func();                                                        \
    c_output_damage(cuts.mgr, output->o);                                      \
  }

#define clients_for_each_in_tag(comp, client) \
  c_list_for_each((comp).clients, (client)) \
    if ((client->tag & (comp).focused_tag))

#define check_init(t, ret, out_label)                                          \
  if ((t) == NULL) {                                                           \
    (ret) = 1;                                                                 \
    c_log(C_LOG_ERROR, "failed to initialize " #t);                            \
    goto out_label;                                                            \
  }

#define pointer_x(comp) (comp).pointer.x[(comp).pointer.coords]
#define pointer_y(comp) (comp).pointer.y[(comp).pointer.coords]
#define pointer_x_prev(comp) (comp).pointer.x[(comp).pointer.coords ^ 1]
#define pointer_y_prev(comp) (comp).pointer.y[(comp).pointer.coords ^ 1]

struct output {
  c_wl_object_id wl_id;
  struct c_output *o;
};

struct client {
  uint32_t tag;
  struct c_wl_connection *connection;
  struct output *output;
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

  c_list *outputs;
  struct output *focused_output;

  uint32_t focused_tag;
	struct layout layout;
} cuts = {0};

struct {
  uint32_t y, height, width;

  struct {
    uint32_t x, width;
  } master;

  struct {
    uint32_t x, width;
  } stack;

} tile_layout = {0};

struct client *client_new();
void client_free(struct client *client);
void client_change_focus(struct client *client, double hotspot_x, double hotspot_y);
void client_close(struct client *client);
void client_toggle_floating(struct client *client);

int count_tiled();
void calc_tile_layout();

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
    c_window_unfocus(cuts.focused_client->window);
    cuts.focused_client->window->border_color = border.c_default;
  }

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
    if (cuts.clients->size > 0) {
      struct client *last = c_list_get(cuts.clients, cuts.clients->size - 1);
      client_change_focus(last, 0, 0);

    } else {
      cuts.focused_client = NULL;
    }
  }
}

static void *on_wl_output_bind(struct c_wl_connection *conn, c_wl_object_id new_id, c_wl_uint version, void *userdata) {
  struct output *output;
  c_list_for_each(cuts.outputs, output) {
    output->wl_id = new_id;
    struct c_output *c_output = output->o;

    if (version >= 4)
      wl_output_name(conn, new_id, c_output->name);

    wl_output_scale(conn, new_id, 1);
    wl_output_geometry(conn, new_id, 0, 0, c_output->mm_width,
                       c_output->mm_height, c_output->subpixel - 1, "unknown",
                       "unknown", WL_OUTPUT_TRANSFORM_NORMAL);

    struct c_output_mode *mode;
    c_list_for_each(c_output->modes, mode) {
      if (mode->preferred)
        wl_output_mode(conn, new_id,
                       WL_OUTPUT_MODE_PREFERRED | WL_OUTPUT_MODE_CURRENT,
                       mode->width, mode->height, mode->refresh_rate * 1000);
      else
        wl_output_mode(conn, new_id, 0, mode->width, mode->height, mode->refresh_rate * 1000);
    }
  }

  wl_output_done(conn, new_id);
  return output;
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
  pointer_x(cuts) = event->x;
  pointer_y(cuts) = event->y;

  if (cuts.pointer.is_dragging || cuts.clients->size == 0) return;
  struct client *focused = NULL;

  struct client *client;
  clients_for_each_in_tag(cuts, client) {
    struct c_window *window = client->window;
    if (CURSOR_INSIDE(event->x, event->y, window))
        focused = client;
  }
  if (!client) return;

  if (focused == cuts.focused_client) {
    c_window_pointer_move(focused->window, event->x, event->y);
  } else if (focused) {
    client_change_focus(focused, event->x, event->y);
  }
}

void on_mouse_scroll(struct c_input_mouse_event *event, void *userdata) {
  if (!cuts.focused_client) return;
  c_window_pointer_scroll(cuts.focused_client->window, event->axis,
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

void on_surface_new(struct c_wl_surface *surface, void *userdata) {
  struct c_wl_object *wl_output = c_wl_object_get(surface->conn, cuts.focused_output->wl_id);
  assert(wl_output);

  surface->output = wl_output->data;
  c_ref(wl_output->data);

  c_log(C_LOG_DEBUG, "surface id: %d output id: %d", surface->id, wl_output->id);
  wl_surface_enter(surface->conn, surface->id, wl_output->id);
};

void on_surface_commit_cb(struct c_wl_surface *surface, void *userdata) {
  c_output_damage(cuts.mgr, surface->output->output);
}

void on_surface_destroy_cb(struct c_wl_surface *surface, void *userdata) {
  c_output_damage(cuts.mgr, surface->output->output);
}

void on_window_new(struct c_xdg_surface *surface, void *userdata) {
  struct client *client = client_new();
  if (!client) {
    cleanup(1, NULL);
    return;
  }

  struct output *o;
  c_list_for_each(cuts.outputs, o) {
    if (o->wl_id == surface->surface->output->id) {
      client->output = o;
      break;
    }
  }

  client->connection = surface->surface->conn;
  client->window->surface = surface;
  client->window->border_width = border.width;
  client->window->title = &surface->toplevel.title;
  client->window->app_id = &surface->toplevel.app_id;

  c_list_insert(&cuts.clients, 0, client, 0);
  client_change_focus(client, 0, 0);
  LAYOUT(client->output);
}

void on_window_close(struct c_xdg_surface *surface, void *userdata) {
  struct client *client;
  c_list_for_each(cuts.clients, client) {
    if (client->window->surface == surface) {
      int is_visible = client->tag & cuts.focused_tag;
      struct output *output = client->output;

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
      struct output *output = client->output;

      client_close(client);
      if (is_visible) {
        LAYOUT(output)
      }
      break;
    }
  }
}

void quit(bind_args *args) {
  cleanup(0, NULL);
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

void move_focus(bind_args *args) {}

void switch_tag(bind_args *args) {
  cuts.focused_tag = args->u;
  LAYOUT(cuts.focused_output);

  struct client *c;
  clients_for_each_in_tag(cuts, c) {
    client_change_focus(c, 0, 0);
    break;
  }
}

int count_tiled() {
  struct client *client;
  int i = 0;
  clients_for_each_in_tag(cuts, client) {
    if (!(client->window->state & C_WINDOW_FLOAT)) i++;
  }
  return i;
}

void calc_tile_layout() {
  struct c_output_mode *mode = cuts.focused_output->o->current_mode;

  tile_layout.width = mode->width - gap * 2;
  tile_layout.height = mode->height - gap * 2;
  tile_layout.y = gap;

  tile_layout.master.x = gap;
  tile_layout.master.width = mode->width * mfact - gap * 2;

  tile_layout.stack.x = tile_layout.master.width + gap * 2;
  tile_layout.stack.width = mode->width - tile_layout.master.width - gap * 3;
}

void tile() {
  if (tile_layout.height == 0)
    calc_tile_layout();

  if (cuts.clients->size == 0) return;

  uint32_t tiled_clients = count_tiled();
  uint32_t stack_clients, master_clients, master_client_height, stack_client_height;

  if (tiled_clients) {
    stack_clients = (tiled_clients - nmaster) & -(tiled_clients >= nmaster);
    master_clients = tiled_clients - stack_clients;

    master_client_height = (tile_layout.height - gap * (master_clients -1)) / master_clients;
    stack_client_height = stack_clients ? (tile_layout.height - gap * (stack_clients - 1)) / stack_clients : 0;

  } else {
    stack_clients = master_clients = master_client_height = stack_client_height = 0;
  }

  struct client *client;
  size_t i = 0;
  clients_for_each_in_tag(cuts, client) {
    struct c_window *window = client->window;
    c_scene_add_window(cuts.scene, window);
    c_log(C_LOG_DEBUG, "window %s", *window->title);

    if (window->state & C_WINDOW_FLOAT) continue;

    if (i < nmaster) {
      window->x = tile_layout.master.x;
      window->width = stack_clients > 0 ? tile_layout.master.width : tile_layout.width;

      window->height = master_client_height;
      window->y = tile_layout.y + master_client_height * i + gap * i;
        
    } else {
      window->x = tile_layout.stack.x;
      window->width = tile_layout.stack.width;

      window->height = stack_client_height;
      window->y = tile_layout.y + stack_client_height * (i - master_clients) + gap * (i - master_clients);
    }

    c_window_resize(window, window->width, window->height);
    i++;
  }
}

void client_bring_on_top(struct client *client) {
  c_list_remove(&cuts.clients, client);
  c_list_push(cuts.clients, client, 0);
}

void client_toggle_floating(struct client *client) {
  cuts.focused_client->window->state ^= C_WINDOW_FLOAT;
  LAYOUT(client->output);
}

void window_toggle_floating(bind_args *args) {
  if (!cuts.focused_client) return;

  client_bring_on_top(cuts.focused_client);
  client_toggle_floating(cuts.focused_client);
}

void window_move(int done, bind_args *args) {
  if (!cuts.focused_client) return;
  struct client *focused = cuts.focused_client;

  client_bring_on_top(focused);
  if (!(focused->window->state & C_WINDOW_FLOAT))
    client_toggle_floating(focused);
    
  float dist_x = pointer_x(cuts) - pointer_x_prev(cuts);
  float dist_y = pointer_y(cuts) - pointer_y_prev(cuts);

  focused->window->x+=dist_x;
  focused->window->y+=dist_y;

  cuts.pointer.is_dragging = !done;

  LAYOUT(focused->output);
}

void window_resize(int done, bind_args *args) {
  if (!cuts.focused_client) return;
  struct client *focused = cuts.focused_client;

  client_bring_on_top(focused);
  if (!(focused->window->state & C_WINDOW_FLOAT))
    client_toggle_floating(focused);
    
  float dist_x = pointer_x(cuts) - pointer_x_prev(cuts);
  float dist_y = pointer_y(cuts) - pointer_y_prev(cuts);

  focused->window->width+=dist_x;
  focused->window->height+=dist_y;

  c_window_resize(focused->window, focused->window->width, focused->window->height);

  cuts.pointer.is_dragging = !done;

  LAYOUT(focused->output);
}

void cleanup(int err, void *userdata) {
  if (cuts.session) {
    c_session_free(cuts.session);
    cuts.session = NULL;
  }

  if (cuts.display) {
    c_wl_display_free(cuts.display);
    cuts.display = NULL;
  }

  if (cuts.scene) {
    c_scene_free(cuts.scene);
    cuts.scene = NULL;
  }

  if (cuts.clients) {
    struct client *client;
    c_list_for_each(cuts.clients, client)
      client_free(client);
    c_list_destroy(cuts.clients);
    cuts.clients = NULL;
  }

  if (cuts.loop) {
    c_event_loop_free(cuts.loop);
    cuts.loop = NULL;
  }

  if (cuts.outputs) {
    c_list_destroy(cuts.outputs);
    cuts.outputs = NULL;
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

  struct c_session *session = c_session_init(loop, display);
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

  // FIXME: create high level cursor management in cursor/ based of low level
  // (the existing one)
  uint32_t cursor_buffer[32 * 32];
  int cursor_size = 20;

  for (int y = 0; y < cursor_size; y++) {
    for (int x = 0; x < cursor_size; x++) {
      if (y == 0 || y == (cursor_size - 1) || x == 0 || x == (cursor_size - 1)) {
        cursor_buffer[y * cursor_size + x] = 0xFF000000;
      } else {
        cursor_buffer[y * cursor_size + x] = 0xFFFFFFFF;
      }
    }
  }

  cuts.outputs = c_list_new();

  struct c_output *output;
  c_list_for_each(mgr->outputs, output) {
    // FIXME: user high level something like comp.pointer = c_pointer_init("CursorTheme", 24);
    output->cursor = c_cursor_init(mgr, session->input);
    c_cursor_update(mgr, output, cursor_buffer, cursor_size * 2);
    check_init(output->cursor, ret, out);

    c_log(C_LOG_INFO, "%s:", output->name);

    struct c_output_mode *mode;
    c_list_for_each(output->modes, mode) {
      c_log(C_LOG_INFO, "  %dx%d@%f preferred: %s", mode->width, mode->height,
            mode->refresh_rate, mode->preferred ? "yes" : "no");
      if (mode->preferred) {
        c_output_set_mode(mgr, output, mode);
      }
    }

    struct output *o = c_malloc(sizeof(*o));
    o->o = output;
    c_list_push(cuts.outputs, o, 0);

    if (!cuts.focused_output)
      cuts.focused_output = o;
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
    .on_surface_new = on_surface_new,
    .on_surface_commit = on_surface_commit_cb,
    .on_surface_destroy = on_surface_destroy_cb,
    .on_toplevel_new = on_window_new,
    .on_toplevel_destroy = on_window_close,
  };

  c_wl_display_add_listener(display, &dpy_listener, NULL);
  
  c_wl_display_add_supported_interface(display, "wl_output", on_wl_output_bind, NULL);

  ret = c_event_loop_run(loop);

out:
  cleanup(ret, &cuts);
}
