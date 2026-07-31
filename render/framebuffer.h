#ifndef CUTS_RENDER_BUFFER_H
#define CUTS_RENDER_BUFFER_H

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#include "render/renderer.h"

struct c_framebuffer {
  int drm_fd;
  EGLDisplay display;
  uint32_t drm_fb_id;
  EGLImageKHR egl_image;
  GLuint texture;
  GLuint fbo;
  GLuint rbo;
};

struct c_framebuffer *c_framebuffer_create(struct c_renderer *renderer,
                                           int drm_fd,
                                           struct gbm_device *gbm_device,
                                           uint32_t width, uint32_t height);
void c_framebuffer_destroy(struct c_framebuffer *buf);

#endif
