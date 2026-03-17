#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "wayland/types/wayland.h"
#include "wayland/types/linux-dmabuf-v1.h"

#include "wayland/server.h"
#include "wayland/error.h"

#include "backend/drm/drm.h"
#include "render/render.h"

#include "util/log.h"


static int send_feedback(struct c_wl_connection *conn, c_wl_object_id id, c_wl_object_id feedback_id, struct c_render *render) {
  int ret = 0;
  dev_t *dev_id = malloc(sizeof(*dev_id));

  if (c_drm_dev_id(render->drm, dev_id) == -1) 
    return c_wl_error_set(id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to get dev id"); 

  c_wl_array device = {
    sizeof(dev_t), 
    dev_id,
  };


  zwp_linux_dmabuf_feedback_v1_main_device(conn, feedback_id, &device);

  int fd = c_render_get_ft_fd(render);
  if (fd == -1) 
    return c_wl_error_set(feedback_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to get format table id");
  
  zwp_linux_dmabuf_feedback_v1_format_table(conn, feedback_id, fd, render->formats.n_entries * 16);
  zwp_linux_dmabuf_feedback_v1_tranche_target_device(conn, feedback_id, &device);
  zwp_linux_dmabuf_feedback_v1_tranche_flags(conn, feedback_id, 
                                             ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT);

  c_wl_array indices;
  indices.size = render->formats.n_entries * 2;
  indices.data = calloc(render->formats.n_entries, sizeof(uint16_t));

  if (!indices.data) {
    c_wl_error_set(id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to calloc indicies array");
    ret = -1;
    goto out;
  }

  for (size_t i = 0; i < render->formats.n_entries; i++) {
    ((uint16_t *)indices.data)[i] = i;
  }

  zwp_linux_dmabuf_feedback_v1_tranche_formats(conn, feedback_id, &indices);
  free(indices.data);

  zwp_linux_dmabuf_feedback_v1_done(conn, feedback_id);

out:
  free(dev_id);
  return ret;
}

int zwp_linux_dmabuf_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id zwp_linux_buffer_v1_id = args[0].n;
  struct c_wl_object *zwp_linux_buffer_v1;
  C_WL_CHECK_IF_REGISTERED(zwp_linux_buffer_v1_id, zwp_linux_buffer_v1);
  
  c_wl_object_del(conn, zwp_linux_buffer_v1_id);

  return 0;
}

int zwp_linux_dmabuf_v1_get_surface_feedback(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id zwp_linux_buffer_feedback_v1_id = args[1].n;
  struct c_wl_object *zwp_linux_buffer_feedback_v1;
  C_WL_CHECK_IF_NOT_REGISTERED(zwp_linux_buffer_feedback_v1_id, zwp_linux_buffer_feedback_v1);

  c_wl_object_add(conn, zwp_linux_buffer_feedback_v1_id, c_wl_interface_get("zwp_linux_dmabuf_feedback_v1"), NULL);
  return send_feedback(conn, args[0].o, zwp_linux_buffer_feedback_v1_id, userdata);
}


int zwp_linux_dmabuf_v1_get_default_feedback(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id zwp_linux_buffer_feedback_v1_id = args[1].n;
  struct c_wl_object *zwp_linux_buffer_feedback_v1;
  C_WL_CHECK_IF_NOT_REGISTERED(zwp_linux_buffer_feedback_v1_id, zwp_linux_buffer_feedback_v1);

  c_wl_object_add(conn, zwp_linux_buffer_feedback_v1_id, c_wl_interface_get("zwp_linux_dmabuf_feedback_v1"), NULL);
  return send_feedback(conn, args[0].o, zwp_linux_buffer_feedback_v1_id, userdata);
}

int zwp_linux_dmabuf_feedback_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id zwp_linux_buffer_feedback_v1_id = args[0].n;
  struct c_wl_object *zwp_linux_buffer_feedback_v1;
  C_WL_CHECK_IF_REGISTERED(zwp_linux_buffer_feedback_v1_id, zwp_linux_buffer_feedback_v1);
  
  // close(*(int *)zwp_linux_buffer_feedback_v1->data);
  free(zwp_linux_buffer_feedback_v1->data);
  c_wl_object_del(conn, zwp_linux_buffer_feedback_v1_id);

  return 0;
}

int zwp_linux_dmabuf_v1_create_params(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id zwp_linux_buffer_params_v1_id = args[1].n;
  struct c_wl_object *zwp_linux_buffer_params_v1;
  C_WL_CHECK_IF_NOT_REGISTERED(zwp_linux_buffer_params_v1_id, zwp_linux_buffer_params_v1);

  struct c_dmabuf *dma = calloc(1, sizeof(*dma));
  if (!dma) {
    c_log(C_LOG_ERROR, "calloc failed");
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  c_wl_object_add(conn, zwp_linux_buffer_params_v1_id, c_wl_interface_get("zwp_linux_buffer_params_v1"), dma);

  return 0;
}

int zwp_linux_buffer_params_v1_add(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id zwp_linux_buffer_params_v1_id = args[0].o;
  struct c_wl_object *zwp_linux_buffer_params_v1 = c_wl_object_get(conn, zwp_linux_buffer_params_v1_id);
  struct c_dmabuf *dma = zwp_linux_buffer_params_v1->data;

  c_wl_fd   fd =          args[1].F;
  c_wl_uint plane_idx =   args[2].u;
  c_wl_uint offset =      args[3].u;
  c_wl_uint stride =      args[4].u;
  c_wl_uint modifier_hi = args[5].u;
  c_wl_uint modifier_lo = args[6].u;

  if (plane_idx >= C_DMABUF_MAX_PLANES)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "only 4 planes are supported");

  if (plane_idx != dma->n_planes)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, 
                          "invalid plane_idx %d (expected %d)", plane_idx, dma->n_planes);

  uint64_t modifier = modifier_hi << 32 | modifier_lo;
  if (dma->modifier > 0 && modifier != dma->modifier)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT, "invalid modifier");

  struct c_dmabuf_plane *plane = &dma->planes[dma->n_planes++];
  plane->fd = fd;
  plane->offset = offset;
  plane->stride = stride;

  dma->modifier = modifier;

  return 0;
}

int zwp_linux_buffer_params_v1_create_immed(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id zwp_linux_buffer_params_v1_id = args[0].o;
  struct c_wl_object *zwp_linux_buffer_params_v1 = c_wl_object_get(conn, zwp_linux_buffer_params_v1_id);
  struct c_dmabuf *dma = zwp_linux_buffer_params_v1->data;

  c_wl_new_id wl_buffer_id = args[1].n;
  c_wl_int    width =        args[2].i;
  c_wl_int    height =       args[3].i;
  c_wl_uint   format =       args[4].u;
  enum zwp_linux_buffer_params_v1_flags_enum flags = args[5].e;

  if (width <= 0)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "invalid width");

  if (height <= 0)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "invalid height");

  struct c_wl_buffer *c_wl_buffer = calloc(1, sizeof(struct c_wl_buffer));
  if (!c_wl_buffer)
        return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");

  dma->drm_format = format;
  dma->flags = flags;

  c_wl_buffer->id = wl_buffer_id;
  c_wl_buffer->width = width;
  c_wl_buffer->height = height;
  c_wl_buffer->dma = dma;

  struct c_dmabuf_params dmabuf_params = {
    .width = c_wl_buffer->width,
    .height = c_wl_buffer->height,
    .modifier = dma->modifier,
    .drm_format = dma->drm_format,
    .n_planes = dma->n_planes,
  };
  memcpy(dmabuf_params.planes, dma->planes, sizeof(dma->planes));

  if (c_render_import_dmabuf(userdata, &dmabuf_params, dma) == -1) {
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to import dmabuf");
  }

  c_wl_object_add(conn, wl_buffer_id, c_wl_interface_get("wl_buffer"), c_wl_buffer);
  return 0;
}

int zwp_linux_buffer_params_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id zwp_linux_buffer_params_v1_id = args[0].n;
  struct c_wl_object *zwp_linux_buffer_params_v1;
  C_WL_CHECK_IF_REGISTERED(zwp_linux_buffer_params_v1_id, zwp_linux_buffer_params_v1);
  
  c_wl_object_del(conn, zwp_linux_buffer_params_v1_id);

  return 0;
}

