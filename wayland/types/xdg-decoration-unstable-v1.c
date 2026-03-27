#include <stdint.h>

#include "wayland/server.h"


#include "wayland/types/xdg-decoration-unstable-v1.h"
C_WL_REQUEST zxdg_decoration_manager_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args);

C_WL_REQUEST zxdg_decoration_manager_v1_get_toplevel_decoration(struct c_wl_connection *conn, union c_wl_arg *args);

C_WL_INTERFACE_REGISTER(zxdg_decoration_manager_v1_interface, "zxdg_decoration_manager_v1", 2, 2, 
    {"destroy",                zxdg_decoration_manager_v1_destroy, 0,  {0}},
    {"get_toplevel_decoration", zxdg_decoration_manager_v1_get_toplevel_decoration, 2,  "no"},
)

C_WL_EVENT zxdg_toplevel_decoration_v1_configure(struct c_wl_connection *conn, c_wl_object_id zxdg_toplevel_decoration_v1, enum zxdg_toplevel_decoration_v1_mode_enum mode) {
  struct c_wl_message msg = {zxdg_toplevel_decoration_v1, 0, "u", "configure"};
  return c_wl_connection_send(conn, &msg, 1, mode);
}
C_WL_REQUEST zxdg_toplevel_decoration_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args);

C_WL_REQUEST zxdg_toplevel_decoration_v1_set_mode(struct c_wl_connection *conn, union c_wl_arg *args);

C_WL_REQUEST zxdg_toplevel_decoration_v1_unset_mode(struct c_wl_connection *conn, union c_wl_arg *args);

C_WL_INTERFACE_REGISTER(zxdg_toplevel_decoration_v1_interface, "zxdg_toplevel_decoration_v1", 2, 3, 
    {"destroy",                zxdg_toplevel_decoration_v1_destroy, 0,  {0}},
    {"set_mode",               zxdg_toplevel_decoration_v1_set_mode, 1,  "u"},
    {"unset_mode",             zxdg_toplevel_decoration_v1_unset_mode, 0,  {0}},
)

