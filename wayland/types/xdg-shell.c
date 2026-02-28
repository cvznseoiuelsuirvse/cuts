#include "wayland/server.h"
#include "wayland/types/xdg-shell.h"

#include <stdint.h>


C_WL_EVENT xdg_wm_base_ping(struct c_wl_connection *conn, c_wl_object_id xdg_wm_base, c_wl_uint serial) {
  struct c_wl_message msg = {xdg_wm_base, 0, "u", "ping"};
  return c_wl_connection_send(conn, &msg, 1, serial);
}
C_WL_REQUEST xdg_wm_base_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_wm_base_create_positioner(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_wm_base_get_xdg_surface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_wm_base_pong(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(xdg_wm_base_interface, "xdg_wm_base", 7, 4, 
    {"destroy",                xdg_wm_base_destroy,          NULL, 0,  {0}},
    {"create_positioner",      xdg_wm_base_create_positioner,NULL, 1,  "n"},
    {"get_xdg_surface",        xdg_wm_base_get_xdg_surface,  NULL, 2,  "no"},
    {"pong",                   xdg_wm_base_pong,             NULL, 1,  "u"},
)

C_WL_REQUEST xdg_positioner_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_size(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_anchor_rect(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_anchor(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_gravity(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_constraint_adjustment(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_offset(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_reactive(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_parent_size(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_positioner_set_parent_configure(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(xdg_positioner_interface, "xdg_positioner", 7, 10, 
    {"destroy",                xdg_positioner_destroy,       NULL, 0,  {0}},
    {"set_size",               xdg_positioner_set_size,      NULL, 2,  "ii"},
    {"set_anchor_rect",        xdg_positioner_set_anchor_rect,NULL, 4,  "iiii"},
    {"set_anchor",             xdg_positioner_set_anchor,    NULL, 1,  "u"},
    {"set_gravity",            xdg_positioner_set_gravity,   NULL, 1,  "u"},
    {"set_constraint_adjustment", xdg_positioner_set_constraint_adjustment,NULL, 1,  "u"},
    {"set_offset",             xdg_positioner_set_offset,    NULL, 2,  "ii"},
    {"set_reactive",           xdg_positioner_set_reactive,  NULL, 0,  {0}},
    {"set_parent_size",        xdg_positioner_set_parent_size,NULL, 2,  "ii"},
    {"set_parent_configure",   xdg_positioner_set_parent_configure,NULL, 1,  "u"},
)

C_WL_EVENT xdg_surface_configure(struct c_wl_connection *conn, c_wl_object_id xdg_surface, c_wl_uint serial) {
  struct c_wl_message msg = {xdg_surface, 0, "u", "configure"};
  return c_wl_connection_send(conn, &msg, 1, serial);
}
C_WL_REQUEST xdg_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_surface_get_toplevel(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_surface_get_popup(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_surface_set_window_geometry(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_surface_ack_configure(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(xdg_surface_interface, "xdg_surface", 7, 5, 
    {"destroy",                xdg_surface_destroy,          NULL, 0,  {0}},
    {"get_toplevel",           xdg_surface_get_toplevel,     NULL, 1,  "n"},
    {"get_popup",              xdg_surface_get_popup,        NULL, 3,  "noo"},
    {"set_window_geometry",    xdg_surface_set_window_geometry,NULL, 4,  "iiii"},
    {"ack_configure",          xdg_surface_ack_configure,    NULL, 1,  "u"},
)

C_WL_EVENT xdg_toplevel_configure(struct c_wl_connection *conn, c_wl_object_id xdg_toplevel, c_wl_int width, c_wl_int height, c_wl_array *states) {
  struct c_wl_message msg = {xdg_toplevel, 0, "iia", "configure"};
  return c_wl_connection_send(conn, &msg, 3, width, height, states);
}
C_WL_EVENT xdg_toplevel_close(struct c_wl_connection *conn, c_wl_object_id xdg_toplevel) {
  struct c_wl_message msg = {xdg_toplevel, 1, {0}, "close"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT xdg_toplevel_configure_bounds(struct c_wl_connection *conn, c_wl_object_id xdg_toplevel, c_wl_int width, c_wl_int height) {
  struct c_wl_message msg = {xdg_toplevel, 2, "ii", "configure_bounds"};
  return c_wl_connection_send(conn, &msg, 2, width, height);
}
C_WL_EVENT xdg_toplevel_wm_capabilities(struct c_wl_connection *conn, c_wl_object_id xdg_toplevel, c_wl_array *capabilities) {
  struct c_wl_message msg = {xdg_toplevel, 3, "a", "wm_capabilities"};
  return c_wl_connection_send(conn, &msg, 1, capabilities);
}
C_WL_REQUEST xdg_toplevel_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_set_parent(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_set_title(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_set_app_id(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_show_window_menu(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_move(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_resize(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_set_max_size(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_set_min_size(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_set_maximized(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_unset_maximized(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_set_fullscreen(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_unset_fullscreen(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_toplevel_set_minimized(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(xdg_toplevel_interface, "xdg_toplevel", 7, 14, 
    {"destroy",                xdg_toplevel_destroy,         NULL, 0,  {0}},
    {"set_parent",             xdg_toplevel_set_parent,      NULL, 1,  "o"},
    {"set_title",              xdg_toplevel_set_title,       NULL, 1,  "s"},
    {"set_app_id",             xdg_toplevel_set_app_id,      NULL, 1,  "s"},
    {"show_window_menu",       xdg_toplevel_show_window_menu,NULL, 4,  "ouii"},
    {"move",                   xdg_toplevel_move,            NULL, 2,  "ou"},
    {"resize",                 xdg_toplevel_resize,          NULL, 3,  "ouu"},
    {"set_max_size",           xdg_toplevel_set_max_size,    NULL, 2,  "ii"},
    {"set_min_size",           xdg_toplevel_set_min_size,    NULL, 2,  "ii"},
    {"set_maximized",          xdg_toplevel_set_maximized,   NULL, 0,  {0}},
    {"unset_maximized",        xdg_toplevel_unset_maximized, NULL, 0,  {0}},
    {"set_fullscreen",         xdg_toplevel_set_fullscreen,  NULL, 1,  "o"},
    {"unset_fullscreen",       xdg_toplevel_unset_fullscreen,NULL, 0,  {0}},
    {"set_minimized",          xdg_toplevel_set_minimized,   NULL, 0,  {0}},
)

C_WL_EVENT xdg_popup_configure(struct c_wl_connection *conn, c_wl_object_id xdg_popup, c_wl_int x, c_wl_int y, c_wl_int width, c_wl_int height) {
  struct c_wl_message msg = {xdg_popup, 0, "iiii", "configure"};
  return c_wl_connection_send(conn, &msg, 4, x, y, width, height);
}
C_WL_EVENT xdg_popup_popup_done(struct c_wl_connection *conn, c_wl_object_id xdg_popup) {
  struct c_wl_message msg = {xdg_popup, 1, {0}, "popup_done"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT xdg_popup_repositioned(struct c_wl_connection *conn, c_wl_object_id xdg_popup, c_wl_uint token) {
  struct c_wl_message msg = {xdg_popup, 2, "u", "repositioned"};
  return c_wl_connection_send(conn, &msg, 1, token);
}
C_WL_REQUEST xdg_popup_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_popup_grab(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST xdg_popup_reposition(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(xdg_popup_interface, "xdg_popup", 7, 3, 
    {"destroy",                xdg_popup_destroy,            NULL, 0,  {0}},
    {"grab",                   xdg_popup_grab,               NULL, 2,  "ou"},
    {"reposition",             xdg_popup_reposition,         NULL, 2,  "ou"},
)

