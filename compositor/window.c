#include <assert.h> 
#include <unistd.h>

#include "wayland/proto/xdg-shell.h"
#include "wayland/impl/xdg-shell.h"
#include "wayland/proto/wayland.h"
#include "wayland/impl/wayland.h"

#include "compositor/window.h"
#include "compositor/scene.h"
#include "util/log.h"
#include "util/helpers.h"

void get_surface_size(struct c_wl_surface *surface, double *width, double *height);

struct c_wl_surface *surface_hit_test(struct c_window *window,
                                      struct c_wl_surface *surface,
                                      double scale, 
                                      double px, double py,
                                      double offset_x, double offset_y,
                                      double *x, double *y) {

  if (!surface->buffer.active) return NULL;

  double surf_x = 0, surf_y = 0;
  double surf_w, surf_h;
  get_surface_size(surface, &surf_w, &surf_h);


  if (window) {
    px /= scale;
    py /= scale;
    offset_x /= scale;
    offset_y /= scale;
  }

  if (window && surface->xdg_surface) {
    double crop_w = window->width / scale;
    double crop_h = window->height / scale;
    surf_x = (surf_w - crop_w) / 2;
    surf_y = (surf_h - crop_h) / 2;
  }
  

  struct c_wl_surface *hit = NULL;

  if (surface->sub.children) {
    struct c_wl_subsurface *ss;
    struct c_wl_surface *h;
    c_list_for_each(surface->sub.children, ss) {
      h = surface_hit_test(NULL, ss->surface, scale, px, py,
                           offset_x + ss->x - surf_x,
                           offset_y + ss->y - surf_y,
                           x, y);
      if (h) hit = h;
    }
  }

  if (surface->xdg_surface && surface->xdg_surface->children) {
    struct c_xdg_surface *xs;
    struct c_wl_surface *h;
    c_list_for_each(surface->xdg_surface->children, xs) {
      h = surface_hit_test(NULL, xs->surface, scale, px, py, 
          offset_x + xs->popup.x - xs->x,
          offset_y + xs->popup.y - xs->y,
          x, y);
      if (h) hit = h;
    }
  }

  if (hit) return hit;


  if (CURSOR_INSIDE(px, py, offset_x, offset_y, surf_w, surf_h)) {
    double sx = px - offset_x + surf_x;
    double sy = py - offset_y + surf_y;

    struct c_wl_region *input = &surface->input.active;
    if (input &&
        !CURSOR_INSIDE(sx, sy, input->x, input->y, input->width, input->height))
      return NULL;

    *x = sx;
    *y = sy;

    return surface;
  }

  return NULL;
}

static void surface_enter(struct c_window *window, struct c_wl_surface *surface, double mx, double my, int enter_keyboard) {
  struct c_wl_connection *conn = window->conn;
  int wl_pointer_serial = c_wl_serial();
  int wl_keyboard_serial = c_wl_serial();
  int wl_keyboard_modifiers_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(conn, o) {
    SWITCH_STR(o->iface->name)
      CASE_STR("wl_keyboard")
        if (enter_keyboard) {
          c_wl_array arr = {0};
          wl_keyboard_enter(conn, o->id, wl_keyboard_serial, surface->obj->id, &arr);
          wl_keyboard_modifiers(conn, o->id, wl_keyboard_modifiers_serial, 0, 0, 0, 0);
        }

      CASE_STR("wl_pointer")
        c_wl_fixed hotspot_x = C_WL_FIXED_FROM_DOUBLE(mx);
        c_wl_fixed hotspot_y = C_WL_FIXED_FROM_DOUBLE(my);

        wl_pointer_enter(conn, o->id, wl_pointer_serial, surface->obj->id, hotspot_x, hotspot_y);
        wl_pointer_frame(conn, o->id);

    SWITCH_STR_END
    }
}

static void surface_leave(struct c_window *window, struct c_wl_surface *surface, int leave_keyboard) {
  struct c_wl_connection *conn = window->conn;
  int wl_keyboard_serial = c_wl_serial();
  int wl_pointer_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(conn, o) {
    SWITCH_STR(o->iface->name)
      CASE_STR("wl_keyboard")
        if (leave_keyboard)
          wl_keyboard_leave(conn, o->id, wl_keyboard_serial, surface->obj->id);
      CASE_STR("wl_pointer")
        wl_pointer_leave(conn, o->id, wl_pointer_serial, surface->obj->id);
        wl_pointer_frame(conn, o->id);
    SWITCH_STR_END
  }
}

struct c_wl_surface *c_window_surface_at(struct c_window *window, double x, double y, double *lx, double *ly) {
  struct c_wl_surface *surf = surface_hit_test(window,
      window->surface->surface, window->scale, x, y, window->x, window->y, lx, ly);

  if (!surf) {
    *lx = (x - window->x) / window->scale;
    *ly = (y - window->y) / window->scale;
    return window->surface->surface;
  }
  return surf;
}

struct c_window *c_window_new(struct c_scene *scene,
                              struct c_wl_connection *connection,
                              struct c_xdg_surface *surface) {
  struct c_window *window = calloc(1, sizeof(*window));
  if (!window) {
    c_log_errno(C_LOG_ERROR, "failed to allocate window for a new client");
    return NULL;
  }

  window->conn = connection;
  window->surface = surface;
  window->title = &surface->toplevel.title;
  window->app_id = &surface->toplevel.app_id;

  window->node = c_scene_add_window(scene, window);

  return window;
}

static void add_states(struct c_window *window, c_wl_enum state[16], size_t *size) {
  if (window->state & C_WINDOW_FULLSCREEN) {
    state[(*size)++] = XDG_TOPLEVEL_STATE_FULLSCREEN;
  }

  if (window->surface->obj->version >= C_XDG_TOPLEVEL_TILED_TOP_SINCE) {
    if (ENUM_FLAG(XDG_TOPLEVEL_STATE_TILED_TOP) & window->xdg_states)
      state[(*size)++] = XDG_TOPLEVEL_STATE_TILED_TOP;

    if (ENUM_FLAG(XDG_TOPLEVEL_STATE_TILED_RIGHT) & window->xdg_states)
      state[(*size)++] = XDG_TOPLEVEL_STATE_TILED_RIGHT;

    if (ENUM_FLAG(XDG_TOPLEVEL_STATE_TILED_BOTTOM) & window->xdg_states)
      state[(*size)++] = XDG_TOPLEVEL_STATE_TILED_BOTTOM;

    if (ENUM_FLAG(XDG_TOPLEVEL_STATE_TILED_LEFT) & window->xdg_states)
      state[(*size)++] = XDG_TOPLEVEL_STATE_TILED_LEFT;
  }

  if (window->surface->obj->version >= C_XDG_TOPLEVEL_CONSTRAINED_TOP_SINCE) {
    if (ENUM_FLAG(XDG_TOPLEVEL_STATE_CONSTRAINED_TOP) & window->xdg_states)
      state[(*size)++] = XDG_TOPLEVEL_STATE_CONSTRAINED_TOP;

    if (ENUM_FLAG(XDG_TOPLEVEL_STATE_CONSTRAINED_RIGHT) & window->xdg_states)
      state[(*size)++] = XDG_TOPLEVEL_STATE_CONSTRAINED_RIGHT;

    if (ENUM_FLAG(XDG_TOPLEVEL_STATE_CONSTRAINED_BOTTOM) & window->xdg_states)
      state[(*size)++] = XDG_TOPLEVEL_STATE_CONSTRAINED_BOTTOM;

    if (ENUM_FLAG(XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT) & window->xdg_states)
      state[(*size)++] = XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT;
  }
}

void c_window_free(struct c_scene *scene, struct c_window *window) {
  c_scene_node_remove(scene, window->node);
  free(window);
}

int c_window_deactivate(struct c_window *window) {
  struct c_xdg_surface *xdg_surface = window->surface;

  uint32_t width = window->width / window->scale;
  uint32_t height = window->height / window->scale;

  size_t state_size = 0;
  c_wl_enum state[16];
  add_states(window, state, &state_size);

  c_wl_array arr = {
    .size = state_size * sizeof(*state),
    .data = &state,
  };

  int serial = c_wl_serial();
  xdg_toplevel_configure(window->conn, xdg_surface->toplevel.obj->id, width, height, &arr);
  xdg_surface_configure(window->conn, xdg_surface->obj->id, serial);

  c_wl_connection_flush(window->conn);
  return serial;
}

int c_window_activate(struct c_window *window) {
  struct c_xdg_surface *xdg_surface = window->surface;

  uint32_t width = window->width / window->scale;
  uint32_t height = window->height / window->scale;
  
  size_t state_size = 1;
  c_wl_enum state[16] = {XDG_TOPLEVEL_STATE_ACTIVATED};
  add_states(window, state, &state_size);

  c_wl_array arr = {
    .size = state_size * sizeof(*state),
    .data = &state,
  };

  int serial = c_wl_serial();
  xdg_toplevel_configure(window->conn, xdg_surface->toplevel.obj->id, width, height, &arr);
  xdg_surface_configure(window->conn, xdg_surface->obj->id, serial);

  c_wl_connection_flush(window->conn);

  return serial;
};

void c_window_focus(struct c_window *window, double mx, double my) {
  double lx, ly;
  struct c_wl_surface *focused = c_window_surface_at(window, mx, my, &lx, &ly);

  surface_enter(window, focused, mx, my, 1);
  c_window_activate(window);
  c_wl_connection_flush(window->conn);
}

void c_window_unfocus(struct c_window *window) {
  struct c_wl_surface *surface = window->surface->surface;
  surface_leave(window, surface, 1);
  c_window_deactivate(window);
  c_wl_connection_flush(window->conn);
}

void c_window_close(struct c_window *window) {
  struct c_xdg_surface *surface = window->surface;
  xdg_toplevel_close(window->conn, surface->toplevel.obj->id);
  c_wl_connection_flush(window->conn);
}

void c_window_pointer_move(struct c_window *window, double x, double y) {
  double lx, ly;
  struct c_wl_surface *focused =
      surface_hit_test(window, window->surface->surface, window->scale, x, y, window->x, window->y, &lx, &ly);

  if (!focused) {
    lx = (x - window->x) / window->scale;
    ly = (y - window->y) / window->scale;
  } else if (focused != window->focused) {
    if (window->focused)
      surface_leave(window, window->focused, 0);

    surface_enter(window, focused, lx, ly, 0);
    window->focused = focused;
  }

  c_wl_fixed hotspot_x = C_WL_FIXED_FROM_DOUBLE(lx);
  c_wl_fixed hotspot_y = C_WL_FIXED_FROM_DOUBLE(ly);
  c_wl_object_id frames[16];
  size_t frame_n = 0;

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_motion(window->conn, o->id, c_since_start_ms(), hotspot_x, hotspot_y);
      frames[frame_n++] = o->id;
    }
  }

  for (size_t i = 0; i < frame_n; i++)
    wl_pointer_frame(window->conn, frames[i]);

  c_wl_connection_flush(window->conn);
}

void c_window_pointer_button(struct c_window *window, uint32_t button, int pressed) {
  int serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_button(window->conn, o->id, serial, c_since_start_ms(), button, pressed);
      wl_pointer_frame(window->conn, o->id);
    }
  }
  c_wl_connection_flush(window->conn);
}

void c_window_pointer_scroll(struct c_window *window, double axis, double axis120,
                             enum wl_pointer_axis_source_enum axis_source,
                             enum wl_pointer_axis_enum axis_orient,
                             int axis_discrete) {

  struct c_wl_connection *conn = window->conn;
  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_axis_source(conn, o->id, axis_source);

      if (C_WL_POINTER_AXIS_DISCRETE_SINCE <= o->version &&
          o->version < C_WL_POINTER_AXIS_DISCRETE_DEPRECATED_SINCE) {
        wl_pointer_axis_discrete(conn, o->id, axis_orient, axis_discrete);

      } else if (o->version >= C_WL_POINTER_AXIS_VALUE120_SINCE) {
        if (o->version >= C_WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE)
          wl_pointer_axis_relative_direction(conn, o->id, axis_orient, WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
        wl_pointer_axis_value120(conn, o->id, axis_orient, (c_wl_int)axis120);
      }

      wl_pointer_axis(conn, o->id, c_since_start_ms(), axis_orient, C_WL_FIXED_FROM_DOUBLE(axis));
      wl_pointer_frame(conn, o->id);
    }
  }
  c_wl_connection_flush(window->conn);
}

void c_window_keyboard_key(struct c_window *window, int32_t key, int pressed, 
		xkb_mod_mask_t mods_depressed, xkb_mod_mask_t mods_latched, xkb_mod_mask_t mods_locked, 
		xkb_layout_index_t group, int send_mods) {

  int serial = c_wl_serial();
  int serial2 = c_wl_serial();
  
  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    if (STREQ(o->iface->name, "wl_keyboard")) {
      wl_keyboard_key(window->conn, o->id, serial, c_since_start_ms(), 
                      key, pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);

      if (send_mods)
        wl_keyboard_modifiers(window->conn, o->id, serial2, mods_depressed, mods_latched, mods_locked, group);

    }
  }
  c_wl_connection_flush(window->conn);
}
