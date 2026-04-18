#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <unistd.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <fcntl.h>
#include <inttypes.h>

#include "render/gl/egl.h"
#include "render/gl/gles.h"
#include "util/log.h"

void c_render_buffer_destroy(struct c_render *render, struct c_render_buffer *buf) {
  if (buf->texture) glDeleteTextures(1, &buf->texture);
  if (buf->drm_fb_id) drmModeRmFB(render->drm->fd, buf->drm_fb_id);
  if (buf->egl_image) eglDestroyImage(render->egl->display, buf->egl_image);
  free(buf);
}

struct c_render_buffer *c_render_buffer_create(struct c_render *render, uint32_t width, uint32_t height) { 
  int success = 1;
  struct c_render_buffer *buf = calloc(1, sizeof(*buf));
  if (!buf) {
    return NULL;
  }

  uint32_t format = DRM_FORMAT_XRGB8888;

  struct gbm_bo *gbm_bo = gbm_bo_create(render->drm->gbm_device, width, height, format, 
      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if (!gbm_bo) {
    c_log_errno(C_LOG_ERROR, "gbm_bo_create failed");
    c_render_buffer_destroy(render, buf);
    return NULL;
  }

  uint32_t handles[C_DMABUF_MAX_PLANES] = {0};
  uint32_t pitches[C_DMABUF_MAX_PLANES] = {0};
  uint32_t offsets[C_DMABUF_MAX_PLANES] = {0};
  uint64_t modifier[C_DMABUF_MAX_PLANES] = {0};

  struct c_dmabuf_params params = {
    .width = width,
    .height = height,
    .drm_format = format,
    .modifier = gbm_bo_get_modifier(gbm_bo),
    .n_planes = gbm_bo_get_plane_count(gbm_bo),
    .planes = calloc(1, sizeof(*params.planes) * C_DMABUF_MAX_PLANES),
  };

  for (size_t i = 0; i < params.n_planes; i++) {
    struct c_dmabuf_plane *plane = &params.planes[i];
    plane->fd = gbm_bo_get_fd(gbm_bo);
    plane->stride = gbm_bo_get_stride(gbm_bo);
    plane->offset = gbm_bo_get_offset(gbm_bo, i);

    handles[i] = gbm_bo_get_handle(gbm_bo).u32;
    pitches[i] = plane->stride;
    offsets[i] = plane->offset;
    modifier[i] = params.modifier;
  }

  int ret;
  ret = drmModeAddFB2WithModifiers(render->drm->fd, width, height, format, handles, pitches, offsets, modifier, &buf->drm_fb_id, DRM_MODE_FB_MODIFIERS);

  if (ret < 0) {
    c_log_errno(C_LOG_WARNING, "falling back to drmModeAddFB2. "
        "drmModeAddFB2WithModifiers(format=0x%"PRIX32", modifier=0x%"PRIX64") failed", format, modifier[0]);

    if (drmModeAddFB2(render->drm->fd, width, height, format, handles, pitches, offsets, &buf->drm_fb_id, 0) < 0) {
      c_log_errno(C_LOG_ERROR, "drmModeAddFB2(format=0x%"PRIX32") failed", format);
      success = 0;
      goto out;

    }
  }

  buf->egl_image = c_egl_create_image_from_dmabuf(render->egl, &params);

  for (uint32_t i = 0; i < params.n_planes; i++) {
    close(params.planes[i].fd);
  } 

  if (!buf->egl_image) {
    success = 0;
    goto out;
  }

 
  GLenum status;
  GLenum tex_target = GL_TEXTURE_2D;
  
  glGenFramebuffers(1, &buf->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);

  glGenTextures(1, &buf->texture);
  glBindTexture(tex_target, buf->texture);

  render->gl->proc.glEGLImageTargetTexture2DOES(tex_target, buf->egl_image);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex_target, buf->texture, 0);

  if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    c_log(C_LOG_ERROR, "framebuffer incomplete: 0x%04X", status);
    success = 0;
    goto out;
  }

  glBindTexture(tex_target, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

out:
  free(params.planes);
  params.planes = NULL;

  gbm_bo_destroy(gbm_bo);
  if (!success) {
    c_render_buffer_destroy(render, buf);
    buf = NULL;
  }

  return buf;
}
