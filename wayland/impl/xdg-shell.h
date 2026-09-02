#ifndef CUTS_WAYLAND_IMPL_XDG_SHELL_H
#define CUTS_WAYLAND_IMPL_XDG_SHELL_H

#include "wayland/types.h"
#include "wayland/proto/xdg-shell.h"
#include "util/list.h"

struct c_xdg_positioner {
  struct c_wl_object *obj;
  c_wl_uint token;

  c_wl_int x; 
  c_wl_int y; 

  c_wl_int width;
  c_wl_int height;

  enum xdg_positioner_gravity_enum               gravity;
  enum xdg_positioner_anchor_enum                anchor;
  enum xdg_positioner_constraint_adjustment_enum constraint_adjustment;

  struct {
    c_wl_int x; 
    c_wl_int y; 

    c_wl_int width;
    c_wl_int height;
  } anchor_rect;
};

enum c_xdg_surface_state_commited {
  C_XDG_SURFACE_STATE_MAX_SIZE = 1 << 0,
  C_XDG_SURFACE_STATE_MIN_SIZE = 1 << 1,
  C_XDG_SURFACE_STATE_GEOM     = 1 << 2,
};

struct c_xdg_surface_state {
  uint32_t commited;

  struct { c_wl_int width, height; } max;
  struct { c_wl_int width, height; } min;

  struct {
    c_wl_int x, y;
    c_wl_int width, height;
  } geo;
  
};

struct c_xdg_surface {
  struct c_wl_object *obj;

  c_wl_uint serial;
  c_wl_uint acked_serial;

  struct c_xdg_surface_state pending, active;
  
  struct {
    struct c_wl_object *obj;

    char *title;
    char *app_id;
  } toplevel;

  struct c_wl_surface *surface;

  struct {
    struct c_xdg_positioner *positioner;
    c_wl_int x, y;
  } popup;

  c_list *children;
  struct c_xdg_surface *parent;
};

void c_xdg_surface_state_apply(struct c_xdg_surface *surface);

#endif
