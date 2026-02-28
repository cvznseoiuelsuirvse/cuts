#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wayland/types/wayland.h"
#include "wayland/types/linux-dmabuf-v1.h"

#include "wayland/server.h"
#include "wayland/error.h"

#include "backend/drm.h"
#include "render/render.h"
#include "render/egl.h"

#include "util/log.h"
#include "util/helpers.h"


static int send_feedback(struct c_wl_connection *conn, c_wl_object_id id, c_wl_object_id feedback_id, struct c_render *render) {
  int ret = 0;
  dev_t *dev_id = malloc(sizeof(*dev_id));

  if (c_drm_backend_dev_id(render->drm, dev_id) == -1) 
    return c_wl_error_set(id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to get dev id"); 

  c_wl_array device = {
    sizeof(dev_t), 
    dev_id,
  };


  struct c_egl *egl = render->egl;
  zwp_linux_dmabuf_feedback_v1_main_device(conn, feedback_id, &device);

  int fd = c_egl_get_format_table_fd(egl);
  if (fd == -1) {
    c_wl_error_set(feedback_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to get format table id");
    return -1;
  }

  zwp_linux_dmabuf_feedback_v1_format_table(conn, feedback_id, fd, egl->format_table.pairs_n * 16);
  zwp_linux_dmabuf_feedback_v1_tranche_target_device(conn, feedback_id, &device);
  zwp_linux_dmabuf_feedback_v1_tranche_flags(conn, feedback_id, 
                                             ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT);

  c_wl_array indices;
  indices.size = egl->format_table.pairs_n * 2;
  indices.data = calloc(egl->format_table.pairs_n, sizeof(uint16_t));

  if (!indices.data) {
    c_wl_error_set(id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to calloc indicies array");
    ret = -1;
    goto out;
  }

  for (size_t i = 0; i < egl->format_table.pairs_n; i++) {
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
  
  c_wl_object_del(conn, zwp_linux_buffer_feedback_v1_id);

  return 0;
}

int zwp_linux_dmabuf_v1_create_params(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id zwp_linux_buffer_params_v1_id = args[1].n;
  struct c_wl_object *zwp_linux_buffer_params_v1;
  C_WL_CHECK_IF_NOT_REGISTERED(zwp_linux_buffer_params_v1_id, zwp_linux_buffer_params_v1);

  struct c_wl_dmabuf *dma = calloc(1, sizeof(struct c_wl_dmabuf));
  if (!dma) {
    c_log(C_LOG_ERROR, "calloc failed");
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  dma->params_id = zwp_linux_buffer_params_v1_id;
  c_wl_object_add(conn, zwp_linux_buffer_params_v1_id, c_wl_interface_get("zwp_linux_buffer_params_v1"), dma);

  return 0;
}

int zwp_linux_buffer_params_v1_add(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id zwp_linux_buffer_params_v1_id = args[0].o;
  struct c_wl_object *zwp_linux_buffer_params_v1 = c_wl_object_get(conn, zwp_linux_buffer_params_v1_id);
  struct c_wl_dmabuf *dma = zwp_linux_buffer_params_v1->data;

  if (dma->used) 
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "already used");

  c_wl_fd   fd =          args[1].F;
  c_wl_uint plane_idx =   args[2].u;
  c_wl_uint offset =      args[3].u;
  c_wl_uint stride =      args[4].u;
  c_wl_uint modifier_hi = args[5].u;
  c_wl_uint modifier_lo = args[6].u;

  if (plane_idx > 0)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "plane_idx >0 not supported");

  dma->fd = fd;
  dma->plane_idx = plane_idx;
  dma->offset = offset;
  dma->stride = stride;
  dma->modifier = modifier_hi << 32 | modifier_lo;

  return 0;
}

int zwp_linux_buffer_params_v1_create_immed(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id zwp_linux_buffer_params_v1_id = args[0].o;
  struct c_wl_object *zwp_linux_buffer_params_v1 = c_wl_object_get(conn, zwp_linux_buffer_params_v1_id);
  struct c_wl_dmabuf *dma = zwp_linux_buffer_params_v1->data;

  if (dma->used) 
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "already used");

  c_wl_new_id wl_buffer_id = args[1].n;
  c_wl_int    width =        args[2].i;
  c_wl_int    height =       args[3].i;
  c_wl_uint   format =       args[4].u;
  enum zwp_linux_buffer_params_v1_flags_enum flags = args[5].e;

  uint32_t bpp = drm_format_to_bpp(format);
  if (!bpp)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT, "invalid format: %d");

  if (width == 0)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "invalid width");

  if (height == 0)
    return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "invalid height");

  struct c_egl *egl = userdata;
  for (size_t i = 0; i < egl->format_table.pairs_n; i++) {
    struct c_format_table_pair pair = egl->format_table.pairs[i];
    if (pair.modifer == dma->modifier && pair.format == format) {
      struct c_wl_buffer *buffer = calloc(1, sizeof(struct c_wl_buffer));
      if (!buffer)
            return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");

      dma->used = 1;
      dma->format = format;
      dma->flags = flags;

      buffer->id = wl_buffer_id;
      buffer->width = width;
      buffer->height = height;
      buffer->dmabuf = dma;

      if (c_egl_import_dmabuf(egl, buffer) == 1) 
        return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to import dmabuf");

      close(dma->fd);
      dma->fd = 0;
      c_wl_object_add(conn, wl_buffer_id, c_wl_interface_get("wl_buffer"), buffer);
      return 0;
    }
  }
  
  return c_wl_error_set(args[0].o, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT, 
                        "format %d + modifier %lu not found", format, dma->modifier);
}

int zwp_linux_buffer_params_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_new_id zwp_linux_buffer_params_v1_id = args[0].n;
  struct c_wl_object *zwp_linux_buffer_params_v1;
  C_WL_CHECK_IF_REGISTERED(zwp_linux_buffer_params_v1_id, zwp_linux_buffer_params_v1);
  
  c_wl_object_del(conn, zwp_linux_buffer_params_v1_id);

  return 0;
}

