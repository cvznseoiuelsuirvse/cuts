#include <stdlib.h>

#include "wayland/server.h"
#include "wayland/impl/wayland.h"
#include "wayland/proto/wlr-layer-shell-unstable-v1.h"
#include "wayland/impl/wlr-layer-shell-unstable-v1.h"
#include "util/mem.h"

int zwlr_layer_shell_v1_get_layer_surface(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id wlr_layer_surface_id = args[1].n;
  struct c_wl_object *wlr_layer_surface;
  C_WL_CHECK_IF_NOT_REGISTERED(wlr_layer_surface_id, wlr_layer_surface);

  c_wl_object_id wl_surface_id = args[2].o;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_REGISTERED(wl_surface_id, wl_surface);
  struct c_wl_surface *surface = wl_surface->data;

  c_wl_object_id wl_output_id = args[3].o;
  struct c_wl_object *wl_output;
  if (wl_output_id) {
    C_WL_CHECK_IF_REGISTERED(wl_output_id, wl_output);
  }

  c_wl_enum layer = args[4].e;
  c_wl_string namespace = args[5].s;

  if (surface->role)
    c_wl_error_set_and_return(self->id, ZWLR_LAYER_SHELL_V1_ERROR_ROLE,
                              "specifed surface already has a role");

  if (surface->buffer.pending || surface->buffer.active)
    c_wl_error_set_and_return(
        self->id, ZWLR_LAYER_SHELL_V1_ERROR_ALREADY_CONSTRUCTED,
        "specifed surface already has an attached or commited buffer");

  if (layer > ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY)
    c_wl_error_set_and_return(self->id, ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER,
                              "invalid layer");

  struct c_zwlr_layer_surface *layer_surface = c_malloc(sizeof(*layer_surface));
  layer_surface->surface = surface;
  c_ref(surface);
  layer_surface->layer = layer;
  layer_surface->namespace = strdup(namespace);

  layer_surface->obj = c_wl_object_add(
      conn, wlr_layer_surface_id, self->version,
      c_wl_interface_get("zwlr_layer_surface_v1"), layer_surface);

  return 0;
}

int zwlr_layer_shell_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }

int zwlr_layer_surface_v1_set_anchor(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_zwlr_layer_surface *layer_surface = self->data;
  layer_surface->anchor = args[1].e;
  return 0;
}

int zwlr_layer_surface_v1_set_size(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_zwlr_layer_surface *layer_surface = self->data;
  c_wl_uint width  = args[1].u;
  c_wl_uint height = args[2].u;

  if (layer_surface->anchor) {
    if (!(layer_surface->anchor & (ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) && !width)
      c_wl_error_set_and_return(
          self->id, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE,
          "width is 0 while surface isn't anchored to left and right");

    if (!(layer_surface->anchor & (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) && !height)
    c_wl_error_set_and_return(
        self->id, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE,
        "height is 0 while surface isn't anchored to top and bottom");
  }

  layer_surface->width  = args[1].u;
  layer_surface->height = args[2].u;
  return 0;
}

int zwlr_layer_surface_v1_set_exclusive_zone(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_zwlr_layer_surface *layer_surface = self->data;

  layer_surface->exclusive_zone = args[1].i;
  return 0;
}

int zwlr_layer_surface_v1_set_keyboard_interactivity(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_zwlr_layer_surface *layer_surface = self->data;

  layer_surface->keyboard_interactivity = args[1].e;
  return 0;
}

int zwlr_layer_surface_v1_ack_configure(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_zwlr_layer_surface *layer_surface = self->data;
  layer_surface->acked_configure = args[1].u;
  return 0;
}

int zwlr_layer_surface_v1_destroy(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_zwlr_layer_surface *layer_surface = self->data;

  free(layer_surface->namespace);
  c_unref(layer_surface->surface);

  C_WL_DESTRUCTOR(conn, args);
}
