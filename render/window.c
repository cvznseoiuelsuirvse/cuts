#include <assert.h> 

#include "wayland/types/xdg-shell.h"
#include "wayland/types/wayland.h"
#include "backend/input.h"
#include "render/window.h"
#include "util/log.h"
#include "util/helpers.h"

#define get_main_surface(window) c_list_get(window->surfaces, 0)

static struct c_wl_surface *get_xdg_toplevel(struct c_window *window) {
  struct c_wl_surface *s;

  c_list_for_each(window->surfaces, s) {
    if (s->role == C_WL_SURFACE_ROLE_XDG_TOPLEVEL) return s;
  }

  return NULL;
}

void c_window_resize(struct c_window *window, uint32_t width, uint32_t height, int activate) {
  struct c_wl_surface *toplevel = get_xdg_toplevel(window);

  c_wl_enum states[2] = {XDG_TOPLEVEL_STATE_RESIZING};

  c_wl_array arr = {
    .size = sizeof(*states),
  };

  if (activate) {
    states[1] = XDG_TOPLEVEL_STATE_ACTIVATED;
    arr.size += sizeof(*states);
  }

  arr.data = states;

  xdg_toplevel_configure(window->conn, toplevel->xdg.toplevel_id, width, height, &arr);
  
  toplevel->xdg.serial = c_wl_serial();
  xdg_surface_configure(window->conn, toplevel->xdg.surface_id, toplevel->xdg.serial);
  
};


void c_window_move(struct c_window *window) {};

void c_window_hide(struct c_window *window) {};

void c_window_deactivate(struct c_window *window) {
  struct c_wl_surface *toplevel = get_xdg_toplevel(window);

  c_wl_array arr = {0};
  xdg_toplevel_configure(window->conn, toplevel->xdg.toplevel_id, window->width, window->height, &arr);
  
  toplevel->xdg.serial = c_wl_serial();
  xdg_surface_configure(window->conn, toplevel->xdg.surface_id, toplevel->xdg.serial);

}

void c_window_activate(struct c_window *window) {
  struct c_wl_surface *toplevel = get_xdg_toplevel(window);
  
  c_wl_enum state = XDG_TOPLEVEL_STATE_ACTIVATED;
  c_wl_array arr = {
    .size = sizeof(state),
    .data = &state,
  };
  xdg_toplevel_configure(window->conn, toplevel->xdg.toplevel_id, window->width, window->height, &arr);
  
  toplevel->xdg.serial = c_wl_serial();
  xdg_surface_configure(window->conn, toplevel->xdg.surface_id, toplevel->xdg.serial);
  
};

void c_window_focus(struct c_window *window, double mx, double my) {
  struct c_wl_surface *surface = get_main_surface(window);

  int wl_keyboard_serial;
  int wl_keyboard_modifiers_serial;
  int wl_pointer_serial;

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    SWITCH_STR(o->iface->name);
      CASE_STR("wl_keyboard") {
        c_wl_array arr = {0};
        wl_keyboard_enter(window->conn, o->id,
                          SET_AND_GET(wl_keyboard_serial, c_wl_serial()),
                          surface->id, &arr);
        wl_keyboard_modifiers(
            window->conn, o->id,
            SET_AND_GET(wl_keyboard_modifiers_serial, c_wl_serial()), 0, 0, 0,
            0);
        SWITCH_STR_BREAK;
      } 

      CASE_STR("wl_pointer") {
        c_wl_fixed hotspot_x = c_wl_fixed_from_double(mx);
        c_wl_fixed hotspot_y = c_wl_fixed_from_double(my);

        wl_pointer_enter(window->conn, o->id,
                         SET_AND_GET(wl_pointer_serial, c_wl_serial()),
                         surface->id, hotspot_x, hotspot_y);
        wl_pointer_frame(window->conn, o->id);
        SWITCH_STR_BREAK;
      } 

      CASE_STR("wl_data_device") {
        wl_data_device_selection(window->conn, o->id, 0);
        SWITCH_STR_BREAK;
      }

      SWITCH_STR_END;
  }

  c_window_activate(window);
}

void c_window_unfocus(struct c_window *window) {
  struct c_wl_surface *surface = get_main_surface(window);

  int wl_keyboard_serial;
  int wl_pointer_serial;

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    SWITCH_STR(o->iface->name);
      CASE_STR("wl_keyboard") {
        wl_keyboard_leave(window->conn, o->id,
                          SET_AND_GET(wl_keyboard_serial, c_wl_serial()),
                          surface->id);
        SWITCH_STR_BREAK;
      } 
      CASE_STR("wl_pointer") {
        wl_pointer_leave(window->conn, o->id,
                         SET_AND_GET(wl_pointer_serial, c_wl_serial()),
                         surface->id);
        wl_pointer_frame(window->conn, o->id);
        SWITCH_STR_BREAK;
      }

    SWITCH_STR_END;

  }

  c_window_deactivate(window);
  
}

void c_window_close(struct c_window *window) {
  struct c_wl_surface *toplevel = get_xdg_toplevel(window);

  xdg_toplevel_close(window->conn, toplevel->xdg.toplevel_id);
}

void c_window_pointer_move(struct c_window *window, double x, double y) {
  struct c_wl_surface *toplevel = get_xdg_toplevel(window);

  c_wl_fixed hotspot_x = c_wl_fixed_from_double(x + toplevel->xdg.x);
  c_wl_fixed hotspot_y = c_wl_fixed_from_double(y + toplevel->xdg.y);

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_motion(window->conn, o->id, c_since_start_ms(), hotspot_x, hotspot_y);
      wl_pointer_frame(window->conn, o->id);
    }
  }
}

void c_window_pointer_button(struct c_window *window, enum c_input_mouse_buttons button, int pressed) {
  int serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_button(window->conn, o->id, serial, c_since_start_ms(), button, pressed);
      wl_pointer_frame(window->conn, o->id);
    }
  }
}

void c_window_pointer_scroll(struct c_window *window, double axis,
                           enum wl_pointer_axis_source_enum axis_source, int axis_discrete) {
  struct c_wl_connection *conn = window->conn;

  if (!axis) {
    c_log(C_LOG_WARNING, "axis value is 0");
    return;
  }
  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(window->conn, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_axis_source(conn, o->id, axis_source);
      wl_pointer_axis_discrete(conn, o->id, WL_POINTER_AXIS_VERTICAL_SCROLL, axis_discrete);
      // wl_pointer_axis_relative_direction(conn, o->id, WL_POINTER_AXIS_VERTICAL_SCROLL, WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
      wl_pointer_axis(conn, o->id, c_since_start_ms(), WL_POINTER_AXIS_VERTICAL_SCROLL, c_wl_fixed_from_double(axis));
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
  c_wl_objects_for_each(window->conn, o) {
    if (STREQ(o->iface->name, "wl_keyboard")) {
      wl_keyboard_key(window->conn, o->id, serial, c_since_start_ms(), 
                      key, pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);

      if (send_mods)
        wl_keyboard_modifiers(window->conn, o->id, serial2, mods_depressed, mods_latched, mods_locked, group);

    }
  }
}

