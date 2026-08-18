#include <stdint.h>

#include "wayland/server.h"
#include "wayland/display.h"
#include "wayland/proto/viewporter.h"


C_WL_INTERFACE_REGISTER(wp_viewporter, 1, 2, 0, 
    {"destroy",                wp_viewporter_destroy,         0,  {0}},
    {"get_viewport",           wp_viewporter_get_viewport,    2,  "no"},
)
void wp_viewporter_add_listener(struct c_wl_display *display, struct c_wp_viewporter_listeners *listeners, void *userdata) {
  c_wl_display_add_interface_listener(display, "wp_viewporter", listeners, userdata);
}

C_WL_INTERFACE_REGISTER(wp_viewport, 1, 3, 0, 
    {"destroy",                wp_viewport_destroy,           0,  {0}},
    {"set_source",             wp_viewport_set_source,        4,  "ffff"},
    {"set_destination",        wp_viewport_set_destination,   2,  "ii"},
)
void wp_viewport_add_listener(struct c_wl_display *display, struct c_wp_viewport_listeners *listeners, void *userdata) {
  c_wl_display_add_interface_listener(display, "wp_viewport", listeners, userdata);
}

