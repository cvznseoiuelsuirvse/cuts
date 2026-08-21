#include <stdint.h>

#include "wayland/server.h"
#include "wayland/display.h"
#include "wayland/proto/cursor-shape-v1.h"


C_WL_INTERFACE_REGISTER(wp_cursor_shape_manager_v1, 2, 3, 0, 
    {"destroy",                wp_cursor_shape_manager_v1_destroy, 0,  {0}},
    {"get_pointer",            wp_cursor_shape_manager_v1_get_pointer, 2,  "no"},
    {"get_tablet_tool_v2",     wp_cursor_shape_manager_v1_get_tablet_tool_v2, 2,  "no"},
)
void wp_cursor_shape_manager_v1_add_listener(struct c_wl_display *display, struct c_wp_cursor_shape_manager_v1_listeners *listeners, void *userdata) {
  c_wl_display_add_interface_listener(display, "wp_cursor_shape_manager_v1", listeners, userdata);
}

C_WL_INTERFACE_REGISTER(wp_cursor_shape_device_v1, 2, 2, 0, 
    {"destroy",                wp_cursor_shape_device_v1_destroy, 0,  {0}},
    {"set_shape",              wp_cursor_shape_device_v1_set_shape, 2,  "uu"},
)
void wp_cursor_shape_device_v1_add_listener(struct c_wl_display *display, struct c_wp_cursor_shape_device_v1_listeners *listeners, void *userdata) {
  c_wl_display_add_interface_listener(display, "wp_cursor_shape_device_v1", listeners, userdata);
}

