#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "render/gl/egl.h"
#include "render/gl/gles.h"
#include "util/log.h"



void c_render_buffer_destroy(struct c_render *render, struct c_render_buffer *buf) {
  if (buf->texture) glDeleteTextures(1, &buf->texture);
  if (buf->drm_fb_id) drmModeRmFB(render->drm->fd, buf->drm_fb_id);
  if (buf->egl_image) eglDestroyImage(render->egl->display, buf->egl_image);
  if (buf->gbm_bo) gbm_bo_destroy(buf->gbm_bo);
  free(buf);
}

struct c_render_buffer *c_render_buffer_create(struct c_render *render, 
                                               uint32_t width, uint32_t height, uint32_t format, 
                                               uint64_t *modifiers, size_t n_modifiers) { 
  struct c_render_buffer *buf = calloc(1, sizeof(*buf));
  if (!buf) {
    return NULL;
  }

  int mods = 1;
  uint32_t usage = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;

  if (n_modifiers == 0) goto gbm_no_modifiers;

  buf->gbm_bo = gbm_bo_create_with_modifiers(render->gbm_device, width, height, format, modifiers, n_modifiers);
  if (!buf->gbm_bo) {
    c_log_errno(C_LOG_WARNING, "gbm_bo_create_with_modifiers failed. falling back to gbm_bo_create");
    if (n_modifiers == 1 && modifiers[0] == DRM_FORMAT_MOD_LINEAR)
      usage |= GBM_BO_USE_LINEAR;

gbm_no_modifiers:
    buf->gbm_bo = gbm_bo_create(render->gbm_device, width, height, format, usage);
    if (!buf->gbm_bo) {
      c_log_errno(C_LOG_ERROR, "gbm_bo_create failed");
      c_render_buffer_destroy(render, buf);
      return NULL;
    }

    mods = 0;
  }
  
  int n_planes = gbm_bo_get_plane_count(buf->gbm_bo);
  uint32_t handles[n_planes];
  uint32_t pitches[n_planes];
  uint32_t offsets[n_planes];
  uint32_t modifier[n_planes];

  struct c_dmabuf_params params = {
    .width = width,
    .height = height,
    .drm_format = format,
    .modifier = mods ? gbm_bo_get_modifier(buf->gbm_bo) : DRM_FORMAT_MOD_LINEAR,
    .n_planes = n_planes,
  };

  for (int i = 0; i < n_planes; i++) {
    struct c_dmabuf_plane *plane = &params.planes[i];
    handles[i] = gbm_bo_get_handle_for_plane(buf->gbm_bo, i).u32;
    modifier[i] = params.modifier;

    plane->fd = gbm_bo_get_fd_for_plane(buf->gbm_bo, i);

    plane->stride = gbm_bo_get_stride_for_plane(buf->gbm_bo, i);
    pitches[i] = plane->stride;

    plane->offset = gbm_bo_get_offset(buf->gbm_bo, i);
    offsets[i] = plane->offset;

  }

  buf->egl_image = c_egl_create_image_from_dmabuf(render->egl, &params);
  if (!buf->egl_image) {
    goto error;
  }

  if (mods) {
    if (drmModeAddFB2WithModifiers(render->drm->fd, 
                               width, height, format, handles, pitches, offsets, modifiers, 
                               &buf->drm_fb_id, 0) < 0) {
      c_log_errno(C_LOG_ERROR, "drmModeAddFB2WithModifiers failed");
      goto error;
    }
  } else {
    if (drmModeAddFB2(render->drm->fd, width, height, format, handles, pitches, offsets, &buf->drm_fb_id, 0) < 0) {
      c_log_errno(C_LOG_ERROR, "drmModeAddFB2 failed");
      goto error;
    }
  }


  glGenTextures(1, &buf->texture);
  glBindTexture(GL_TEXTURE_2D, buf->texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  render->gl->proc.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buf->egl_image);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &buf->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    c_log(C_LOG_ERROR, "failed to bind framebuffer: 0x%x", status);
    goto error;
  }
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buf->texture, 0);

  return buf;

error:
  c_render_buffer_destroy(render, buf);
  return NULL;
}
