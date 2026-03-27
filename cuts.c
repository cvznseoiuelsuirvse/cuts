#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#include "wayland/display.h"
#include "wayland/server.h"
#include "wayland/types.h"

#include "wayland/types/wayland.h"

#include "backend/backend.h"

#include "render/render.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/signal.h"
#include "util/log.h"

#include "config.h"

struct client {
  struct c_wl_connection *conn;
  struct c_window *window;

  c_wl_object_id wl_pointer_id;
  c_wl_object_id wl_keyboard_id;
  
};

struct cuts {
  struct c_wl_display *display;
  struct c_backend    *backend;
  struct c_render     *render;
  struct c_output     *output;

  c_list 	      *clients;
  struct client *focused_client;
  int           focused_client_n;

  size_t        n_stacked_windows;
  size_t        curr_layout;
  struct layout *layouts;
};

struct cuts *comp;

#define LAYOUT(comp) (comp)->layouts[(comp)->curr_layout].func()

struct client *get_client_from_connection(struct c_wl_connection *conn) {
  struct client *c;
  c_list_for_each(comp->clients, c) {
    if (c->conn == conn) return c;
  }
  return NULL;
}

struct client *get_client_from_window(struct c_window *window) {
  struct client *c;
  c_list_for_each(comp->clients, c) {
    if (c->window == window) return c;
  }
  return NULL;
}

void client_focus(struct client *client, double mx, double my) {
  struct c_window *window = client->window;

  c_wl_array arr = {0};
  if (client->wl_keyboard_id)
    wl_keyboard_enter(window->wl_surface->conn, client->wl_keyboard_id, c_wl_serial(), window->wl_surface->id, &arr);
  if (client->wl_pointer_id) {
    wl_pointer_enter(window->wl_surface->conn, client->wl_pointer_id, c_wl_serial(), window->wl_surface->id, mx, my);
    wl_pointer_frame(client->conn, client->wl_pointer_id);
  }
  c_window_activate(window);
  comp->focused_client = client;
};

void client_unfocus(struct client *client) {
  struct c_window *window = client->window;

  if (client->wl_keyboard_id)
    wl_keyboard_leave(window->wl_surface->conn, client->wl_keyboard_id, c_wl_serial(), window->wl_surface->id);
  if (client->wl_pointer_id) {
    wl_pointer_leave(window->wl_surface->conn, client->wl_pointer_id, c_wl_serial(), window->wl_surface->id);
    wl_pointer_frame(client->conn, client->wl_pointer_id);
  }

  c_window_deactivate(window);
  comp->focused_client = NULL;
};

void client_close(struct cuts *comp, struct client *client) {
  if (client->window && !(client->window->state & C_WINDOW_FLOAT))
    comp->n_stacked_windows--;

  if (comp->focused_client == client) {
    client_unfocus(client);
    comp->focused_client = NULL;
  }

  if (client->window)
    c_window_close(client->window);

  c_list_remove_ptr(&comp->clients, client);

  LAYOUT(comp);
}

int on_client_new(struct c_wl_connection *conn, void *userdata) {
  struct client client = {0};
  client.conn = conn;
  
  c_list_push(comp->clients, &client, sizeof(client));
  return 0;
};

int on_client_gone(struct c_wl_connection *conn, void *userdata) {
  struct client *client = get_client_from_connection(conn);
  if (!client) return 0;
  client_close(comp, client);
  return 0;
}

int on_window_new_cb(struct c_window *window, void *userdata) {
  struct client *client = get_client_from_connection(window->wl_surface->conn);
  client->window = window;

  if (!(window->state & C_WINDOW_FLOAT))
    comp->n_stacked_windows++;

  LAYOUT(comp);
  return 0;
}

int on_window_close_cb(struct c_window *window, void *userdata) {
  struct client *client = get_client_from_window(window);
  if (!client) return 0;

  client_close(comp, client);
  return 0;
}

int on_keyboard_key(struct c_input_keyboard_event *event, void *userdata) {
  struct cuts *comp = userdata;
  if (!comp->focused_client || !comp->focused_client->wl_keyboard_id) return 0;

  struct client *client = comp->focused_client;
  struct c_window *window = client->window;
  struct c_wl_surface *surface = window->wl_surface;

  wl_keyboard_key(surface->conn, client->wl_keyboard_id, c_wl_serial(), c_since_start_ms(), 
                  event->key, event->pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);

  if (event->changed)
    wl_keyboard_modifiers(surface->conn, client->wl_keyboard_id, c_wl_serial(), 
                          event->mods_depressed, event->mods_latched, event->mods_locked, event->group);

  return 0;
}

int on_mouse_scroll(struct c_input_mouse_event *event, void *userdata) {
  if (!comp->focused_client) return 0;
  struct client *client = comp->focused_client;
  if (!client->wl_pointer_id) return 0;
  
  if (!event->axis) {
    c_log(C_LOG_WARNING, "axis value is 0");
    return 0;
  }

  wl_pointer_axis_source(client->conn, client->wl_pointer_id, event->axis_source - 1);
  wl_pointer_axis_discrete(client->conn, client->wl_pointer_id, WL_POINTER_AXIS_VERTICAL_SCROLL, event->axis_discrete);
  // wl_pointer_axis_relative_direction(client->conn, client->wl_pointer_id, WL_POINTER_AXIS_VERTICAL_SCROLL, WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
  wl_pointer_axis(client->conn, client->wl_pointer_id, c_since_start_ms(), WL_POINTER_AXIS_VERTICAL_SCROLL, (float)event->axis);
  wl_pointer_frame(client->conn, client->wl_pointer_id);
  return 0;
}

int on_mouse_button(struct c_input_mouse_event *event, void *userdata) {
  if (!comp->focused_client) return 0;
  struct client *client = comp->focused_client;
  if (!client->wl_pointer_id) return 0;

  wl_pointer_button(client->conn, client->wl_pointer_id, c_wl_serial(), c_since_start_ms(), event->button, event->button_pressed);
  wl_pointer_frame(client->conn, client->wl_pointer_id);
  return 0;
}

int on_mouse_movement(struct c_input_mouse_event *event, void *userdata) {
  double mx = event->x;
  double my = event->y;
  uint32_t wx, wy, ww, wh;

  struct client *focused_client = comp->focused_client;
  struct client *client;
  int i = 0;
  c_list_for_each(comp->clients, client) {
    struct c_window *w = client->window;
    if (!w) continue;
    wx = w->x;
    wy = w->y;
    ww = w->width;
    wh = w->height;

    if (wx <= mx && mx <= wx + ww &&
        wy <= my && my <= wy + wh) {
      c_wl_fixed hotspot_x = c_wl_fixed_from_double(mx - wx);
      c_wl_fixed hotspot_y = c_wl_fixed_from_double(my - wy);

      if (focused_client == client) {
        wl_pointer_motion(focused_client->conn, focused_client->wl_pointer_id, c_since_start_ms(), hotspot_x, hotspot_y);
        wl_pointer_frame(focused_client->conn, focused_client->wl_pointer_id);
        return 0;
      }

      if (focused_client)
        client_unfocus(focused_client);
      client_focus(client, hotspot_x, hotspot_y);
      comp->focused_client_n = i;
      return 0;
    }
    i++;
  }

  return 0;
}

int wl_seat_get_keyboard(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_keyboard_id = args[1].o;
  struct c_wl_object *wl_keyboard;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_keyboard_id, wl_keyboard);

  struct client *client = get_client_from_connection(conn);

  struct c_input *input = comp->backend->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_KEYBOARD))
    return c_wl_error_set(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  client->wl_keyboard_id = wl_keyboard_id;
  c_wl_object_add(conn, wl_keyboard_id, c_wl_interface_get("wl_keyboard"), NULL);

  int keymap_fd;
  int keymap_len = c_input_get_xkb_keymap_fd(comp->backend->input, &keymap_fd);
  if (keymap_len < 0)
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed ot get xkb_keymap");

  wl_keyboard_keymap(conn, wl_keyboard_id, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymap_fd, keymap_len);

  return 0;
}

int wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_pointer_id = args[1].o;
  struct c_wl_object *wl_pointer;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_pointer_id, wl_pointer);

  struct client *client = get_client_from_connection(conn);

  struct c_input *input = comp->backend->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_POINTER))
    return c_wl_error_set(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  client->wl_pointer_id = wl_pointer_id;
  c_wl_object_add(conn, wl_pointer_id, c_wl_interface_get("wl_pointer"), NULL);

  return 0;
}

void cleanup(int err, void *userdata) {
  if (comp->render)  c_render_free(comp->render);
  if (comp->backend) c_backend_free(comp->backend);
  if (comp->display) c_wl_display_free(comp->display);

  c_list_destroy(comp->clients);
  exit(err);
}

void spawn_client(bind_args *args) {
  if (fork() == 0) {
    close(STDIN_FILENO);
    open("/dev/null", O_RDWR);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    setsid();
    c_log(C_LOG_DEBUG, "%s", args->s);
    execvp("/bin/sh", (char *const []){"/bin/sh", "-c", args->s, NULL});
  }
}

void kill_client(bind_args *args) {
  if (comp->focused_client)
    client_close(comp, comp->focused_client);
}

void focus(bind_args *args) {
  if (comp->clients->size < 2 || !comp->focused_client) return;
  client_unfocus(comp->focused_client);

  int clients_size = comp->clients->size;
  int new_i = comp->focused_client_n + args->i;
  comp->focused_client_n = ((new_i % clients_size) + clients_size) % clients_size;
  comp->focused_client = c_list_get(comp->clients, comp->focused_client_n);
  client_focus(comp->focused_client, 0, 0);
}

void quit(bind_args *args) {
  cleanup(0, NULL);
}

void tile() {
  struct c_render *render = comp->render;
  c_list *clients = comp->clients;
  if (comp->n_stacked_windows == 0) return;

  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(render->drm);
  uint32_t mon_width =  preferred_mode->width;
  uint32_t mon_height = preferred_mode->height;

  mon_width -= (comp->n_stacked_windows + 1) * gap;
  uint32_t one_window_width = mon_width / comp->n_stacked_windows;

  int i = 0;
  struct client *client;
  c_list_for_each(clients, client) {
    struct c_window *window = client->window;
    if (!window) {
      c_log(C_LOG_ERROR, "%p client (conn %p) has no window", client, client->conn);
      cleanup(1, NULL);
    }
    if (window->state & C_WINDOW_FLOAT) continue;

    window->x = i++ * one_window_width;
    window->x += i * gap;
    window->y = gap;

    window->width = one_window_width;
    window->height = mon_height - gap * 2;
    c_window_resize(window, window->width, window->height, client == comp->focused_client);
  }
}

int main() {
  int ret = 0;

  struct cuts _comp = {0};
  comp = &_comp;

  c_signal_handler_add(SIGTERM, cleanup, &comp);
  c_signal_handler_add(SIGINT, cleanup, &comp);

  c_log_set_level(C_LOG_INFO | C_LOG_ERROR | C_LOG_DEBUG | C_LOG_WARNING | C_LOG_WAYLAND);

  struct c_wl_display *display = c_wl_display_init();
  if (!display) 
    goto out;

  struct c_wl_display_listener display_listener = {
    .on_client_new = on_client_new,
    .on_client_gone = on_client_gone,
  };
  c_wl_display_add_listener(display, &display_listener, &_comp);

  comp->display = display;
  comp->clients = c_list_new();
  comp->layouts = layouts;
  comp->curr_layout = 0;

  struct c_backend *backend = c_backend_init(display);
  if (!backend) goto out;

  if (c_input_init_xkb_state(backend->input, &xkb_rules) < 0) goto out;

  struct c_input_event_listener_mouse mouse_listener = {
    .on_mouse_movement = on_mouse_movement,
    .on_mouse_button = on_mouse_button,
    .on_mouse_scroll = on_mouse_scroll,
  };
  c_input_add_event_listener_mouse(backend->input, &mouse_listener, comp);

  struct c_input_event_listener_keyboard listener_keyboard = {
    .on_keyboard_key = on_keyboard_key,
  };
  c_input_add_event_listener_keyboard(backend->input, &listener_keyboard, comp);

  for(size_t i = 0; i < LENGTH(binds); i++) {
    struct bind *b = &binds[i];
    c_input_add_shortcut_handler(backend->input, b->modmask, b->keysym, (void (*)(void *))b->func, &b->args);
  }

  comp->backend = backend;

  struct c_render *render = c_render_init(display, backend->drm);
  if (!render) goto out;

  struct c_render_listener render_listener = {
    .on_window_new = on_window_new_cb,
    .on_window_close = on_window_close_cb,
  };
  c_render_add_listener(render, &render_listener, comp);

  comp->render = render;


  ret = c_event_loop_run(display->loop);

out:
  cleanup(ret, &comp);
}
