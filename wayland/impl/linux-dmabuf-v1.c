#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wayland/display.h"
#include "wayland/proto/wayland.h"
#include "wayland/impl/wayland.h"
#include "wayland/proto/linux-dmabuf-v1.h"
#include "wayland/server.h"

#include "render/types.h"
#include "util/mem.h"
#include "util/log.h"

void defer_wl_buffer(void *data);

int zwp_linux_dmabuf_v1_get_surface_feedback(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id zwp_linux_buffer_feedback_v1_id = args[1].n;
  struct c_wl_object *zwp_linux_buffer_feedback_v1;
  C_WL_CHECK_IF_NOT_REGISTERED(zwp_linux_buffer_feedback_v1_id, zwp_linux_buffer_feedback_v1);

  c_wl_object_add(conn, zwp_linux_buffer_feedback_v1_id, self->version, c_wl_interface_get("zwp_linux_dmabuf_feedback_v1"), NULL);
  return 0;
}


int zwp_linux_dmabuf_v1_get_default_feedback(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id zwp_linux_buffer_feedback_v1_id = args[1].n;
  struct c_wl_object *zwp_linux_buffer_feedback_v1;
  C_WL_CHECK_IF_NOT_REGISTERED(zwp_linux_buffer_feedback_v1_id, zwp_linux_buffer_feedback_v1);

  c_wl_object_add(conn, zwp_linux_buffer_feedback_v1_id, self->version, c_wl_interface_get("zwp_linux_dmabuf_feedback_v1"), NULL);
  return 0;
}

int zwp_linux_dmabuf_v1_create_params(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id zwp_linux_buffer_params_v1_id = args[1].n;
  struct c_wl_object *zwp_linux_buffer_params_v1;
  C_WL_CHECK_IF_NOT_REGISTERED(zwp_linux_buffer_params_v1_id, zwp_linux_buffer_params_v1);

  struct c_wl_buffer *buffer = c_malloc(sizeof(*buffer));
  if (!buffer) {
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  c_wl_object_add(conn, zwp_linux_buffer_params_v1_id, self->version,
                  c_wl_interface_get("zwp_linux_buffer_params_v1"), buffer);

  return 0;
}

int zwp_linux_dmabuf_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }

int zwp_linux_dmabuf_feedback_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }

int zwp_linux_buffer_params_v1_add(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_buffer *buffer = c_wl_self(conn, args)->data;
  if (!buffer->dma)
    buffer->dma = c_malloc(sizeof(*buffer->dma));

  struct c_dmabuf *dma = buffer->dma;

  c_wl_fd fd            = args[1].F;
  c_wl_uint plane_idx   = args[2].u;
  c_wl_uint offset      = args[3].u;
  c_wl_uint stride      = args[4].u;
  c_wl_uint modifier_hi = args[5].u;
  c_wl_uint modifier_lo = args[6].u;

  if (plane_idx >= 4)
    c_wl_error_set_and_return(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "only 4 planes are supported");

  if (plane_idx != dma->n_planes)
    c_wl_error_set_and_return(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, 
                          "invalid plane_idx %d (expected %d)", plane_idx, dma->n_planes);

  uint64_t modifier = (uint64_t)modifier_hi << 32 | modifier_lo;
  if (dma->modifier > 0 && modifier != dma->modifier)
    c_wl_error_set_and_return(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT, "invalid modifier");

  struct c_dmabuf_plane *plane = &dma->planes[dma->n_planes++];
  plane->fd = fd;
  plane->offset = offset;
  plane->stride = stride;
  dma->modifier = modifier;

  return 0;
}

int zwp_linux_buffer_params_v1_create_immed(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_buffer *buffer = self->data;
  if (!buffer->dma)
    c_wl_error_set_and_return(
        self->id, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
        "params are incomplete (no add requests were called)");

  struct c_dmabuf *dma = buffer->dma;

  struct c_wl_object *wl_buffer;
  c_wl_new_id wl_buffer_id = args[1].n;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_buffer_id, wl_buffer);

  c_wl_int width           = args[2].i;
  c_wl_int height          = args[3].i;
  c_wl_uint format         = args[4].u;
  // enum zwp_linux_buffer_params_v1_flags_enum flags = args[5].e;

  if (width <= 0)
    c_wl_error_set_and_return(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "invalid width");

  if (height <= 0)
    c_wl_error_set_and_return(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "invalid height");

  c_defer(buffer, defer_wl_buffer); 
  dma->drm_format = format;
  dma->width = width;
  dma->height = height;

  buffer->dma = dma;

  buffer->obj = c_wl_object_add(conn, wl_buffer_id, self->version, c_wl_interface_get("wl_buffer"), buffer);
  c_ref(buffer);

  return 0;
}

int zwp_linux_buffer_params_v1_create(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_buffer *buffer = self->data;
  if (!buffer->dma)
    c_wl_error_set_and_return(
        self->id, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
        "params are incomplete (no add requests were called)");

  struct c_dmabuf *dma = buffer->dma;

  c_wl_int width           = args[1].i;
  c_wl_int height          = args[2].i;
  c_wl_uint format         = args[3].u;
  // enum zwp_linux_buffer_params_v1_flags_enum flags = args[4].e;

  if (width <= 0)
    c_wl_error_set_and_return(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "invalid width");

  if (height <= 0)
    c_wl_error_set_and_return(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "invalid height");

  c_defer(buffer, defer_wl_buffer); 

  dma->drm_format = format;
  dma->width = width;
  dma->height = height;

  buffer->dma = dma;

  buffer->obj = c_wl_object_add(conn, C_WL_OBJECT_NEW_SERVER_ID, self->version, c_wl_interface_get("wl_buffer"), buffer);
  c_ref(buffer);

  zwp_linux_buffer_params_v1_created(conn, self->id, buffer->obj->id);
  return 0;
}

int zwp_linux_buffer_params_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }
