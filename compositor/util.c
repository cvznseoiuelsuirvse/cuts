#include "render/types.h"
#include "wayland/impl/wayland.h"
#include "wayland/impl/viewporter.h"

#include "compositor/util.h"

void get_surface_raw_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  if (surface->active.buffer->dma) {
    *width = surface->active.buffer->dma->width;
    *height = surface->active.buffer->dma->height;
  } else {
    *width = surface->active.buffer->shm->width;
    *height = surface->active.buffer->shm->height;
  }
}

void get_surface_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  double scale;
  if (surface->fscale)
    scale = surface->fscale / 120.0f;
  else
    scale = surface->active.scale;

  if (surface->active.buffer->dma) {
    *width = surface->active.buffer->dma->width / scale;
    *height = surface->active.buffer->dma->height / scale;
  } else {
    *width = surface->active.buffer->shm->width / scale;
    *height = surface->active.buffer->shm->height / scale;
  }
}

void get_surface_size(struct c_wl_surface *surface, double *width, double *height) {
  int32_t buf_w, buf_h;
  get_surface_buf_size(surface, &buf_w, &buf_h);

  *width = buf_w;
  *height = buf_h;

  if (!surface->viewport) return;

  struct c_wp_viewport_state *vp = &surface->viewport->active;

  if (vp->src.width > 0 && vp->src.height > 0) {
    *width = vp->src.width;
    *height = vp->src.height;
  }

  if (vp->dst.width > 0 && vp->dst.height > 0) {
    *width = vp->dst.width;
    *height = vp->dst.height;
  }
}
