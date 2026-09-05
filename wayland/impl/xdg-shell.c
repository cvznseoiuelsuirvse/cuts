#include <stdlib.h>
#include <string.h>

#include "wayland/types.h"

#include "wayland/display.h"
#include "wayland/proto/xdg-shell.h"
#include "wayland/impl/xdg-shell.h"
#include "wayland/proto/wayland.h"
#include "wayland/impl/wayland.h"

#include "util/mem.h"
#include "util/log.h"

static void calc_popup_coords(struct c_xdg_surface *surface, int32_t *x, int32_t *y) {
  struct c_xdg_positioner *popup = surface->popup.positioner;

  int32_t anchor_x, anchor_y;
  int32_t dx, dy;

  switch (popup->anchor) {
    case XDG_POSITIONER_ANCHOR_RIGHT:
    case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      anchor_x = popup->anchor_rect.width;
      break;

    case XDG_POSITIONER_ANCHOR_TOP:
    case XDG_POSITIONER_ANCHOR_BOTTOM:
      anchor_x = popup->anchor_rect.width / 2;
      break;

    default:
      anchor_x = 0;
      break;
  }

  switch (popup->anchor) {
    case XDG_POSITIONER_ANCHOR_BOTTOM:
    case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      anchor_y = popup->anchor_rect.height;
      break;

    case XDG_POSITIONER_ANCHOR_LEFT:
    case XDG_POSITIONER_ANCHOR_RIGHT:
      anchor_y = popup->anchor_rect.height / 2;
      break;

    default:
      anchor_y = 0;
      break;
  }

  switch (popup->gravity) {
    case XDG_POSITIONER_GRAVITY_LEFT:
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
    case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
      dx = -popup->width;
      break;

    case XDG_POSITIONER_GRAVITY_TOP:
    case XDG_POSITIONER_GRAVITY_BOTTOM:
      dx = -popup->width / 2;
      break;

    default:
      dx = 0;
      break;
  }

  switch (popup->gravity) {
    case XDG_POSITIONER_GRAVITY_TOP:
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
    case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
      dy = -popup->height;
      break;

    case XDG_POSITIONER_GRAVITY_LEFT:
    case XDG_POSITIONER_GRAVITY_RIGHT:
      dy = -popup->height / 2;
      break;

    default:
      dy = 0;
      break;
  }

  *x = popup->anchor_rect.x + popup->x + anchor_x + dx;
  *y = popup->anchor_rect.y + popup->y + anchor_y + dy;
}

void c_xdg_surface_state_apply(struct c_xdg_surface *surface) {
  struct c_xdg_surface_state *p = &surface->pending;
  struct c_xdg_surface_state *a = &surface->active;

  if (p->commited & C_XDG_SURFACE_STATE_MAX_SIZE) {
    a->max = p->max;
    p->commited &= ~C_XDG_SURFACE_STATE_MAX_SIZE;
  }
  if (p->commited & C_XDG_SURFACE_STATE_MIN_SIZE) {
    a->min = p->min;
    p->commited &= ~C_XDG_SURFACE_STATE_MIN_SIZE;
  }
  if (p->commited & C_XDG_SURFACE_STATE_GEO) {
    a->geo = p->geo;
    p->commited &= ~C_XDG_SURFACE_STATE_GEO;
  }
}

int xdg_wm_base_get_xdg_surface(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id xdg_surface_id = args[1].n;
  struct c_wl_object *xdg_surface_o;
  C_WL_CHECK_IF_NOT_REGISTERED(xdg_surface_id, xdg_surface_o);

  c_wl_object_id wl_surface_id = args[2].o;
  struct c_wl_object *wl_surface_o;
  C_WL_CHECK_IF_REGISTERED(wl_surface_id, wl_surface_o);

  struct c_wl_surface *wl_surface = wl_surface_o->data;

  struct c_xdg_surface *xdg_surface = c_malloc(sizeof(*xdg_surface));
  if (!xdg_surface) 
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_NO_MEMORY, "failed to calloc c_xdg_surface");


  xdg_surface->surface = wl_surface;
  c_ref(wl_surface);

  wl_surface->xdg_surface = xdg_surface;
  c_ref(xdg_surface);

  xdg_surface->obj =
      c_wl_object_add(conn, xdg_surface_id, self->version,
                      c_wl_interface_get("xdg_surface"), xdg_surface);

  return 0;
}

int xdg_wm_base_destroy(struct c_wl_connection *conn, c_wl_args args) {
  c_wl_object_del(conn, args[0].o);
  return 0;
}

int xdg_surface_ack_configure(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_xdg_surface *xs = self->data;

  c_wl_uint serial = args[1].u;

  if (!serial) {
    c_wl_error_set_and_return(self->id, XDG_SURFACE_ERROR_INVALID_SERIAL, "invalid serial");
    return -1;
  }
  
  xs->ack_configure = serial;
  return 0;
}

int xdg_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id xdg_surface_id = args[0].o;
  struct c_xdg_surface *xdg_surface = c_wl_object_get(conn, xdg_surface_id)->data;

  if (xdg_surface->children) {
    struct c_xdg_surface *s;
    c_list_for_each(xdg_surface->children, s) {
      s->parent = NULL;
      c_unref(s);
      c_unref(xdg_surface);
    }
    c_list_destroy(xdg_surface->children);
  }

  if (xdg_surface->surface) {
    xdg_surface->surface->xdg_surface = NULL;
    c_unref(xdg_surface->surface);

    xdg_surface->surface = NULL;
    c_unref(xdg_surface);
  }

  if (xdg_surface->popup.positioner) {
    c_unref(xdg_surface->popup.positioner);
    xdg_surface->popup.positioner = NULL;
  }

  c_wl_object_del(conn, xdg_surface_id);
  return 0;
}

int xdg_surface_set_window_geometry(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;

  c_wl_int x = args[1].i;
  c_wl_int y = args[2].i;
  c_wl_int width = args[3].i;
  c_wl_int height = args[4].i;

  surface->pending.geo.x = x;
  surface->pending.geo.y = y;
  surface->pending.geo.width = width;
  surface->pending.geo.height = height;

  surface->pending.commited |= C_XDG_SURFACE_STATE_GEO;

  return 0;
}

int xdg_surface_get_toplevel(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id xdg_toplevel_id = args[1].n;
  struct c_wl_object *xdg_toplevel;
  C_WL_CHECK_IF_NOT_REGISTERED(xdg_toplevel_id, xdg_toplevel);

  struct c_xdg_surface *xdg_surface = self->data;
  if (xdg_surface->surface->role)
    c_wl_error_set_and_return(self->id, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "specified surface already has a role");

  xdg_surface->surface->role = C_WL_SURFACE_ROLE_XDG_TOPLEVEL;
  c_ref(xdg_surface);

  xdg_surface->toplevel.obj =
      c_wl_object_add(conn, xdg_toplevel_id, self->version,
                      c_wl_interface_get("xdg_toplevel"), xdg_surface);

  return 0;
}

int xdg_toplevel_set_app_id(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;
  if (surface->toplevel.app_id)
    free(surface->toplevel.app_id);
  surface->toplevel.app_id = strdup(args[1].s);
  return 0;
}

int xdg_toplevel_set_title(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;
  if (surface->toplevel.title)
    free(surface->toplevel.title);

  surface->toplevel.title = strdup(args[1].s);
  return 0;
}

int xdg_toplevel_set_min_size(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;

  c_wl_int min_width = args[1].i;
  c_wl_int min_height = args[2].i;

  surface->pending.min.width = min_width;
  surface->pending.min.height = min_height;

  surface->pending.commited |= C_XDG_SURFACE_STATE_MIN_SIZE;
  return 0;
}

int xdg_toplevel_set_max_size(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;

  c_wl_int max_width = args[1].i;
  c_wl_int max_height = args[2].i;

  surface->pending.max.width =  max_width;
  surface->pending.max.height = max_height;

  surface->pending.commited |= C_XDG_SURFACE_STATE_MAX_SIZE;
  return 0;
}

int xdg_toplevel_set_parent(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  struct c_xdg_surface *xdg_surface = self->data;
  c_wl_object_id xdg_parent_id = args[1].o;
  
  if (self->id == xdg_parent_id)
    c_wl_error_set_and_return(args[0].o, XDG_TOPLEVEL_ERROR_INVALID_PARENT,
                              "parent and child cannot be the same objects");

  if (!xdg_parent_id) {
    if (xdg_surface->parent) {
      c_list_remove(&xdg_surface->parent->children, xdg_surface);
      c_unref(xdg_surface);
    }

    xdg_surface->parent = NULL;
    return 0;
  }

  struct c_xdg_surface *xdg_parent = c_wl_object_get(conn, xdg_parent_id)->data;

  if (!xdg_parent->children)
    xdg_parent->children = c_list_new();

  c_list_push(xdg_parent->children, xdg_surface, 0);
  c_ref(xdg_surface);

  xdg_surface->parent = xdg_parent;
  c_ref(xdg_parent);

  return 0;
}

int xdg_toplevel_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *xdg_surface = c_wl_self(conn, args)->data;
  struct c_xdg_surface *parent = xdg_surface->parent;

  if (parent) {
    c_list_remove(&parent->children, xdg_surface);
    // c_unref(xdg_surface);
    c_unref(parent);
  }

  if (xdg_surface->surface)         xdg_surface->surface->role = 0;
  if (xdg_surface->toplevel.title)  free(xdg_surface->toplevel.title);
  if (xdg_surface->toplevel.app_id) free(xdg_surface->toplevel.app_id);

  c_wl_object_del(conn, args[0].o);
  return 0;
}


int xdg_wm_base_create_positioner(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_object *positioner;
  C_WL_CHECK_IF_NOT_REGISTERED(args[1].n, positioner);

  struct c_xdg_positioner *p = c_malloc(sizeof(*p));
  if (!p) {
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_NO_MEMORY, "failed to calloc c_xdg_positioner");
    return -1;
  }

  p->obj = c_wl_object_add(conn, args[1].n, self->version, c_wl_interface_get("xdg_positioner"), p);
  return 0;
}

int xdg_positioner_set_size(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_self(conn, args)->data;
  p->width = args[1].i;
  p->height = args[2].i;
  return 0;
}

int xdg_positioner_set_offset(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_self(conn, args)->data;
  p->x = args[1].i;
  p->y = args[2].i;
  return 0;
}

int xdg_positioner_set_gravity(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_self(conn, args)->data;
  p->gravity = args[1].e;
  return 0;
}

int xdg_positioner_set_constraint_adjustment(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_self(conn, args)->data;
  p->constraint_adjustment = args[1].e;
  return 0;
}

int xdg_positioner_set_anchor(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_self(conn, args)->data;
  p->anchor =  args[1].e;
  return 0;
}

int xdg_positioner_set_anchor_rect(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_positioner *p = c_wl_self(conn, args)->data;
  p->anchor_rect.x =       args[1].i;
  p->anchor_rect.y =       args[2].i;
  p->anchor_rect.width =   args[3].i;
  p->anchor_rect.height =  args[4].i;
  return 0;
}

int xdg_positioner_destroy(struct c_wl_connection *conn, union c_wl_arg *args) { C_WL_DESTRUCTOR(conn, args); }

int xdg_surface_get_popup(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  c_wl_new_id xdg_popup_id = args[1].n;

  struct c_wl_object *popup;
  struct c_wl_object *parent_surface;
  struct c_wl_object *positioner;

  C_WL_CHECK_IF_NOT_REGISTERED(xdg_popup_id, popup);
  C_WL_CHECK_IF_REGISTERED(args[2].o, parent_surface);
  C_WL_CHECK_IF_REGISTERED(args[3].o, positioner);

  struct c_xdg_surface *xdg_surface = self->data;
  struct c_xdg_surface *xdg_surface_parent = parent_surface->data;
  struct c_xdg_positioner *xdg_positioner = positioner->data;
  
  if (xdg_surface->surface->role)
    c_wl_error_set_and_return(self->id, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "specified surface already has a role");

  xdg_surface->surface->role = C_WL_SURFACE_ROLE_XDG_POPUP;

  xdg_surface->parent = xdg_surface_parent;
  c_ref(xdg_surface_parent);

  xdg_surface->popup.positioner = xdg_positioner;
  c_ref(xdg_positioner);

  if (!xdg_surface_parent->children)
    xdg_surface_parent->children = c_list_new();

  c_list_push(xdg_surface_parent->children, xdg_surface, 0);
  c_ref(xdg_surface);

  c_ref(xdg_surface);
  c_wl_object_add(conn, args[1].n, self->version, c_wl_interface_get("xdg_popup"), xdg_surface);

  calc_popup_coords(xdg_surface, &xdg_surface->popup.x, &xdg_surface->popup.y);

  c_log(C_LOG_DEBUG, "popup#%d (%d %d %d) %dx%d x=%d y=%d", xdg_popup_id, 
      xdg_positioner->anchor, xdg_positioner->gravity, xdg_positioner->constraint_adjustment,
      xdg_positioner->width, xdg_positioner->height,
      xdg_surface->popup.x, xdg_surface->popup.y);

  xdg_popup_configure(conn, xdg_popup_id, xdg_surface->popup.x,
                      xdg_surface->popup.y, xdg_positioner->width,
                      xdg_positioner->height);

  xdg_surface_configure(conn, xdg_surface->obj->id, c_wl_serial());

  return 0;
}

int xdg_popup_reposition(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_xdg_surface *xdg_surface = self->data;

  struct c_wl_object *positioner;
  C_WL_CHECK_IF_REGISTERED(args[1].o, positioner);
  struct c_xdg_positioner *xdg_positioner = positioner->data;

  c_wl_uint token = args[2].u;

  memcpy(&xdg_surface->popup, xdg_positioner, sizeof(*xdg_positioner));
  calc_popup_coords(xdg_surface, &xdg_surface->popup.x, &xdg_surface->popup.y);

  xdg_popup_repositioned(conn, self->id, token);

  xdg_popup_configure(conn, self->id, xdg_surface->popup.x,
                      xdg_surface->popup.y, xdg_positioner->width,
                      xdg_positioner->height);

  xdg_surface_configure(conn, xdg_surface->obj->id, c_wl_serial());
  return 0;
}


int xdg_popup_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_xdg_surface *xdg_surface = c_wl_self(conn, args)->data;

  xdg_surface->surface->role = 0;

  c_list_remove(&xdg_surface->parent->children, xdg_surface);
  c_unref(xdg_surface);
  c_unref(xdg_surface->parent);

  c_unref(xdg_surface->popup.positioner);
  xdg_surface->popup.positioner = NULL;
  xdg_surface->popup.x = 0;
  xdg_surface->popup.y = 0;

  c_wl_object_del(conn, args[0].o);
  return 0;
}
