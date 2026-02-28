#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "render/render.h"
#include "render/egl.h"
#include "render/vulkan.h"

#include "wayland/server.h"

#include "backend/drm.h"
#include "util/log.h"

int drm_add_fb_egl(struct c_render *render) {
  eglSwapBuffers(render->egl->display, render->egl->surface);
  struct gbm_bo *bo = gbm_surface_lock_front_buffer(render->gbm_surface);
  if (!bo) { 
    c_log(C_LOG_ERROR, "gbm_surface_lock_front_buffer failed: %s", strerror(errno));
    return -1; 
  }
  render->gbm_bo_next = bo;

  uint32_t width = gbm_bo_get_width(bo);
  uint32_t height = gbm_bo_get_height(bo);
  uint32_t format = gbm_bo_get_format(bo);
  uint32_t handles[4] = {gbm_bo_get_handle(bo).u32};
  uint32_t pitches[4] = {gbm_bo_get_stride(bo)};
  uint32_t offsets[4] = {gbm_bo_get_offset(bo, 0)};
  uint64_t modifier[4] = {gbm_bo_get_modifier(bo)};

  if (drmModeAddFB2WithModifiers(render->drm->fd, 
                    width, height, format, 
                    handles, pitches, offsets, modifier, &render->drm->buf_id, 0) != 0) {
    c_log(C_LOG_ERROR, "drmModeAddFB2 failed: %s", strerror(errno));
    return -1;
  }

  return 0;

}

int drm_add_fb_vulkan(struct c_render *render) {}

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *user_data) {
  struct c_render *render = user_data;
  if (render->gbm_bo)
    gbm_surface_release_buffer(render->gbm_surface, render->gbm_bo);
   
  render->gbm_bo = render->gbm_bo_next;
  render->gbm_bo_next = NULL;
      
  render->drm->connector->waiting_for_flip = 0;
  printf("waiting_for_flip: %d\n", render->drm->connector->waiting_for_flip);

}

int c_render_handle_event(struct c_render *render) {
  drmEventContext ctx = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler,
  };

  if (drmHandleEvent(render->drm->fd, &ctx) < 0) {
    perror("c_drm_backend_page_flip drmHandleEvent");
    return -1;
  }

  return 0;
}

int c_render_new_page_flip(struct c_render *render) {
  // if (!render->drm->connector->waiting_for_flip) {
  //   if (drm_add_fb(render) != 0) return -1;
  //   if (drmModePageFlip(render->drm->fd, render->drm->connector->crtc_id, 
  //                       render->drm->buf_id, DRM_MODE_PAGE_FLIP_EVENT, render) != 0) {
  //     c_log(C_LOG_ERROR, "drmModePageFlip failed: %s", strerror(errno));
  //     return -1;
  //   }
  //   render->drm->connector->waiting_for_flip = 1;
  // }
  //
  // printf("waiting_for_flip: %d\n", render->drm->connector->waiting_for_flip);
  return 0;
}


static int gbm_init(struct c_render *render) {
  struct c_drm_backend *drm = render->drm;
  render->gbm_device = gbm_create_device(drm->fd);
  if (!render->gbm_device) {
    c_log(C_LOG_ERROR, "gbm_create_device failed: %s", strerror(errno));
    return -1;
  }

  uint32_t width =  drm->connector->mode->hdisplay;
  uint32_t height = drm->connector->mode->vdisplay;
  render->gbm_surface = gbm_surface_create(render->gbm_device, 
                                           width, height, GBM_FORMAT_XRGB8888, 
                                           GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    
  if (!render->gbm_surface) {
    c_log(C_LOG_ERROR, "gbm_surface_create failed");
    return -1;
  }

  // render->gbm_bo = gbm_bo_create(render->gbm_device, width, height, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  //
  // if (!render->gbm_bo) {
  //   gbm_device_destroy(render->gbm_device);
  //   c_log(C_LOG_ERROR, "gbm_bo_create failed");
  //   return -1;
  // }

  return 0;
}



void c_render_free(struct c_render *render) {
  if (render->egl) c_egl_free(render->egl);
  if (render->vk) c_vulkan_free(render->vk);

  if (render->gbm_bo)      gbm_bo_destroy(render->gbm_bo);
  if (render->gbm_bo_next) gbm_bo_destroy(render->gbm_bo_next);
  if (render->gbm_surface) gbm_surface_destroy(render->gbm_surface);
  if (render->gbm_device)  gbm_device_destroy(render->gbm_device);

  if (render->drm) c_drm_backend_free(render->drm);

  free(render);
}

struct c_render *c_render_init(struct c_drm_backend *drm) {
  int ret;
  struct c_render *render = calloc(1, sizeof(struct c_render));
  if (!render) 
    return NULL;

  render->drm = drm;
  ret = gbm_init(render); 
  if (ret != 0) goto error;

  render->vk = c_vulkan_init(drm->fd);
  if (!render->vk) goto error;

  // render->egl = c_egl_init(render->gbm_device, render->gbm_surface);
  // if (!render->egl) goto error;

  // if (drm_add_fb(render) != 0) goto error;
  // if (drmModeSetCrtc(drm->fd, drm->connector->crtc_id, drm->buf_id, 
  //                  0, 0, &drm->connector->id, 1, drm->connector->mode) != 0) {
  //   c_log(C_LOG_ERROR, "drmModeSetCrtc failed: %s", strerror(errno));
  //   goto error;
  // }
  // c_render_new_page_flip(render);

  struct c_wl_interface *zwp_linux_dmabuf_v1 = c_wl_interface_get("zwp_linux_dmabuf_v1");
  assert(zwp_linux_dmabuf_v1);

  zwp_linux_dmabuf_v1->requests[2].handler_data = render; // zwp_linux_dmabuf_v1_get_default_feedback
  zwp_linux_dmabuf_v1->requests[3].handler_data = render; // zwp_linux_dmabuf_v1_get_surface_feedback
    

  struct c_wl_interface *zwp_linux_buffer_params_v1 = c_wl_interface_get("zwp_linux_buffer_params_v1");
  assert(zwp_linux_buffer_params_v1);

  zwp_linux_buffer_params_v1->requests[2].handler_data = render->egl; // zwp_linux_buffer_params_v1_create
  zwp_linux_buffer_params_v1->requests[3].handler_data = render->egl; // zwp_linux_buffer_params_v1_create_immed

  return render;

error:
  c_render_free(render);
  return NULL;
}
