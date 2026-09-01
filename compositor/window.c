#include <assert.h> 
#include <unistd.h>
#include <stdlib.h>

#include "wayland/proto/xdg-shell.h"
#include "wayland/impl/xdg-shell.h"
#include "wayland/impl/wayland.h"

#include "compositor/window.h"
#include "compositor/surface.h"
#include "compositor/util.h"

#include "util/log.h"
#include "util/helpers.h"

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
    double crop_w = surface->xdg_surface->width;
    double crop_h = surface->xdg_surface->height;
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

struct c_window *c_window_new(struct c_wl_connection *connection, struct c_xdg_surface *surface) {
  struct c_window *window = calloc(1, sizeof(*window));
  if (!window) {
    c_log_errno(C_LOG_ERROR, "failed to allocate window for a new client");
    return NULL;
  }

  window->conn = connection;
  window->surface = surface;
  window->title = &surface->toplevel.title;
  window->app_id = &surface->toplevel.app_id;
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

void c_window_free(struct c_window *window) {
  free(window);
}

void c_window_deactivate(struct c_window *window) {
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
  xdg_surface->serial = serial;

  xdg_toplevel_configure(window->conn, xdg_surface->toplevel.obj->id, width, height, &arr);
  xdg_surface_configure(window->conn, xdg_surface->obj->id, serial);

  c_wl_connection_flush(window->conn);
}

void c_window_activate(struct c_window *window) {
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
  xdg_surface->serial = serial;

  xdg_toplevel_configure(window->conn, xdg_surface->toplevel.obj->id, width, height, &arr);
  xdg_surface_configure(window->conn, xdg_surface->obj->id, serial);

  c_wl_connection_flush(window->conn);
};

void c_window_focus(struct c_window *window, double mx, double my) {
  double lx, ly;
  struct c_wl_surface *focused = c_window_surface_at(window, mx, my, &lx, &ly);

  c_surface_enter(focused, mx, my);
  c_window_activate(window);
  c_wl_connection_flush(window->conn);
}

void c_window_unfocus(struct c_window *window) {
  struct c_wl_surface *surface = window->surface->surface;

  c_surface_leave(surface);
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
      c_surface_leave_pointer(window->focused);

    c_surface_enter_pointer(focused, lx, ly);
    window->focused = focused;
  }

  c_surface_pointer_move(window->conn, lx, ly);
}
