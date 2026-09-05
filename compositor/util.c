#include "render/types.h"
#include "wayland/impl/wayland.h"
#include "wayland/impl/viewporter.h"
#include "compositor/util.h"

double get_surface_scale(struct c_wl_surface *surface) {
  if (surface->fscale)
    return surface->fscale / 120.0f;

  return surface->active.scale;
}

void get_surface_raw_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  if (surface->active.buffer->dma) {
    *width = surface->active.buffer->dma->width;
    *height = surface->active.buffer->dma->height;
  } else {
    *width = surface->active.buffer->shm->width;
    *height = surface->active.buffer->shm->height;
  }
}

void get_surface_logical_buf_size(struct c_wl_surface *surface, double *width, double *height) {
  double scale = get_surface_scale(surface);
  if (surface->active.buffer->dma) {
    *width = surface->active.buffer->dma->width / scale;
    *height = surface->active.buffer->dma->height / scale;
  } else {
    *width = surface->active.buffer->shm->width / scale;
    *height = surface->active.buffer->shm->height / scale;
  }
}

void get_surface_size(struct c_wl_surface *surface, double *width, double *height) {
  double l_w, l_h;
  get_surface_logical_buf_size(surface, &l_w, &l_h);

  *width = l_w;
  *height = l_h;
}
