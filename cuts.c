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

struct cuts {
  struct c_wl_display *display;
  struct c_backend    *backend;
  struct c_render     *render;
  struct c_output     *output;

  c_list 	        *windows;
  struct c_window *focused_window;
  int              focused_window_i;

  size_t        curr_layout;
  struct layout *layouts;
};

struct cuts *comp;

#define LAYOUT(comp) (comp)->layouts[(comp)->curr_layout].func()

struct c_window *get_window_from_connection(struct c_wl_connection *conn) {
  struct c_window *w;
  c_list_for_each(comp->windows, w) {
    if (w->conn == conn) return w;
  }
  return NULL;
}

size_t count_tiled_windows() {
  size_t i = 0;
  struct c_window *w;
  c_list_for_each(comp->windows, w) {
    if (w->state & C_WINDOW_FLOAT) continue;
    i++;
  }
  return i;
}

void window_focus(struct c_window *window, double mx, double my) {
  c_window_focus(window, mx, my);
  comp->focused_window = window;
};

void window_unfocus(struct c_window *window) {
  c_window_unfocus(window);
  comp->focused_window = NULL;
};

void window_close(struct c_window *window) {
  if (comp->focused_window == window) {
    window_unfocus(window);
    comp->focused_window = NULL;
  }

  c_list_remove_ptr(&comp->windows, window);
  LAYOUT(comp);
}

int on_client_gone(struct c_wl_connection *conn, void *userdata) {
  struct c_window *window = get_window_from_connection(conn);
  if (!window) return 0;

  window_close(window);
  if (comp->windows->size > 0) 
    window_focus(c_list_get(comp->windows, comp->windows->size-1), 0, 0);

  return 0;
}

int on_window_new_cb(struct c_window *window, void *userdata) {
  c_list_push(comp->windows, window, 0);

  if (comp->focused_window)
    window_unfocus(comp->focused_window);

  window_focus(window, 0, 0);
  LAYOUT(comp);
  return 0;
}

int on_keyboard_key(struct c_input_keyboard_event *event, void *userdata) {
  struct cuts *comp = userdata;
  if (!comp->focused_window) return 0;

  struct c_window *window = comp->focused_window;

  c_window_keyboard_key(window, event->key, event->pressed, 
      event->mods_depressed, event->mods_latched, event->mods_locked, event->group, event->changed);

  return 0;
}

int on_mouse_scroll(struct c_input_mouse_event *event, void *userdata) {
  if (!comp->focused_window) return 0;
  struct c_window *window = comp->focused_window;
  c_window_pointer_scroll(window, event->axis, event->axis_source - 1, event->axis_discrete);
  return 0;
}

int on_mouse_button(struct c_input_mouse_event *event, void *userdata) {
  if (!comp->focused_window) return 0;
  struct c_window *window = comp->focused_window;
  c_window_pointer_button(window, event->button, event->button_pressed);
  return 0;
}

int on_mouse_movement(struct c_input_mouse_event *event, void *userdata) {
  double mx = event->x;
  double my = event->y;
  uint32_t wx, wy;

  int i = 0;
  struct c_window *focused_window = comp->focused_window;
  struct c_window *window;
  c_list_for_each(comp->windows, window) {
    wx = window->x;
    wy = window->y;

    if (POINTER_INSIDE(mx, my, window)) {
      double hotspot_x = mx - wx;
      double hotspot_y = my - wy;

      if (focused_window == window) {
        c_window_pointer_move(window, hotspot_x, hotspot_y);
        return 0;
      }

      if (focused_window)
        window_unfocus(focused_window);

      window_focus(window, hotspot_x, hotspot_y);
      comp->focused_window_i = i;
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

  struct c_input *input = comp->backend->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_KEYBOARD))
    return c_wl_error_set(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  c_wl_object_add(conn, wl_keyboard_id, c_wl_interface_get("wl_keyboard"), NULL);

  int keymap_fd;
  int keymap_len = c_input_get_xkb_keymap_fd(comp->backend->input, &keymap_fd);
  if (keymap_len < 0)
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed ot get xkb_keymap");

  wl_keyboard_keymap(conn, wl_keyboard_id, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymap_fd, keymap_len);
  wl_keyboard_repeat_info(conn, wl_keyboard_id, keyboard_repeat_rate, keyboard_repeat_delay);

  // close(keymap_fd);
  return 0;
}

int wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_pointer_id = args[1].o;
  struct c_wl_object *wl_pointer;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_pointer_id, wl_pointer);

  struct c_input *input = comp->backend->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_POINTER))
    return c_wl_error_set(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  c_wl_object_add(conn, wl_pointer_id, c_wl_interface_get("wl_pointer"), NULL);

  return 0;
}

void cleanup(int err, void *userdata) {
  if (comp->render)  c_render_free(comp->render);
  if (comp->backend) c_backend_free(comp->backend);

  c_list_destroy(comp->windows);

  if (comp->display) c_wl_display_free(comp->display);

  exit(err);
}

void sh_cmd(bind_args *args) {
  if (fork() == 0) {
    close(STDIN_FILENO);
    open("/dev/null", O_RDWR);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    setsid();
    c_log(C_LOG_DEBUG, "%s", args->s);
    execvp("/bin/sh", (char *const []){"/bin/sh", "-c", args->s, NULL});
  }
}

void kill_window(bind_args *args) {
  if (comp->focused_window) {
    struct c_window *window = comp->focused_window;

    window_close(window);
    c_list_remove_ptr(&comp->windows, window);
    if (comp->windows->size > 0)
      window_focus(c_list_get(comp->windows, comp->windows->size-1), 0, 0);
    
    
    LAYOUT(comp);
  }
}

void focus(bind_args *args) {
  if (comp->windows->size < 2 || !comp->focused_window) {
    c_log(C_LOG_WARNING, "no focused window");
    return;
  }
  window_unfocus(comp->focused_window);

  int windows_n = comp->windows->size;
  int new_i = comp->focused_window_i + args->i;

  comp->focused_window_i = ((new_i % windows_n) + windows_n) % windows_n;
  comp->focused_window = c_list_get(comp->windows, comp->focused_window_i);
  window_focus(comp->focused_window, 0, 0);
}

void quit(bind_args *args) {
  cleanup(0, NULL);
}

void tile() {
  struct c_render *render = comp->render;
  c_list *windows = comp->windows;
  size_t n_tiled_windows = count_tiled_windows();
  if (n_tiled_windows == 0) return;

  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(render->drm);
  uint32_t mon_width =  preferred_mode->width;
  uint32_t mon_height = preferred_mode->height;

  mon_width -= (n_tiled_windows + 1) * gap;
  uint32_t one_window_width = mon_width / n_tiled_windows;

  int i = 0;
  struct c_window *window;
  c_list_for_each(windows, window) {
    if (window->state & C_WINDOW_FLOAT) continue;
    c_log(C_LOG_DEBUG, "%d %p", n_tiled_windows, window);

    window->x = i++ * one_window_width;
    window->x += i * gap;
    window->y = gap;

    window->width = one_window_width;
    window->height = mon_height - gap * 2;
    c_window_resize(window, window->width, window->height, window == comp->focused_window);

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

  comp->display = display;
  comp->windows = c_list_new();
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

  struct c_wl_display_listener display_listener = {
    .on_client_gone = on_client_gone,
  };
  c_wl_display_add_listener(display, &display_listener, &_comp);

  struct c_render *render = c_render_init(display, backend->drm);
  if (!render) goto out;

  struct c_render_listener render_listener = {
    .on_window_new = on_window_new_cb,
  };
  c_render_add_listener(render, &render_listener, comp);

  comp->render = render;

  ret = c_event_loop_run(display->loop);

out:
  cleanup(ret, &comp);
}
