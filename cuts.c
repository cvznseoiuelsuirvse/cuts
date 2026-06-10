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

#include "backend/backend.h"

#include "render/render.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/signal.h"
#include "util/log.h"

#include "config.h"

#define LAYOUT(comp) \
{ \
  c_scene_clear(); \
  (comp).layout.func(); \
  c_render_draw_scene(comp.render); \
}

#define HEX_TO_VEC4(n) \
{ \
  ((n >> 24) & 0xff) / 255.0f, \
  ((n >> 16) & 0xff) / 255.0f, \
  ((n >>  8) & 0xff) / 255.0f, \
  ((n >>  0) & 0xff) / 255.0f, \
}


#define clients_for_each_in_tag(comp, client) \
  c_list_for_each((comp).clients, (client)) \
    if ((client->tag & (comp).current_tag))

void cleanup(int err, void *userdata);

struct client {
  uint32_t tag;
  struct c_window *window;
};

struct {
  struct c_event_loop *loop;
  struct c_wl_display *display;
  struct c_backend    *backend;
  struct c_render     *render;
  struct c_output     *output;

  uint32_t current_tag;

  c_list *clients;
  struct client *focused_client;

	struct layout layout;
} comp;


struct c_output_mode *preferred_output_mode() {
  struct c_output_mode *mode;
  c_list_for_each(comp.output->modes, mode)
    if (mode->preferred) return mode;

  assert(0);
}

void client_close(struct client *client) {
  if (comp.focused_client == client) {
    c_window_unfocus(client->window);
    if (comp.clients->size > 1) {
      struct client *pre_last = c_list_get_end(comp.clients, -2);
      comp.focused_client = pre_last;
    } else {
      comp.focused_client = NULL;
    }
  }

  c_scene_remove_window(client->window);
  free(client->window);
  c_list_remove_ptr(&comp.clients, client);
}

void client_change_focus(struct client *client, double hotspot_x, double hotspot_y) {
  if (client == comp.focused_client) return;

  if (comp.focused_client)
    c_window_unfocus(comp.focused_client->window);

  c_window_focus(client->window, hotspot_x, hotspot_y);
  comp.focused_client = client;
}

int wl_seat_get_keyboard(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id wl_keyboard_id = args[1].o;
  struct c_wl_object *wl_keyboard;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_keyboard_id, wl_keyboard);

  struct c_input *input = comp.backend->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_KEYBOARD))
    c_wl_error_set_and_return(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  c_wl_object_add(conn, wl_keyboard_id, self->version, c_wl_interface_get("wl_keyboard"), NULL);

  int keymap_fd;
  int keymap_len = c_input_get_xkb_keymap_fd(comp.backend->input, &keymap_fd);
  if (keymap_len < 0)
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed ot get xkb_keymap");

  wl_keyboard_keymap(conn, wl_keyboard_id, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymap_fd, keymap_len);
  wl_keyboard_repeat_info(conn, wl_keyboard_id, keyboard_repeat_rate, keyboard_repeat_delay);

  // close(keymap_fd);
  return 0;
}

int wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id wl_pointer_id = args[1].o;
  struct c_wl_object *wl_pointer;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_pointer_id, wl_pointer);

  struct c_input *input = comp.backend->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_POINTER))
    c_wl_error_set_and_return(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  c_wl_object_add(conn, wl_pointer_id, self->version, c_wl_interface_get("wl_pointer"), NULL);

  return 0;
}

void on_mouse_movement(struct c_input_mouse_event *event, void *userdata) {
  if (comp.clients->size == 0) return;

  struct client *client;
  clients_for_each_in_tag(comp, client) {
    struct c_window *window = client->window;
    if (POINTER_INSIDE(event->x, event->y, window)) {
      if (client == comp.focused_client) {
        c_window_pointer_move(window, event->x, event->y);
        return;
      }

      if (comp.focused_client)
        c_window_unfocus(comp.focused_client->window);

      c_window_focus(window, event->x, event->y);
      comp.focused_client = client;

      break;
    }
  }
}
void on_mouse_scroll(struct c_input_mouse_event *event, void *userdata) {
  if (!comp.focused_client) return;
  c_window_pointer_scroll(comp.focused_client->window, event->axis,
                          (enum wl_pointer_axis_source_enum)event->axis_source,
                          event->axis_discrete);
}

void on_mouse_button(struct c_input_mouse_event *event, void *userdata) {
  if (!comp.focused_client) return;
  c_window_pointer_button(comp.focused_client->window, event->button, event->button_pressed);
}

void on_keyboard_key(struct c_input_keyboard_event *event, void *userdata) {
  if (!comp.focused_client) return;

  c_window_keyboard_key(comp.focused_client->window, event->key, event->pressed,
                        event->mods_depressed, event->mods_latched,
                        event->mods_locked, event->group, event->changed);
}

void on_window_new(struct c_wl_surface *surface, void *userdata) {
  struct c_window *window = calloc(1, sizeof(*window));
  window->surface = surface;
  struct client client = {
    .tag = comp.current_tag,
    .window = window,
  };

  struct client *client_cpy =c_list_push(comp.clients, &client, sizeof(client));
  client_change_focus(client_cpy, 0, 0);
  LAYOUT(comp);
}

void on_window_close(struct c_wl_surface *surface, void *userdata) {
  struct client *client;
  c_list_for_each(comp.clients, client) {
    if (client->window->surface == surface) {
      int visible = client->tag & comp.current_tag;
      client_close(client);
      if (visible) goto layout;
      return;
    }
  }

layout:
  LAYOUT(comp);
}

void quit(bind_args *args) {
  cleanup(0, NULL);
}

void sh(bind_args *args) {
  if (fork() == 0) {
    close(STDIN_FILENO);
    open("/dev/null", O_RDWR);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    setsid();
    execvp("/bin/sh", (char *const []){"/bin/sh", "-c", args->s, NULL});
  }
}

void kill_window(bind_args *args) {
  if (!comp.focused_client) return;
  c_window_close(comp.focused_client->window);
}

void move_focus(bind_args *args) {}

void switch_tag(bind_args *args) {
  struct client *client; 
  clients_for_each_in_tag(comp, client) {
    c_scene_remove_window(client->window);
  }

  comp.current_tag = args->u;

  struct client *c;
  clients_for_each_in_tag(comp, c) {
    client_change_focus(c, 0, 0);
    break;
  }

  LAYOUT(comp);
}

int count_tiled() {
  struct client *client;
  int i = 0;
  clients_for_each_in_tag(comp, client) {
    if (!(client->window->state & C_WINDOW_FLOAT)) i++;
  }
  return i;
}

void tile() {
  uint32_t scene_width, scene_height;
  struct c_output_mode *preferred = preferred_output_mode();
  scene_width = preferred->width;
  scene_height = preferred->height;
   
  int tiled_clients = count_tiled();
  if (tiled_clients == 0) return;

  int window_count = 0;

  uint32_t tag_window_height = scene_height - gap * 2;
  uint32_t tag_window_width = (scene_width - gap * (tiled_clients + 1)) / tiled_clients;

  struct client *client;
  clients_for_each_in_tag(comp, client) {
    struct c_window *window = client->window;
    c_scene_add_window(window);

    if (window->state & C_WINDOW_FLOAT) continue;

    window->x = (gap * (window_count + 1)) + (window_count * tag_window_width);
    window->y = gap;

    window->width = tag_window_width;
    window->height = tag_window_height;

    c_window_resize(window, window->width, window->height, client == comp.focused_client);
    window_count++;
  }
}

void cleanup(int err, void *userdata) {
  if (comp.display) {
    c_wl_display_free(comp.display);
    comp.display = NULL;
  }

  if (comp.render)  {
    c_render_free(comp.render);
    comp.render = NULL;
  }

  if (comp.backend) {
    c_backend_free(comp.backend);
    comp.backend = NULL;
  }

  if (comp.clients) {
    struct client *client;
    c_list_for_each(comp.clients, client)
      free(client->window);
    c_list_destroy(comp.clients);
    comp.clients = NULL;
  }

  if (comp.loop) {
    c_event_loop_free(comp.loop);
    comp.loop = NULL;
  }

  exit(err);
}


int main() {
  int ret = 0;
  comp.current_tag = 1 << 0;

  c_signal_handler_add(SIGTERM, cleanup, NULL);
  c_signal_handler_add(SIGINT, cleanup, NULL);
  c_signal_handler_add(SIGABRT, cleanup, NULL);

  float background4[4] = HEX_TO_VEC4(background);
  c_scene_set_background(background4);

  struct c_log_config cfg;
  cfg.level_mask = C_LOG_INFO | C_LOG_ERROR | C_LOG_DEBUG | C_LOG_WARNING;
  cfg.level_mask |= C_LOG_WAYLAND;
  cfg.color = 1;
  c_log_setup(&cfg);

  struct c_event_loop *loop = c_event_loop_init();
  if (!loop) {
    c_log(C_LOG_ERROR, "failed to create event loop");
    goto out;
  }
  comp.loop = loop;

  struct c_wl_display *display = c_wl_display_init(loop);
  if (!display) {
    c_log(C_LOG_ERROR, "failed to initialize wl_display");
    goto out;
  }

  comp.display = display;
  comp.clients = c_list_new();

  comp.layout = layouts[0];

  struct c_backend *backend = c_backend_init(loop, display);
  if (!backend) {
    c_log(C_LOG_ERROR, "failed to initialize backend");
    goto out;
  }

  comp.output = backend->drm->output;

  if (c_input_init_xkb_state(backend->input, &xkb_rules) < 0) goto out;

  struct c_input_event_listener_mouse mouse_listener = {
    .on_mouse_movement = on_mouse_movement,
    .on_mouse_button = on_mouse_button,
    .on_mouse_scroll = on_mouse_scroll,
  };
  c_input_add_event_listener_mouse(backend->input, &mouse_listener, NULL);

  struct c_input_event_listener_keyboard listener_keyboard = {
    .on_keyboard_key = on_keyboard_key,
  };
  c_input_add_event_listener_keyboard(backend->input, &listener_keyboard, NULL);

  for(size_t i = 0; i < LENGTH(binds); i++) {
    struct bind *b = &binds[i];
    c_input_add_shortcut_handler(backend->input, b->modmask, b->keysym, (void (*)(void *))b->func, &b->args);
  }

  comp.backend = backend;

  struct c_render *render = c_render_init(loop, display, backend->drm);
  if (!render) goto out;

  struct c_wl_display_listener dpy_listener = {
    .on_toplevel_new = on_window_new,
    .on_toplevel_destroy = on_window_close,
  };
  c_wl_display_add_listener(display, &dpy_listener, NULL);

  comp.render = render;

  ret = c_event_loop_run(loop);

out:
  cleanup(ret, &comp);
}
