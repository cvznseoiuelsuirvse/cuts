#include "wayland/proto/wayland.h"
#include "wayland/impl/wayland.h"

#include "wayland/display.h"
#include "compositor/surface.h"
#include "util/helpers.h"
#include "util/log.h"

#define EVENT_POINTER  1 << 0
#define EVENT_KEYBOARD 1 << 1

static void enter(struct c_wl_surface *surface, double mx, double my, int devs) {
  struct c_wl_connection *conn = surface->obj->conn;

  int wl_pointer_serial = c_wl_serial();
  int wl_keyboard_serial = c_wl_serial();
  int wl_keyboard_modifiers_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(conn, o) {
    SWITCH_STR(o->iface->name)
      CASE_STR("wl_keyboard")
        if (devs & EVENT_KEYBOARD) {
          c_wl_array arr = {0};
          wl_keyboard_enter(conn, o->id, wl_keyboard_serial, surface->obj->id, &arr);
          wl_keyboard_modifiers(conn, o->id, wl_keyboard_modifiers_serial, 0, 0, 0, 0);
        }

      CASE_STR("wl_pointer")
        if (devs & EVENT_POINTER) {
          c_wl_fixed hotspot_x = c_wl_fixed_from_double(mx);
          c_wl_fixed hotspot_y = c_wl_fixed_from_double(my);

          wl_pointer_enter(conn, o->id, wl_pointer_serial, surface->obj->id, hotspot_x, hotspot_y);
          wl_pointer_frame(conn, o->id);
        }

    SWITCH_STR_END
    }
}

static void leave(struct c_wl_surface *surface, int devs) {
  struct c_wl_connection *conn = surface->obj->conn;

  int wl_keyboard_serial = c_wl_serial();
  int wl_pointer_serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(conn, o) {
    SWITCH_STR(o->iface->name)
      CASE_STR("wl_keyboard")
        if (devs & EVENT_KEYBOARD) {
          wl_keyboard_leave(conn, o->id, wl_keyboard_serial, surface->obj->id);
        }
      CASE_STR("wl_pointer")
        if (devs & EVENT_POINTER) {
          wl_pointer_leave(conn, o->id, wl_pointer_serial, surface->obj->id);
          wl_pointer_frame(conn, o->id);
        }
    SWITCH_STR_END
  }

}

void c_surface_enter(struct c_wl_surface *surface, double mx, double my) { 
  enter(surface, mx, my, EVENT_POINTER | EVENT_KEYBOARD); 
}
void c_surface_enter_pointer(struct c_wl_surface *surface, double mx, double my) { 
  enter(surface, mx, my, EVENT_POINTER); 
}
void c_surface_enter_keyboard(struct c_wl_surface *surface) {
  enter(surface, 0, 0, EVENT_KEYBOARD);
}

void c_surface_leave(struct c_wl_surface *surface) {
  leave(surface, EVENT_POINTER | EVENT_KEYBOARD);
}
void c_surface_leave_pointer(struct c_wl_surface *surface) {
  leave(surface, EVENT_POINTER);
}
void c_surface_leave_keyboard(struct c_wl_surface *surface) {
  leave(surface, EVENT_KEYBOARD);
}

void c_surface_pointer_move(struct c_wl_connection *conn, double x, double y) {
  c_wl_fixed hotspot_x = c_wl_fixed_from_double(x);
  c_wl_fixed hotspot_y = c_wl_fixed_from_double(y);

  c_wl_object_id frames[16];
  size_t frame_n = 0;

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(conn, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_motion(conn, o->id, c_since_start_ms(), hotspot_x, hotspot_y);
      frames[frame_n++] = o->id;
    }
  }

  for (size_t i = 0; i < frame_n; i++)
    wl_pointer_frame(conn, frames[i]);

  c_wl_connection_flush(conn);
}

void c_surface_pointer_button(struct c_wl_connection *conn, uint32_t button, int pressed) {
  int serial = c_wl_serial();

  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(conn, o) {
    if (STREQ(o->iface->name, "wl_pointer")) {
      wl_pointer_button(conn, o->id, serial, c_since_start_ms(), button, pressed);
      wl_pointer_frame(conn, o->id);
    }
  }
  c_wl_connection_flush(conn);
}

void c_surface_pointer_scroll(struct c_wl_connection *conn, double axis, double axis120,
                             enum wl_pointer_axis_source_enum axis_source,
                             enum wl_pointer_axis_enum axis_orient,
                             int axis_discrete) {
  struct c_wl_object *o;
  c_wl_objects_for_each(conn, o) {
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

      wl_pointer_axis(conn, o->id, c_since_start_ms(), axis_orient, c_wl_fixed_from_double(axis));
      wl_pointer_frame(conn, o->id);
    }
  }
  c_wl_connection_flush(conn);
}

void c_surface_keyboard_key(struct c_wl_connection *conn, int32_t key, int pressed, 
		xkb_mod_mask_t mods_depressed, xkb_mod_mask_t mods_latched, xkb_mod_mask_t mods_locked, 
		xkb_layout_index_t group, int send_mods) {
  int serial = c_wl_serial();
  int serial2 = c_wl_serial();
  
  struct c_wl_object *o = NULL;
  c_wl_objects_for_each(conn, o) {
    if (STREQ(o->iface->name, "wl_keyboard")) {
      wl_keyboard_key(conn, o->id, serial, c_since_start_ms(), 
                      key, pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);

      if (send_mods)
        wl_keyboard_modifiers(conn, o->id, serial2, mods_depressed, mods_latched, mods_locked, group);

    }
  }
  c_wl_connection_flush(conn);
}
