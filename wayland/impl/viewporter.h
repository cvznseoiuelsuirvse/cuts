#ifndef CUTS_WAYLAND_IMPL_VIEWPORTER_H
#define CUTS_WAYLAND_IMPL_VIEWPORTER_H

#include "wayland/types.h"

struct c_wp_viewport {
  struct c_wl_object *obj;
  struct c_wl_surface *surface;

  struct { 
    double x, y;
    double width, height;
  } src;

  struct {
    c_wl_int width, height;
  } dst;
};

#endif
