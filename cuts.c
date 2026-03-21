#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>

#include "wayland/display.h"
#include "wayland/server.h"
#include "wayland/error.h"
#include "wayland/types.h"

#include "wayland/types/wayland.h"

#include "backend/backend.h"
#include "backend/input/keycodes.h"

#include "render/render.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/signal.h"
#include "util/log.h"

struct cuts {
  struct c_wl_display *display;
  struct c_backend    *backend;
  struct c_render     *render;

  struct c_window *focused_window;

  c_list 	            *windows;
  c_wl_object_id wl_pointer_id;
  c_wl_object_id wl_keyboard_id;

};

static uint32_t epoch_millis() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (((uint32_t)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

static void window_focus(struct cuts *comp, struct c_window *window, double mx, double my) {
  c_wl_array arr = {0};
  wl_keyboard_enter(window->wl_surface->conn, comp->wl_keyboard_id, CLOCK, window->wl_surface->id, &arr);
  wl_pointer_enter(window->wl_surface->conn, comp->wl_pointer_id, CLOCK, window->wl_surface->id, mx, my);
  c_window_activate(window);
  comp->focused_window = window;
};

static void window_unfocus(struct cuts *comp) {
  struct c_window *window = comp->focused_window;
  wl_keyboard_leave(window->wl_surface->conn, comp->wl_keyboard_id, CLOCK, window->wl_surface->id);
  wl_pointer_leave(window->wl_surface->conn, comp->wl_pointer_id, CLOCK, window->wl_surface->id);
  c_window_deactivate(window);
  comp->focused_window = NULL;
};

static int on_window_new_cb(struct c_window *window, void *userdata) {
  struct cuts *comp = userdata;
  struct c_render *render = comp->render;
  uint32_t mon_width = render->drm->output->width;
  uint32_t mon_height = render->drm->output->height;
  uint32_t gap = 15;

  c_list_push(comp->windows, window, 0);
  c_log(C_LOG_DEBUG, "windows len: %d", c_list_len(comp->windows));

  mon_width -= (c_list_len(comp->windows) + 1) * gap;
  uint32_t one_window_width = mon_width / c_list_len(comp->windows);

  int i = 0;
  struct c_window *_w;
  c_list_for_each(comp->windows, _w) {
    _w->x = i++ * one_window_width;
    _w->x += i * gap;
    _w->y = gap;

    _w->width = one_window_width;
    _w->height = mon_height - gap * 2;
    c_window_resize(_w, _w->width, _w->height, 0);
  }

  return 0;
}

static int on_keyboard_key(struct c_input_keyboard_event *event, void *userdata) {
  c_log(C_LOG_DEBUG, "keyboard key=%s (%s)", keycode_str(event->key), event->pressed ? "pressed" : "released");
  struct cuts *comp = userdata;
  if (!comp->focused_window) return 0;

  struct c_wl_surface *surface = comp->focused_window->wl_surface;
  // wl_keyboard_key(struct c_wl_connection *conn, comp->wl_keyboard_id, CLOCK, epoch_millis, c_wl_uint key, enum wl_keyboard_key_state_enum state)
  return 0;
}

static int on_mouse_movement(struct c_input_mouse_event *event, void *userdata) {
  double mx = event->x;
  double my = event->y;
  uint32_t wx, wy, ww, wh;

  struct cuts *comp = userdata;
  struct c_window *w;

  c_list_for_each(comp->windows, w) {
    wx = w->x;
    wy = w->y;
    ww = w->width;
    wh = w->height;

    if (wx <= mx && mx <= wx + ww &&
        wy <= my && my <= wy + wh) {
      float hotspot_x = (float)(mx - wx);
      float hotspot_y = (float)(my - wy);

      if (w != comp->focused_window) {
        if (comp->focused_window)
          window_unfocus(comp);
        
        window_focus(comp, w, hotspot_x, hotspot_y);
      } else {
        struct c_wl_surface *surface = comp->focused_window->wl_surface;
        wl_pointer_motion(surface->conn, comp->wl_pointer_id, epoch_millis(), hotspot_x, hotspot_y);
      }

      break;
    }
  }

  return 0;
}

static void *on_wl_seat_bind(struct c_wl_connection *conn, c_wl_object_id new_id, void *userdata) {
  struct cuts *comp = userdata;

  wl_seat_name(conn, new_id, "seat0");
  wl_seat_capabilities(conn, new_id, comp->backend->input->capabilities);

  return comp;
}

static void *on_wl_shm_bind(struct c_wl_connection *conn, c_wl_object_id new_id, void *userdata) {
  c_list *supported_formats = c_list_new();
  c_list_push(supported_formats, (void *)WL_SHM_FORMAT_ARGB8888, 0);
  c_list_push(supported_formats, (void *)WL_SHM_FORMAT_XRGB8888, 0);
  
  int *fmt;
  c_list_for_each(supported_formats, fmt)
      wl_shm_format(conn, new_id, (uint64_t)fmt);
  
  return supported_formats;
}

int wl_seat_get_keyboard(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id wl_keyboard_id = args[1].o;
  struct c_wl_object *wl_keyboard;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_keyboard_id, wl_keyboard);

  struct cuts *comp = c_wl_object_get(conn, args[0].o)->data;
  struct c_input *input = comp->backend->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_KEYBOARD))
    return c_wl_error_set(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  comp->wl_keyboard_id = wl_keyboard_id;
  c_wl_object_add(conn, wl_keyboard_id, c_wl_interface_get("wl_keyboard"), NULL);

  return 0;
}

int wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id wl_pointer_id = args[1].o;
  struct c_wl_object *wl_pointer;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_pointer_id, wl_pointer);

  struct cuts *comp = c_wl_object_get(conn, args[0].o)->data;
  struct c_input *input = comp->backend->input;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_POINTER))
    return c_wl_error_set(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  comp->wl_pointer_id = wl_pointer_id;
  c_wl_object_add(conn, wl_pointer_id, c_wl_interface_get("wl_pointer"), NULL);

  return 0;
}

static void cleanup(int err, void *userdata) {
  struct cuts *comp = userdata;
  if (comp->render)  c_render_free(comp->render);
  if (comp->backend) c_backend_free(comp->backend);
  if (comp->display) c_wl_display_free(comp->display);

  c_list_destroy(comp->windows);
  exit(err);
}

int main() {
  int ret = 0;

  struct cuts comp = {0};
  c_signal_handler_add(SIGTERM, cleanup, &comp);
  c_signal_handler_add(SIGINT, cleanup, &comp);

  
  c_log_set_level(C_LOG_INFO | C_LOG_ERROR | C_LOG_DEBUG);

  struct c_wl_display *display = c_wl_display_init();
  if (!display) 
    goto out;

  comp.display = display;
  comp.windows = c_list_new();

  struct c_backend *backend = c_backend_init(display->loop);
  if (!backend) goto out;


  struct c_input_listener_mouse mouse_listener = {
    .on_mouse_movement = on_mouse_movement,
  };
  c_input_add_listener_mouse(backend->input, &mouse_listener, &comp);

  struct c_input_listener_kbd listener_kbd = {
    .on_kbd_key = on_keyboard_key,
  };
  c_input_add_listener_kbd(backend->input, &listener_kbd, NULL);

  comp.backend = backend;

  struct c_render *render = c_render_init(display, backend->drm);
  if (!render) goto out;

  struct c_render_listener render_listener = {
    .on_window_new = on_window_new_cb,
  };
  c_render_add_listener(render, &render_listener, &comp);

  comp.render = render;

  c_wl_display_add_supported_interface(display, "wl_compositor", NULL, NULL);
  c_wl_display_add_supported_interface(display, "wl_shm", on_wl_shm_bind, &comp);
  c_wl_display_add_supported_interface(display, "wl_seat", on_wl_seat_bind, &comp);
  c_wl_display_add_supported_interface(display, "xdg_wm_base", NULL, NULL);
  c_wl_display_add_supported_interface(display, "zwp_linux_dmabuf_v1", NULL, NULL);

  ret = c_event_loop_run(display->loop);

out:
  cleanup(ret, &comp);
}
