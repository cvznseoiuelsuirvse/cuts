#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "wayland/types/xdg-shell.h"
#include "wayland/server.h"
#include "wayland/error.h"


int xdg_wm_base_get_xdg_surface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id xdg_surface_id = args[1].n;
  struct c_wl_object *xdg_surface_obj;
  C_WL_CHECK_IF_NOT_REGISTERED(xdg_surface_id, xdg_surface_obj);

  c_wl_object_id wl_surface_id = args[2].o;
  struct c_wl_object *c_wl_surface_obj;
  C_WL_CHECK_IF_REGISTERED(wl_surface_id, c_wl_surface_obj);

  c_wl_object_add(conn, xdg_surface_id, c_wl_interface_get("xdg_surface"), c_wl_surface_obj->data);

  return 0;
}

int xdg_surface_ack_configure(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_wl_object *xdg_surface_obj = c_wl_object_get(conn, xdg_surface_id);

  struct c_wl_surface *wl_surface = xdg_surface_obj->data;

  if (args[1].u != wl_surface->serial) {
    c_wl_error_set(xdg_surface_id, XDG_SURFACE_ERROR_INVALID_SERIAL, "invalid serial");
    return -1;
  }

  if (!wl_surface->serial) {
    c_wl_error_set(xdg_surface_id, XDG_SURFACE_ERROR_INVALID_SERIAL, "unexpected xdg_surface.ack_configure()");
    return -1;
  }

  wl_surface->serial = 0;

  return 0;
}

int xdg_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_wl_object *xdg_surface_obj = c_wl_object_get(conn, xdg_surface_id);

  free(xdg_surface_obj->data);
  c_wl_object_del(conn, xdg_surface_id);

  return 0;
}

int xdg_surface_set_window_geometry(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_wl_object *xdg_surface_obj = c_wl_object_get(conn, xdg_surface_id);
  struct c_wl_surface *wl_surface = xdg_surface_obj->data;

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

int xdg_surface_get_toplevel(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_wl_object *xdg_surface_obj = c_wl_object_get(conn, xdg_surface_id);

  c_wl_new_id xdg_toplevel_id = args[1].n;
  struct c_wl_object *xdg_toplevel;
  C_WL_CHECK_IF_NOT_REGISTERED(xdg_toplevel_id, xdg_toplevel);

  struct c_wl_surface *wl_surface = xdg_surface_obj->data;
  wl_surface->role = C_WL_SURFACE_TOPLEVEL;

  c_wl_object_add(conn, xdg_toplevel_id, c_wl_interface_get("xdg_toplevel"), wl_surface);
  c_wl_array arr = {0, NULL};
  xdg_toplevel_configure(conn, xdg_toplevel_id, 0, 0, &arr);

  wl_surface->serial = C_WL_SERIAL;
  xdg_surface_configure(conn, xdg_surface_id, wl_surface->serial);

  return 0;
}

int xdg_toplevel_set_app_id(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_toplevel_id = args[0].o;
  struct c_wl_object *xdg_toplevel_obj = c_wl_object_get(conn, xdg_toplevel_id);
  struct c_wl_surface *wl_surface = xdg_toplevel_obj->data;

  snprintf(wl_surface->xdg.app_id, sizeof(wl_surface->xdg.app_id), "%s", args[1].s);
  return 0;
}

int xdg_toplevel_set_title(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_toplevel_id = args[0].o;
  struct c_wl_object *xdg_toplevel_obj = c_wl_object_get(conn, xdg_toplevel_id);
  struct c_wl_surface *wl_surface = xdg_toplevel_obj->data;

  snprintf(wl_surface->xdg.title, sizeof(wl_surface->xdg.title), "%s", args[1].s);
  return 0;
}

int xdg_toplevel_set_min_size(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_toplevel_id = args[0].o;
  struct c_wl_object *xdg_toplevel_obj = c_wl_object_get(conn, xdg_toplevel_id);
  struct c_wl_surface *wl_surface = xdg_toplevel_obj->data;

  c_wl_int min_width = args[1].i;
  c_wl_int min_height = args[2].i;

  wl_surface->xdg.min_width = min_width;
  wl_surface->xdg.min_height = min_height;

  return 0;
}

int xdg_toplevel_set_max_size(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_toplevel_id = args[0].o;
  struct c_wl_object *xdg_toplevel_obj = c_wl_object_get(conn, xdg_toplevel_id);
  struct c_wl_surface *wl_surface = xdg_toplevel_obj->data;

  c_wl_int max_width = args[1].i;
  c_wl_int max_height = args[2].i;

  wl_surface->xdg.max_width =  max_width;
  wl_surface->xdg.max_height = max_height;

  return 0;
}

int xdg_toplevel_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id xdg_toplevel_id = args[0].o;
  struct c_wl_object *xdg_toplevel_obj = c_wl_object_get(conn, xdg_toplevel_id);
  struct c_wl_surface *wl_surface = xdg_toplevel_obj->data;

  wl_surface->role = 0;
  c_wl_object_del(conn, xdg_toplevel_id);
  return 0;
}
