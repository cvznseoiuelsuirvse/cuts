#ifndef CUTS_WAYLAND_IMPL_WLR_LAYER_SHELL_H
#define CUTS_WAYLAND_IMPL_WLR_LAYER_SHELL_H

#include "wayland/impl/wayland.h"
#include "wayland/proto/wlr-layer-shell-unstable-v1.h"

struct c_zwlr_layer_surface {
  struct c_wl_object *obj;
  struct c_wl_surface *surface;

  c_wl_uint configure;
  c_wl_uint acked_configure;

  enum zwlr_layer_shell_v1_layer_enum    layer;
  enum zwlr_layer_surface_v1_anchor_enum anchor;

  c_wl_uint width, height;
  c_wl_int exclusive_zone;
  enum zwlr_layer_surface_v1_keyboard_interactivity_enum keyboard_interactivity;

  char *namespace;
};

#endif
