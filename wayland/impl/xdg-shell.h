#ifndef CUTS_WAYLAND_IMPL_XDG_SHELL_H
#define CUTS_WAYLAND_IMPL_XDG_SHELL_H

#include "wayland/types.h"
#include "util/list.h"

struct c_xdg_positioner {
  struct c_wl_object *obj;
  c_wl_uint token;

  c_wl_int x; 
  c_wl_int y; 

  c_wl_int width;
  c_wl_int height;

  c_wl_uint gravity;
  c_wl_uint anchor;
  c_wl_uint constraint_adjustment;

  struct {
    c_wl_int x; 
    c_wl_int y; 

    c_wl_int width;
    c_wl_int height;
  } anchor_rect;
};

struct c_xdg_surface {
  struct c_wl_object *obj;

  c_wl_int x;
  c_wl_int y;
  c_wl_int width;
  c_wl_int height;
  
  struct {
    struct c_wl_object *obj;

    c_wl_int max_width;
    c_wl_int max_height;
    c_wl_int min_width;
    c_wl_int min_height;

    char *title;
    char *app_id;

  } toplevel;

  struct c_wl_surface *surface;

  struct {
    struct c_xdg_positioner positioner;
    c_wl_int x, y;
  } popup;

  c_list *children;
  struct c_xdg_surface *parent;
};

#endif
