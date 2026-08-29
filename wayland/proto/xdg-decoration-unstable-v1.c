/* auto-generated */
#include <stdint.h>

#include "wayland/server.h"
#include "wayland/display.h"
#include "wayland/proto/xdg-decoration-unstable-v1.h"


C_WL_INTERFACE_REGISTER(zxdg_decoration_manager_v1, 2, 2, 0, 
    {"destroy",                zxdg_decoration_manager_v1_destroy, 0,  {0}},
    {"get_toplevel_decoration", zxdg_decoration_manager_v1_get_toplevel_decoration, 2,  "no"},
)
void zxdg_decoration_manager_v1_add_listener(struct c_wl_display *display, struct c_zxdg_decoration_manager_v1_listeners *listeners, void *userdata) {
  c_wl_display_add_interface_listener(display, "zxdg_decoration_manager_v1", listeners, userdata);
}

C_WL_EVENT zxdg_toplevel_decoration_v1_configure(struct c_wl_connection *conn, c_wl_object_id zxdg_toplevel_decoration_v1, enum zxdg_toplevel_decoration_v1_mode_enum mode) {
  struct c_wl_message msg = {zxdg_toplevel_decoration_v1, 0, "u", "configure"};
  return c_wl_connection_post(conn, &msg, 1, mode);
}
C_WL_INTERFACE_REGISTER(zxdg_toplevel_decoration_v1, 2, 3, 0, 
    {"destroy",                zxdg_toplevel_decoration_v1_destroy, 0,  {0}},
    {"set_mode",               zxdg_toplevel_decoration_v1_set_mode, 1,  "u"},
    {"unset_mode",             zxdg_toplevel_decoration_v1_unset_mode, 0,  {0}},
)
void zxdg_toplevel_decoration_v1_add_listener(struct c_wl_display *display, struct c_zxdg_toplevel_decoration_v1_listeners *listeners, void *userdata) {
  c_wl_display_add_interface_listener(display, "zxdg_toplevel_decoration_v1", listeners, userdata);
}

