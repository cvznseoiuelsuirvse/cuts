#ifndef CUTS_WAYLAND_IMPL_VIEWPORTER_H
#define CUTS_WAYLAND_IMPL_VIEWPORTER_H

#include "wayland/types.h"

enum c_wp_viewport_state_commited {
  C_WP_VIEWPORT_STATE_SRC = 1 << 0,
  C_WP_VIEWPORT_STATE_DST = 1 << 1,
};

struct c_wp_viewport_state {
  uint32_t commited;

  struct { 
    double x, y;
    double width, height;
  } src;

  struct {
    c_wl_int width, height;
  } dst;
};

struct c_wp_viewport {
  struct c_wl_object *obj;
  struct c_wl_surface *surface;

  struct c_wp_viewport_state pending, active;
};

int c_wp_viewport_state_apply(struct c_wp_viewport *viewport);

#endif
