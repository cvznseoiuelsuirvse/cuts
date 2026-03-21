#include "wayland/types/xdg-shell.h"

#include "util/helpers.h"
#include "render/window.h"

void c_window_resize(struct c_window *window, uint32_t width, uint32_t height, int activate) {
  struct c_wl_surface *surface = window->wl_surface;
  c_wl_enum states[2] = {XDG_TOPLEVEL_STATE_RESIZING};

  c_wl_array arr = {
    .size = sizeof(*states),
  };

  if (activate) {
    states[1] = XDG_TOPLEVEL_STATE_ACTIVATED;
    arr.size += sizeof(*states);
  }

  arr.data = states;

  xdg_toplevel_configure(surface->conn, surface->xdg_state.toplevel_id, width, height, &arr);
  
  surface->xdg_state.serial = CLOCK;
  xdg_surface_configure(surface->conn, surface->xdg_state.surface_id, surface->xdg_state.serial);
};


void c_window_move(struct c_window *window) {};

void c_window_hide(struct c_window *window) {};

void c_window_deactivate(struct c_window *window) {
  struct c_wl_surface *surface = window->wl_surface;
  c_wl_array arr = {0};
  xdg_toplevel_configure(surface->conn, surface->xdg_state.toplevel_id, window->width, window->height, &arr);
  
  surface->xdg_state.serial = CLOCK;
  xdg_surface_configure(surface->conn, surface->xdg_state.surface_id, surface->xdg_state.serial);

}

void c_window_activate(struct c_window *window) {
  struct c_wl_surface *surface = window->wl_surface;
  c_wl_enum state = XDG_TOPLEVEL_STATE_ACTIVATED;
  c_wl_array arr = {
    .size = sizeof(state),
    .data = &state,
  };
  xdg_toplevel_configure(surface->conn, surface->xdg_state.toplevel_id, window->width, window->height, &arr);
  
  surface->xdg_state.serial = CLOCK;
  xdg_surface_configure(surface->conn, surface->xdg_state.surface_id, surface->xdg_state.serial);
};
