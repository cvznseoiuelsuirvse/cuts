#include "../server.h"
#include "xdg-shell.h"

#include <stdint.h>


WL_EVENT xdg_wm_base_ping(struct wl_connection *conn, wl_object_id xdg_wm_base, wl_uint serial) {
  struct wl_message msg = {xdg_wm_base, 0, 0, "u"};
  return wl_connection_send(conn, &msg, 1, serial);
}
WL_REQUEST xdg_wm_base_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_wm_base_create_positioner(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_wm_base_get_xdg_surface(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_wm_base_pong(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(xdg_wm_base_interface, "xdg_wm_base", 7, 4, 
    {"destroy",                xdg_wm_base_destroy,           0,  {0}},
    {"create_positioner",      xdg_wm_base_create_positioner, 1,  "n"},
    {"get_xdg_surface",        xdg_wm_base_get_xdg_surface,   2,  "no"},
    {"pong",                   xdg_wm_base_pong,              1,  "u"},
)

WL_REQUEST xdg_positioner_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_size(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_anchor_rect(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_anchor(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_gravity(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_constraint_adjustment(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_offset(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_reactive(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_parent_size(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_positioner_set_parent_configure(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(xdg_positioner_interface, "xdg_positioner", 7, 10, 
    {"destroy",                xdg_positioner_destroy,        0,  {0}},
    {"set_size",               xdg_positioner_set_size,       2,  "ii"},
    {"set_anchor_rect",        xdg_positioner_set_anchor_rect, 4,  "iiii"},
    {"set_anchor",             xdg_positioner_set_anchor,     1,  "u"},
    {"set_gravity",            xdg_positioner_set_gravity,    1,  "u"},
    {"set_constraint_adjustment", xdg_positioner_set_constraint_adjustment, 1,  "u"},
    {"set_offset",             xdg_positioner_set_offset,     2,  "ii"},
    {"set_reactive",           xdg_positioner_set_reactive,   0,  {0}},
    {"set_parent_size",        xdg_positioner_set_parent_size, 2,  "ii"},
    {"set_parent_configure",   xdg_positioner_set_parent_configure, 1,  "u"},
)

WL_EVENT xdg_surface_configure(struct wl_connection *conn, wl_object_id xdg_surface, wl_uint serial) {
  struct wl_message msg = {xdg_surface, 0, 0, "u"};
  return wl_connection_send(conn, &msg, 1, serial);
}
WL_REQUEST xdg_surface_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_surface_get_toplevel(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_surface_get_popup(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_surface_set_window_geometry(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_surface_ack_configure(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(xdg_surface_interface, "xdg_surface", 7, 5, 
    {"destroy",                xdg_surface_destroy,           0,  {0}},
    {"get_toplevel",           xdg_surface_get_toplevel,      1,  "n"},
    {"get_popup",              xdg_surface_get_popup,         3,  "noo"},
    {"set_window_geometry",    xdg_surface_set_window_geometry, 4,  "iiii"},
    {"ack_configure",          xdg_surface_ack_configure,     1,  "u"},
)

WL_EVENT xdg_toplevel_configure(struct wl_connection *conn, wl_object_id xdg_toplevel, wl_int width, wl_int height, wl_array states, wl_uint states_size) {
  struct wl_message msg = {xdg_toplevel, 0, 0, "iia"};
  return wl_connection_send(conn, &msg, 3, width, height, states);
}
WL_EVENT xdg_toplevel_close(struct wl_connection *conn, wl_object_id xdg_toplevel) {
  struct wl_message msg = {xdg_toplevel, 1, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT xdg_toplevel_configure_bounds(struct wl_connection *conn, wl_object_id xdg_toplevel, wl_int width, wl_int height) {
  struct wl_message msg = {xdg_toplevel, 2, 0, "ii"};
  return wl_connection_send(conn, &msg, 2, width, height);
}
WL_EVENT xdg_toplevel_wm_capabilities(struct wl_connection *conn, wl_object_id xdg_toplevel, wl_array capabilities, wl_uint capabilities_size) {
  struct wl_message msg = {xdg_toplevel, 3, 0, "a"};
  return wl_connection_send(conn, &msg, 1, capabilities);
}
WL_REQUEST xdg_toplevel_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_set_parent(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_set_title(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_set_app_id(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_show_window_menu(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_move(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_resize(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_set_max_size(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_set_min_size(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_set_maximized(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_unset_maximized(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_set_fullscreen(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_unset_fullscreen(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_toplevel_set_minimized(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(xdg_toplevel_interface, "xdg_toplevel", 7, 14, 
    {"destroy",                xdg_toplevel_destroy,          0,  {0}},
    {"set_parent",             xdg_toplevel_set_parent,       1,  "o"},
    {"set_title",              xdg_toplevel_set_title,        1,  "s"},
    {"set_app_id",             xdg_toplevel_set_app_id,       1,  "s"},
    {"show_window_menu",       xdg_toplevel_show_window_menu, 4,  "ouii"},
    {"move",                   xdg_toplevel_move,             2,  "ou"},
    {"resize",                 xdg_toplevel_resize,           3,  "ouu"},
    {"set_max_size",           xdg_toplevel_set_max_size,     2,  "ii"},
    {"set_min_size",           xdg_toplevel_set_min_size,     2,  "ii"},
    {"set_maximized",          xdg_toplevel_set_maximized,    0,  {0}},
    {"unset_maximized",        xdg_toplevel_unset_maximized,  0,  {0}},
    {"set_fullscreen",         xdg_toplevel_set_fullscreen,   1,  "o"},
    {"unset_fullscreen",       xdg_toplevel_unset_fullscreen, 0,  {0}},
    {"set_minimized",          xdg_toplevel_set_minimized,    0,  {0}},
)

WL_EVENT xdg_popup_configure(struct wl_connection *conn, wl_object_id xdg_popup, wl_int x, wl_int y, wl_int width, wl_int height) {
  struct wl_message msg = {xdg_popup, 0, 0, "iiii"};
  return wl_connection_send(conn, &msg, 4, x, y, width, height);
}
WL_EVENT xdg_popup_popup_done(struct wl_connection *conn, wl_object_id xdg_popup) {
  struct wl_message msg = {xdg_popup, 1, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT xdg_popup_repositioned(struct wl_connection *conn, wl_object_id xdg_popup, wl_uint token) {
  struct wl_message msg = {xdg_popup, 2, 0, "u"};
  return wl_connection_send(conn, &msg, 1, token);
}
WL_REQUEST xdg_popup_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_popup_grab(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST xdg_popup_reposition(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(xdg_popup_interface, "xdg_popup", 7, 3, 
    {"destroy",                xdg_popup_destroy,             0,  {0}},
    {"grab",                   xdg_popup_grab,                2,  "ou"},
    {"reposition",             xdg_popup_reposition,          2,  "ou"},
)

