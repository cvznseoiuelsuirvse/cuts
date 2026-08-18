#include <assert.h> 
#include <unistd.h>

#include "wayland/proto/xdg-shell.h"
#include "wayland/proto/wayland.h"
#include "compositor/window.h"
#include "compositor/scene.h"
#include "util/log.h"
#include "util/helpers.h"

static void get_surface_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  if (surface->active->dma) {
    *width = surface->active->dma->width / surface->active->scale;
    *height = surface->active->dma->height / surface->active->scale;
  } else {
    *width = surface->active->shm->width / surface->active->scale;
    *height = surface->active->shm->height / surface->active->scale;
  }
}

static struct c_wl_surface *surface_hit_test(struct c_wl_surface *surface,
                                            double ox, double oy,
                                            double px, double py,
                                            double *lx, double *ly) {
  if (!surface->active) return NULL;

  int32_t buf_w, buf_h;
  get_surface_buf_size(surface, &buf_w, &buf_h);

  double w_x = 0, w_y = 0;
  uint32_t w_w = buf_w, w_h = buf_h;
  if (surface->xdg_surface && surface->xdg_surface->width > 0) {
   w_x = surface->xdg_surface->x;
   w_y = surface->xdg_surface->y;
   w_w = surface->xdg_surface->width;
   w_h = surface->xdg_surface->height;
  }

  struct c_wl_surface *hit = NULL;

  if (surface->sub.children) {
    struct c_wl_subsurface *s;
    c_list_for_each(surface->sub.children, s) {
      hit = surface_hit_test(s->surface, ox + s->x, oy + s->y, px, py, lx, ly);
    }
  }

  if (surface->xdg_surface && surface->xdg_surface->children) {
    struct c_xdg_surface *xs;
    c_list_for_each(surface->xdg_surface->children, xs) {
      hit = surface_hit_test(xs->surface, ox + xs->x + xs->popup.x,
                           oy + xs->y + xs->popup.y, px, py, lx, ly);
    }
  }

  if (hit) return hit;

  if (px >= ox && px < ox + w_w && py >= oy && py < oy + w_h) {
    double sx = px - ox + w_x;
    double sy = py - oy + w_y;

    int has_input = surface->input.width > 0 && surface->input.height > 0;

    if (!has_input ||
        !(sx >= surface->input.x && sx < surface->input.x + surface->input.width &&
          sy >= surface->input.y && sy < surface->input.y + surface->input.height))
      return NULL;

    *lx = sx;
    *ly = sy;
    return surface;
  }

  return NULL;
}


static void surface_pointer_focus(struct c_wl_connection *connection,
                                  struct c_wl_surface *surface, double mx, double my) {
  int wl_pointer_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(connection, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      c_wl_fixed hotspot_x = C_WL_FIXED_FROM_DOUBLE(mx);
      c_wl_fixed hotspot_y = C_WL_FIXED_FROM_DOUBLE(my);

      wl_pointer_enter(connection, o->id, wl_pointer_serial, surface->id, hotspot_x, hotspot_y);
      wl_pointer_frame(connection, o->id);
    }
  }
}

static void surface_pointer_unfocus(struct c_wl_connection *connection, struct c_wl_surface *surface) {
  int wl_pointer_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(connection, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_leave(connection, o->id, wl_pointer_serial, surface->id);
      wl_pointer_frame(connection, o->id);
    }
  }
}

static void surface_focus(struct c_wl_connection *connection,
                          struct c_wl_surface *surface, double mx, double my) {
  int wl_keyboard_serial = c_wl_serial();
  int wl_keyboard_modifiers_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(connection, o) {
    if (STREQ(o->iface->name, "wl_keyboard")) {
      c_wl_array arr = {0};
      wl_keyboard_enter(connection, o->id, wl_keyboard_serial, surface->id, &arr);
      wl_keyboard_modifiers(connection, o->id, wl_keyboard_modifiers_serial, 0, 0, 0, 0);
    }
  }

  surface_pointer_focus(connection, surface, mx, my);
}

static void surface_unfocus(struct c_wl_connection *connection, struct c_wl_surface *surface) {
  int wl_keyboard_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(connection, o) {
    if (STREQ(o->iface->name, "wl_keyboard")) {
      wl_keyboard_leave(connection, o->id, wl_keyboard_serial, surface->id);
    }
  }

  surface_pointer_unfocus(connection, surface);
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

void c_window_free(struct c_scene *scene, struct c_window *window) {
  c_scene_node_remove(scene, window->node);
  free(window);
}

void c_window_deactivate(struct c_window *window) {
  struct c_xdg_surface *xdg_surface = window->surface;

  uint32_t width = window->width;
  uint32_t height = window->height;

  int state_size = 0;
  c_wl_enum state[1] = {0};

  if (window->state & C_WINDOW_FULLSCREEN) {
    state[state_size++] = XDG_TOPLEVEL_STATE_FULLSCREEN;
  }

  c_wl_array arr = {
    .size = state_size * sizeof(c_wl_enum),
    .data = &state,
  };

  int serial = c_wl_serial();
  xdg_toplevel_configure(window->conn, xdg_surface->toplevel.id, width, height, &arr);
  xdg_surface_configure(window->conn, xdg_surface->id, serial);

  c_wl_connection_flush(window->conn);
}

void c_window_activate(struct c_window *window) {
  struct c_xdg_surface *xdg_surface = window->surface;
  
  int state_size = 1;
  c_wl_enum state[2] = {XDG_TOPLEVEL_STATE_ACTIVATED, 0};

  if (window->state & C_WINDOW_FULLSCREEN) {
    state[state_size++] = XDG_TOPLEVEL_STATE_FULLSCREEN;
  }

  c_wl_array arr = {
    .size = state_size * sizeof(*state),
    .data = &state,
  };

  uint32_t width = window->width;
  uint32_t height = window->height;

  int serial = c_wl_serial();
  xdg_toplevel_configure(window->conn, xdg_surface->toplevel.id, width, height, &arr);
  xdg_surface_configure(window->conn, xdg_surface->id, serial);

  c_wl_connection_flush(window->conn);
};

void c_window_focus(struct c_window *window, double mx, double my) {
  struct c_wl_surface *surface = window->surface->surface;
  surface_focus(window->conn, surface, mx, my);
  c_window_activate(window);
  c_wl_connection_flush(window->conn);
}

void c_window_unfocus(struct c_window *window) {
  struct c_wl_surface *surface = window->surface->surface;
  surface_unfocus(window->conn, surface);
  c_window_deactivate(window);
  c_wl_connection_flush(window->conn);
}

void c_window_close(struct c_window *window) {
  struct c_xdg_surface *surface = window->surface;
  xdg_toplevel_close(window->conn, surface->toplevel.id);
  c_wl_connection_flush(window->conn);
}

void c_window_pointer_move(struct c_window *window, double x, double y) {
  double lx, ly;
  struct c_wl_surface *focused = surface_hit_test(window->surface->surface, window->x, window->y, x, y, &lx, &ly);

  if (!focused) {
    lx = x - window->x;
    ly = y - window->y;
  }

  if (focused != window->focused) {
    if (window->focused)
      surface_pointer_unfocus(window->conn, window->focused);

    surface_pointer_focus(window->conn, focused, lx, ly);
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
