#include "render/types.h"
#include "wayland/impl/wayland.h"
#include "wayland/impl/viewporter.h"
#include "wayland/proto/viewporter.h"

#include "compositor/util.h"

void get_surface_raw_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  if (surface->buffer.active->dma) {
    *width = surface->buffer.active->dma->width;
    *height = surface->buffer.active->dma->height;
  } else {
    *width = surface->buffer.active->shm->width;
    *height = surface->buffer.active->shm->height;
  }
}

void get_surface_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  double scale;
  if (surface->fscale)
    scale = surface->fscale / 120.0f;
  else
    scale = surface->scale;

  if (surface->buffer.active->dma) {
    *width = surface->buffer.active->dma->width / scale;
    *height = surface->buffer.active->dma->height / scale;
  } else {
    *width = surface->buffer.active->shm->width / scale;
    *height = surface->buffer.active->shm->height / scale;
  }
}

void get_surface_size(struct c_wl_surface *surface, double *width, double *height) {
  int32_t buf_w, buf_h;
  get_surface_buf_size(surface, &buf_w, &buf_h);

  *width = buf_w;
  *height = buf_h;

  if (!surface->viewport) return;

  struct c_wp_viewport *vp = surface->viewport;

  if (vp->src.width > 0 && vp->src.height > 0) {
    *width = vp->src.width;
    *height = vp->src.height;
  }

  if (vp->dst.width > 0 && vp->dst.height > 0) {
    *width = vp->dst.width;
    *height = vp->dst.height;
  }
}
