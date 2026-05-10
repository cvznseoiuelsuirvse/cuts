#include "wayland/types/xdg-decoration-unstable-v1.h"
#include "wayland/types/xdg-shell.h"

#include "util/malloc.h"

int zxdg_decoration_manager_v1_get_toplevel_decoration(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id new_id = args[1].n;
  struct c_wl_object *toplevel_dec;
  struct c_wl_object *toplevel;
  C_WL_CHECK_IF_NOT_REGISTERED(new_id, toplevel_dec);
  C_WL_CHECK_IF_REGISTERED(args[2].o, toplevel);

  c_ref(toplevel->data);
  c_wl_object_add(conn, new_id, self->version, c_wl_interface_get("zxdg_toplevel_decoration_v1"), toplevel->data);
  return 0;
}

int zxdg_decoration_manager_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_del(conn, args[0].o);
  return 0;
}


int zxdg_toplevel_decoration_v1_set_mode(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_object_get(conn, args[0].o)->data;

  if (!(args[1].e & (ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE | ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)))
    c_wl_error_set_and_return(args[0].o,
                              ZXDG_TOPLEVEL_DECORATION_V1_ERROR_INVALID_MODE,
                              "invalid mode");

  zxdg_toplevel_decoration_v1_configure(conn, args[0].o, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  xdg_surface_configure(conn, surface->id, c_wl_serial());
  return 0;
}

int zxdg_toplevel_decoration_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_del(conn, args[0].o);
  return 0;
}

