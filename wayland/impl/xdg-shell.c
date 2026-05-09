#include <stdlib.h>
#include <string.h>

#include "wayland/types.h"
#include "wayland/types/xdg-shell.h"
#include "wayland/types/wayland.h"

int xdg_wm_base_get_xdg_surface(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id xdg_surface_id = args[1].n;
  struct c_wl_object *xdg_surface_o;
  C_WL_CHECK_IF_NOT_REGISTERED(xdg_surface_id, xdg_surface_o);

  c_wl_object_id wl_surface_id = args[2].o;
  struct c_wl_object *wl_surface_o;
  C_WL_CHECK_IF_REGISTERED(wl_surface_id, wl_surface_o);

  struct c_wl_surface *wl_surface = c_wl_object_data_get2(wl_surface_o);

  struct c_xdg_surface *xdg_surface = calloc(1, sizeof(*xdg_surface));
  if (!xdg_surface) 
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_NO_MEMORY, "failed to calloc c_xdg_surface");

  xdg_surface->id = xdg_surface_id;

  xdg_surface->surface = wl_surface;
  wl_surface->xdg_surface = xdg_surface;

  struct c_wl_object_data *data = c_wl_object_data_create(xdg_surface);
  c_wl_object_add(conn, xdg_surface_id, self->version, c_wl_interface_get("xdg_surface"), data);

  return 0;
}

int xdg_surface_ack_configure(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_surface_id = args[0].o;
  if (!args[1].u) {
    c_wl_error_set_and_return(xdg_surface_id, XDG_SURFACE_ERROR_INVALID_SERIAL, "invalid serial");
    return -1;
  }
  return 0;
}

int xdg_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_xdg_surface *xdg_surface = c_wl_object_data_get(conn, xdg_surface_id);

  if (xdg_surface->children) {
    struct c_xdg_surface *s;
    c_list_for_each(xdg_surface->children, s)
      s->parent = NULL;
    c_list_destroy(xdg_surface->children);
  }

  xdg_surface->surface->xdg_surface = NULL;

  c_wl_object_del(conn, xdg_surface_id);
  return 0;
}

int xdg_surface_set_window_geometry(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_object_data_get(conn, args[0].o);

  c_wl_int x = args[1].i;
  c_wl_int y = args[2].i;
  c_wl_int width = args[3].i;
  c_wl_int height = args[4].i;

  surface->x = x;
  surface->y = y;
  surface->width = width;
  surface->height = height;

  return 0;
}

int xdg_surface_get_toplevel(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_wl_object *xdg_surface_o = c_wl_object_get(conn, xdg_surface_id);

  c_wl_new_id xdg_toplevel_id = args[1].n;
  struct c_wl_object *xdg_toplevel;
  C_WL_CHECK_IF_NOT_REGISTERED(xdg_toplevel_id, xdg_toplevel);

  struct c_xdg_surface *xdg_surface = c_wl_object_data_get2(xdg_surface_o);
  xdg_surface->surface->role = C_WL_SURFACE_ROLE_XDG_TOPLEVEL;
  xdg_surface->toplevel.id = xdg_toplevel_id;

  c_wl_object_add(conn, xdg_toplevel_id, xdg_surface_o->version, c_wl_interface_get("xdg_toplevel"), xdg_surface_o->data);

  struct c_wl_array arr = {0};

  xdg_toplevel_configure(conn, xdg_toplevel_id, 0, 0, &arr);
  xdg_surface_configure(conn, xdg_surface_id, c_wl_serial());

  return 0;
}

int xdg_toplevel_set_app_id(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_object_data_get(conn, args[0].o);
  surface->toplevel.app_id = strdup(args[1].s);
  return 0;
}

int xdg_toplevel_set_title(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_object_data_get(conn, args[0].o);
  surface->toplevel.title = strdup(args[1].s);
  return 0;
}

int xdg_toplevel_set_min_size(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_object_data_get(conn, args[0].o);

  c_wl_int min_width = args[1].i;
  c_wl_int min_height = args[2].i;

  surface->toplevel.min_width = min_width;
  surface->toplevel.min_height = min_height;

  return 0;
}

int xdg_toplevel_set_max_size(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_object_data_get(conn, args[0].o);

  c_wl_int max_width = args[1].i;
  c_wl_int max_height = args[2].i;

  surface->toplevel.max_width =  max_width;
  surface->toplevel.max_height = max_height;

  return 0;
}

int xdg_toplevel_set_parent(struct c_wl_connection *conn, union c_wl_arg *args) {
  if (args[0].o == args[1].o)
    c_wl_error_set_and_return(args[0].o, XDG_TOPLEVEL_ERROR_INVALID_PARENT,
        "parent and child cannot be the same objects");

  struct c_xdg_surface *xdg_surface = c_wl_object_data_get(conn, args[0].o);
  struct c_xdg_surface *parent = xdg_surface->parent;

  if (args[1].o == 0) {
    if (parent)
      c_list_remove_ptr(&parent->children, xdg_surface);

    xdg_surface->parent = NULL;
    return 0;
  }

  struct c_xdg_surface *xdg_surface_parent = c_wl_object_data_get(conn, args[1].o);
  struct c_xdg_surface *parent2 = xdg_surface_parent->parent;

  if (!parent2->children)
    parent2->children = c_list_new();


  c_list_push(parent2->children, xdg_surface, 0);
  xdg_surface->parent = xdg_surface_parent;

  return 0;
}

int xdg_toplevel_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *xdg_surface = c_wl_object_data_get(conn, args[0].o);
  struct c_xdg_surface *parent = xdg_surface->parent;

  xdg_surface->surface->role = 0;
  if (parent)
    c_list_remove_ptr(&parent->children, xdg_surface);

  if (xdg_surface->toplevel.title)  free(xdg_surface->toplevel.title);
  if (xdg_surface->toplevel.app_id) free(xdg_surface->toplevel.app_id);

  c_wl_object_del(conn, args[0].o);
  return 0;
}


int xdg_wm_base_create_positioner(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_object *positioner;
  C_WL_CHECK_IF_NOT_REGISTERED(args[1].n, positioner);

  struct c_xdg_positioner *p = calloc(1, sizeof(*p));
  if (!p) {
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_NO_MEMORY, "failed to calloc c_xdg_positioner");
    return -1;
  }
  p->id = args[1].n;

  struct c_wl_object_data *data = c_wl_object_data_create(p);
  c_wl_object_add(conn, args[1].n, self->version, c_wl_interface_get("xdg_positioner"), data);

  return 0;
}

int xdg_positioner_set_size(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_object_data_get(conn, args[0].o);
  p->width = args[1].i;
  p->height = args[2].i;
  return 0;
}

int xdg_positioner_set_offset(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_object_data_get(conn, args[0].o);
  p->x = args[1].i;
  p->y = args[2].i;
  return 0;
}

int xdg_positioner_set_gravity(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_object_data_get(conn, args[0].o);
  p->gravity = args[1].i;
  return 0;
}

int xdg_positioner_set_constraint_adjustment(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_object_data_get(conn, args[0].o);
  p->constraint_adjustment = args[1].i;
  return 0;
}

int xdg_positioner_set_anchor_rect(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_object_data_get(conn, args[0].o);
  p->anchor_rect.x =       args[1].i;
  p->anchor_rect.y =       args[2].i;
  p->anchor_rect.width =   args[3].i;
  p->anchor_rect.height =  args[4].i;
  return 0;
}

int xdg_positioner_set_anchor(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_object_data_get(conn, args[0].o);
  p->anchor =  args[1].i;
  return 0;
}


int xdg_positioner_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_del(conn, args[0].o);
  return 0;
}


int xdg_surface_get_popup(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  struct c_wl_object *popup;
  struct c_wl_object *parent_surface;
  struct c_wl_object *positioner;

  C_WL_CHECK_IF_NOT_REGISTERED(args[1].n, popup);
  C_WL_CHECK_IF_REGISTERED(args[2].o, parent_surface);
  C_WL_CHECK_IF_REGISTERED(args[3].o, positioner);

  struct c_xdg_surface *xdg_surface = c_wl_object_data_get2(self);
  struct c_xdg_surface *xdg_surface_parent = c_wl_object_data_get2(parent_surface);
  struct c_xdg_positioner *xdg_positioner = c_wl_object_data_get2(positioner);
  
  xdg_surface->surface->role = C_WL_SURFACE_ROLE_XDG_POPUP;
  xdg_surface->parent = xdg_surface_parent;
  xdg_surface->popup.id = args[1].n;
  memcpy(&xdg_surface->popup.positioner, xdg_positioner, sizeof(*xdg_positioner));


  if (!xdg_surface_parent->children)
    xdg_surface_parent->children = c_list_new();


  c_list_push(xdg_surface_parent->children, xdg_surface, 0);
  c_wl_object_add(conn, args[1].n, self->version, c_wl_interface_get("xdg_popup"), self->data);
  return 0;
}


int xdg_popup_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *xdg_surface = c_wl_object_data_get(conn, args[0].o);

  memset(&xdg_surface->popup, 0, sizeof(xdg_surface->popup));
  xdg_surface->surface->role = 0;

  c_list_remove_ptr(&xdg_surface->parent->children, xdg_surface);

  c_wl_object_del(conn, args[0].o);
  return 0;
}
