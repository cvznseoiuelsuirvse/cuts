#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

#include "wayland/types/xdg-shell.h"

int xdg_wm_base_get_xdg_surface(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_new_id xdg_surface_id = args[1].n;
  struct c_wl_object *xdg_surface;
  C_WL_CHECK_IF_NOT_REGISTERED(xdg_surface_id, xdg_surface);

  c_wl_object_id wl_surface_id = args[2].o;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_REGISTERED(wl_surface_id, wl_surface);

  struct c_wl_surface *c_wl_surface = c_wl_object_data_get2(wl_surface);
  c_wl_surface->xdg.surface_id = xdg_surface_id;

  c_wl_object_add(conn, xdg_surface_id, c_wl_interface_get("xdg_surface"), wl_surface->data);

  return 0;
}

int xdg_surface_ack_configure(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_wl_surface *wl_surface = c_wl_object_data_get(conn, xdg_surface_id);

  if (!args[1].u) {
    c_wl_error_set(xdg_surface_id, XDG_SURFACE_ERROR_INVALID_SERIAL, "invalid serial");
    return -1;
  }

  // if (!wl_surface->xdg.serial) {
  //   c_wl_error_set(xdg_surface_id, XDG_SURFACE_ERROR_INVALID_SERIAL, "unexpected xdg_surface.ack_configure()");
  //   return -1;
  // }

  wl_surface->xdg.serial = 0;

  return 0;
}

int xdg_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_wl_surface *c_wl_surface = c_wl_object_data_get(conn, xdg_surface_id);
  c_wl_surface->role = 0;
  memset(&c_wl_surface->xdg, 0, sizeof(c_wl_surface->xdg));
  c_wl_object_del(conn, xdg_surface_id);
  return 0;
}

int xdg_surface_set_window_geometry(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_surface *wl_surface = c_wl_object_data_get(conn, args[0].o);

  c_wl_int x = args[1].i;
  c_wl_int y = args[2].i;
  c_wl_int width = args[3].i;
  c_wl_int height = args[4].i;

  wl_surface->xdg.x = x;
  wl_surface->xdg.y = y;
  wl_surface->xdg.width = width;
  wl_surface->xdg.height = height;

  return 0;
}

int xdg_surface_get_toplevel(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_wl_object *xdg_surface = c_wl_object_get(conn, xdg_surface_id);

  c_wl_new_id xdg_toplevel_id = args[1].n;
  struct c_wl_object *xdg_toplevel;
  C_WL_CHECK_IF_NOT_REGISTERED(xdg_toplevel_id, xdg_toplevel);

  struct c_wl_surface *c_wl_surface = c_wl_object_data_get2(xdg_surface);
  c_wl_surface->role = C_WL_SURFACE_ROLE_XDG_TOPLEVEL;
  c_wl_surface->xdg.toplevel_id = xdg_toplevel_id;
  c_wl_surface->xdg.children = c_list_new();

  c_wl_object_add(conn, xdg_toplevel_id, c_wl_interface_get("xdg_toplevel"), xdg_surface->data);

  struct c_wl_array arr = {0};

  xdg_toplevel_configure(conn, xdg_toplevel_id, 0, 0, &arr);
  xdg_surface_configure(conn, xdg_surface_id, c_wl_serial());

  return 0;
}

int xdg_toplevel_set_app_id(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_toplevel_id = args[0].o;
  struct c_wl_surface *wl_surface = c_wl_object_data_get(conn, xdg_toplevel_id);

  snprintf(wl_surface->xdg.app_id, sizeof(wl_surface->xdg.app_id), "%s", args[1].s);
  return 0;
}

int xdg_toplevel_set_title(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_toplevel_id = args[0].o;
  struct c_wl_surface *wl_surface = c_wl_object_data_get(conn, xdg_toplevel_id);
  snprintf(wl_surface->xdg.title, sizeof(wl_surface->xdg.title), "%s", args[1].s);
  return 0;
}

int xdg_toplevel_set_min_size(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_surface *wl_surface = c_wl_object_data_get(conn,args[0].o);

  c_wl_int min_width = args[1].i;
  c_wl_int min_height = args[2].i;

  wl_surface->xdg.min_width = min_width;
  wl_surface->xdg.min_height = min_height;

  return 0;
}

int xdg_toplevel_set_max_size(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_surface *wl_surface = c_wl_object_data_get(conn,args[0].o);

  c_wl_int max_width = args[1].i;
  c_wl_int max_height = args[2].i;

  wl_surface->xdg.max_width =  max_width;
  wl_surface->xdg.max_height = max_height;

  return 0;
}

int xdg_toplevel_set_parent(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *xdg_toplevel_surface_child = c_wl_object_get(conn, args[0].o);
  struct c_wl_object *xdg_toplevel_surface_parent = c_wl_object_get(conn, args[1].o);
  struct c_wl_surface *surface_child = c_wl_object_data_get2(xdg_toplevel_surface_child);

  if (xdg_toplevel_surface_child == xdg_toplevel_surface_parent)
    return c_wl_error_set(args[0].o, XDG_TOPLEVEL_ERROR_INVALID_PARENT, "parent and child cannot be the same objects");

  if (!xdg_toplevel_surface_parent) {
    if (surface_child->xdg.parent)
      c_list_remove_ptr(&surface_child->xdg.parent->xdg.children, surface_child);
    surface_child->xdg.parent = NULL;
    return 0;
  }

  struct c_wl_surface *surface_parent = c_wl_object_data_get2(xdg_toplevel_surface_parent);
  c_list_push(surface_parent->xdg.children, surface_child, 0);
  surface_child->xdg.parent = surface_parent;

  return 0;
}

int xdg_toplevel_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_toplevel_id = args[0].o;
  struct c_wl_surface *wl_surface = c_wl_object_data_get(conn, xdg_toplevel_id);

  wl_surface->role = 0;
  if (wl_surface->xdg.children) {
    struct c_wl_surface *child_surface;
    c_list_for_each(wl_surface->xdg.children, child_surface)
      child_surface->xdg.parent = NULL;
    c_list_destroy(wl_surface->xdg.children);
  }

  c_wl_object_del(conn, xdg_toplevel_id);
  return 0;
}
