#ifndef CUTS_WAYLAND_IMPL_WLR_LAYER_SHELL_H
#define CUTS_WAYLAND_IMPL_WLR_LAYER_SHELL_H

#include "wayland/impl/wayland.h"
#include "wayland/proto/wlr-layer-shell-unstable-v1.h"

enum c_zwlr_layer_surface_state_commited {
  C_ZWLR_LAYER_SURFACE_STATE_LAYER            = 1 << 0,
  C_ZWLR_LAYER_SURFACE_STATE_ANCHOR           = 1 << 1,
  C_ZWLR_LAYER_SURFACE_STATE_SIZE             = 1 << 2,
  C_ZWLR_LAYER_SURFACE_STATE_EXCLUSIVE_ZONE   = 1 << 3,
  C_ZWLR_LAYER_SURFACE_STATE_KEYBOARD_INTERAC = 1 << 4,
};

struct c_zwlr_layer_surface_state {
  uint32_t commited;

  enum zwlr_layer_shell_v1_layer_enum    layer;
  enum zwlr_layer_surface_v1_anchor_enum anchor;

  c_wl_uint width, height;
  c_wl_int  exclusive_zone;
  enum zwlr_layer_surface_v1_keyboard_interactivity_enum keyboard_interactivity;
};

struct c_zwlr_layer_surface {
  struct c_wl_object *obj;
  struct c_wl_surface *surface;

  struct c_zwlr_layer_surface_state pending, active;

  c_wl_uint configure;
  c_wl_uint acked_configure;

  c_wl_int  exclusive_edge;

  char *namespace;
};

void c_zwlr_layer_surface_state_apply(struct c_zwlr_layer_surface *surface);

#endif
