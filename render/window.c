#include <assert.h> 

#include "wayland/types/xdg-shell.h"
#include "wayland/types/wayland.h"
#include "backend/input.h"
#include "render/window.h"
#include "util/log.h"
#include "util/helpers.h"

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

  xdg_toplevel_configure(surface->conn, surface->xdg.toplevel_id, width, height, &arr);
  
  surface->xdg.serial = c_wl_serial();
  xdg_surface_configure(surface->conn, surface->xdg.surface_id, surface->xdg.serial);
};


void c_window_move(struct c_window *window) {};

void c_window_hide(struct c_window *window) {};

void c_window_deactivate(struct c_window *window) {
  struct c_wl_surface *surface = window->wl_surface;
  c_wl_array arr = {0};
  xdg_toplevel_configure(surface->conn, surface->xdg.toplevel_id, window->width, window->height, &arr);
  
  surface->xdg.serial = c_wl_serial();
  xdg_surface_configure(surface->conn, surface->xdg.surface_id, surface->xdg.serial);

}

void c_window_activate(struct c_window *window) {
  struct c_wl_surface *surface = window->wl_surface;
  c_wl_enum state = XDG_TOPLEVEL_STATE_ACTIVATED;
  c_wl_array arr = {
    .size = sizeof(state),
    .data = &state,
  };
  xdg_toplevel_configure(surface->conn, surface->xdg.toplevel_id, window->width, window->height, &arr);
  
  surface->xdg.serial = c_wl_serial();
  xdg_surface_configure(surface->conn, surface->xdg.surface_id, surface->xdg.serial);
};

void c_window_focus(struct c_window *window, c_wl_object_id wl_keyboard, c_wl_object_id wl_pointer) {
  struct c_wl_surface *surface = window->wl_surface;
  c_wl_array arr = {0};
  if (wl_keyboard)
    wl_keyboard_enter(window->wl_surface->conn, wl_keyboard, c_wl_serial(), window->wl_surface->id, &arr);

  if (wl_pointer) {
    struct c_wl_region input_reg = surface->input;
    double mx = MAX(0, MIN((uint32_t)input_reg.x + input_reg.width + 1, window->width));
    double my = MAX(0, MIN((uint32_t)input_reg.y + input_reg.height + 1, window->height));

    c_wl_fixed hotspot_x = c_wl_fixed_from_double(mx);
    c_wl_fixed hotspot_y = c_wl_fixed_from_double(my);

    wl_pointer_enter(surface->conn, wl_pointer, c_wl_serial(), surface->id, hotspot_x, hotspot_y);
    wl_pointer_frame(surface->conn, wl_pointer);
  }

  // if (surface->sub.children)
  //   c_list_for_each(window->wl_surface->sub.children, surface) {
  //     c_wl_array arr = {0};
  //     if (wl_keyboard)
  //       wl_keyboard_enter(window->wl_surface->conn, wl_keyboard, c_wl_serial(), window->wl_surface->id, &arr);
  //
  //     if (wl_pointer) {
  //       struct c_wl_region input_reg = surface->input;
  //       double mx = MAX(0, MIN((uint32_t)input_reg.x + input_reg.width + 1, window->width));
  //       double my = MAX(0, MIN((uint32_t)input_reg.y + input_reg.height + 1, window->height));
  //
  //       c_wl_fixed hotspot_x = c_wl_fixed_from_double(mx);
  //       c_wl_fixed hotspot_y = c_wl_fixed_from_double(my);
  //
  //       wl_pointer_enter(surface->conn, wl_pointer, c_wl_serial(), surface->id, hotspot_x, hotspot_y);
  //       wl_pointer_frame(surface->conn, wl_pointer);
  //     }
  // }

  if (window->wl_surface->role == C_WL_SURFACE_ROLE_XDG_TOPLEVEL)
    c_window_activate(window);
}

void c_window_unfocus(struct c_window *window, c_wl_object_id wl_keyboard, c_wl_object_id wl_pointer) {
  struct c_wl_surface *surface = window->wl_surface;
  if (wl_keyboard)
    wl_keyboard_leave(surface->conn, wl_keyboard, c_wl_serial(), surface->id);
  if (wl_pointer) {
    wl_pointer_leave(surface->conn, wl_pointer, c_wl_serial(), surface->id);
    wl_pointer_frame(surface->conn, wl_pointer);
  }

  // if (surface->sub.children)
  //   c_list_for_each(window->wl_surface->sub.children, surface){
  //     if (wl_keyboard)
  //       wl_keyboard_leave(surface->conn, wl_keyboard, c_wl_serial(), surface->id);
  //     if (wl_pointer) {
  //       wl_pointer_leave(surface->conn, wl_pointer, c_wl_serial(), surface->id);
  //       wl_pointer_frame(surface->conn, wl_pointer);
  //     }
  // }

  if (window->wl_surface->role == C_WL_SURFACE_ROLE_XDG_TOPLEVEL)
    c_window_deactivate(window);
}

void c_window_close(struct c_window *window) {
  struct c_wl_surface *surface = window->wl_surface;
  xdg_toplevel_close(surface->conn, surface->xdg.toplevel_id);
}

void c_window_pointer_move(struct c_window *window, c_wl_object_id wl_pointer, double x, double y) {
  if (!wl_pointer) return;

  c_wl_fixed hotspot_x = c_wl_fixed_from_double(x);
  c_wl_fixed hotspot_y = c_wl_fixed_from_double(y);

  struct c_wl_surface *surface = window->wl_surface;
  if (POINTER_INSIDE(x, y, &surface->input)) {
    wl_pointer_motion(window->wl_surface->conn, wl_pointer, c_since_start_ms(), hotspot_x, hotspot_y);
    wl_pointer_frame(window->wl_surface->conn, wl_pointer);
  }
}

void c_window_pointer_button(struct c_window *window, 
                             c_wl_object_id wl_pointer, enum c_input_mouse_buttons button, int pressed) {
  if (!wl_pointer) return;
  wl_pointer_button(window->wl_surface->conn, wl_pointer, c_wl_serial(), c_since_start_ms(), button, pressed);
  wl_pointer_frame(window->wl_surface->conn, wl_pointer);
}

void c_window_pointer_scroll(struct c_window *window, c_wl_object_id wl_pointer, double axis,
                           enum wl_pointer_axis_source_enum axis_source, int axis_discrete) {
  struct c_wl_connection *conn = window->wl_surface->conn;

  if (!wl_pointer) return;
  if (!axis) {
    c_log(C_LOG_WARNING, "axis value is 0");
    return;
  }

  wl_pointer_axis_source(conn, wl_pointer, axis_source);
  wl_pointer_axis_discrete(conn, wl_pointer, WL_POINTER_AXIS_VERTICAL_SCROLL, axis_discrete);
  // wl_pointer_axis_relative_direction(conn, wl_pointer, WL_POINTER_AXIS_VERTICAL_SCROLL, WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
  wl_pointer_axis(conn, wl_pointer, c_since_start_ms(), WL_POINTER_AXIS_VERTICAL_SCROLL, c_wl_fixed_from_double(axis));
  wl_pointer_frame(conn, wl_pointer);

}
