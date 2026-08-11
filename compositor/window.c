#include <assert.h> 

#include "wayland/proto/xdg-shell.h"
#include "wayland/proto/wayland.h"
#include "compositor/window.h"
#include "util/log.h"
#include "util/helpers.h"

#define window_conn(window) (window)->conn

void c_window_deactivate(struct c_window *window) {
  struct c_xdg_surface *xdg_surface = window->surface;

  uint32_t width = window->width;
  uint32_t height = window->height;

  c_wl_array arr = {0};
  xdg_toplevel_configure(window_conn(window), xdg_surface->toplevel.id, width, height, &arr);
  xdg_surface_configure(window_conn(window), xdg_surface->id, c_wl_serial());

}

void c_window_activate(struct c_window *window) {
  struct c_xdg_surface *xdg_surface = window->surface;
  
  c_wl_enum state = XDG_TOPLEVEL_STATE_ACTIVATED;
  c_wl_array arr = {
    .size = sizeof(state),
    .data = &state,
  };

  uint32_t width = window->width;
  uint32_t height = window->height;

  xdg_toplevel_configure(window_conn(window), xdg_surface->toplevel.id, width, height, &arr);
  xdg_surface_configure(window_conn(window), xdg_surface->id, c_wl_serial());
  
};

void c_window_focus(struct c_window *window, double mx, double my) {
  struct c_wl_surface *surface = window->surface->surface;

  int wl_keyboard_serial = c_wl_serial();
  int wl_keyboard_modifiers_serial = c_wl_serial();
  int wl_pointer_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window_conn(window), o) {
    SWITCH_STR(o->iface->name)
      CASE_STR("wl_keyboard")
        c_wl_array arr = {0};
        wl_keyboard_enter(window_conn(window), o->id, wl_keyboard_serial, surface->id, &arr);
        wl_keyboard_modifiers(window_conn(window), o->id, wl_keyboard_modifiers_serial, 0, 0, 0, 0);

      CASE_STR("wl_pointer")
        c_wl_fixed hotspot_x = C_WL_FIXED_FROM_DOUBLE(mx);
        c_wl_fixed hotspot_y = C_WL_FIXED_FROM_DOUBLE(my);

        wl_pointer_enter(window_conn(window), o->id, wl_pointer_serial, surface->id, hotspot_x, hotspot_y);
        wl_pointer_frame(window_conn(window), o->id);

      SWITCH_STR_END;
  }

  c_window_activate(window);
}

void c_window_unfocus(struct c_window *window) {
  struct c_wl_surface *surface = window->surface->surface;

  int wl_keyboard_serial = c_wl_serial();
  int wl_pointer_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window_conn(window), o) {
    SWITCH_STR(o->iface->name)
      CASE_STR("wl_keyboard")
        wl_keyboard_leave(window_conn(window), o->id, wl_keyboard_serial, surface->id);
     
      CASE_STR("wl_pointer")
        wl_pointer_leave(window_conn(window), o->id, wl_pointer_serial, surface->id);
        wl_pointer_frame(window_conn(window), o->id);

    SWITCH_STR_END;

  }

  c_window_deactivate(window);
  
}

void c_window_close(struct c_window *window) {
  struct c_xdg_surface *surface = window->surface;
  xdg_toplevel_close(window_conn(window), surface->toplevel.id);
}

void c_window_pointer_move(struct c_window *window, double x, double y) {
  x -= window->x;
  y -= window->y;

  if (window->surface && (window->surface->x > 0 || window->surface->y > 0)) {
    x += window->surface->x;
    y += window->surface->y;
  }

  c_wl_fixed hotspot_x = C_WL_FIXED_FROM_DOUBLE(x);
  c_wl_fixed hotspot_y = C_WL_FIXED_FROM_DOUBLE(y);

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window_conn(window), o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_motion(window_conn(window), o->id, c_since_start_ms(), hotspot_x, hotspot_y);
      wl_pointer_frame(window_conn(window), o->id);
    }
  }
}

void c_window_pointer_button(struct c_window *window, uint32_t button, int pressed) {
  int serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window_conn(window), o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_button(window_conn(window), o->id, serial, c_since_start_ms(), button, pressed);
      wl_pointer_frame(window_conn(window), o->id);
    }
  }
}

void c_window_pointer_scroll(struct c_window *window, double axis, double axis120,
                             enum wl_pointer_axis_source_enum axis_source,
                             int axis_discrete) {
  struct c_wl_connection *conn = window_conn(window);

  if (!axis) {
    c_log(C_LOG_WARNING, "axis value is 0");
    return;
  }
  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window_conn(window), o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_axis_source(conn, o->id, axis_source);

      if (o->version < 8) {
        wl_pointer_axis_discrete(conn, o->id, WL_POINTER_AXIS_VERTICAL_SCROLL, axis_discrete);
      } else if (o->version >= 9) {
        wl_pointer_axis_relative_direction(
            conn, o->id, WL_POINTER_AXIS_VERTICAL_SCROLL,
            WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
        wl_pointer_axis_value120(conn, o->id, WL_POINTER_AXIS_VERTICAL_SCROLL, (c_wl_int)axis120);
      }

      wl_pointer_axis(conn, o->id, c_since_start_ms(), WL_POINTER_AXIS_VERTICAL_SCROLL, C_WL_FIXED_FROM_DOUBLE(axis));

      wl_pointer_frame(conn, o->id);
    }
  }
}

void c_window_keyboard_key(struct c_window *window, int32_t key, int pressed, 
		xkb_mod_mask_t mods_depressed, xkb_mod_mask_t mods_latched, xkb_mod_mask_t mods_locked, 
		xkb_layout_index_t group, int send_mods) {

  int serial = c_wl_serial();
  int serial2 = c_wl_serial();
  
  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window_conn(window), o) {
    if (STREQ(o->iface->name, "wl_keyboard")) {
      wl_keyboard_key(window_conn(window), o->id, serial, c_since_start_ms(), 
                      key, pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);

      if (send_mods)
        wl_keyboard_modifiers(window_conn(window), o->id, serial2, mods_depressed, mods_latched, mods_locked, group);

    }
  }
}
