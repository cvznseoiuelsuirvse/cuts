#ifndef CUTS_RENDER_BUFFER_H
#define CUTS_RENDER_BUFFER_H

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

struct c_renderer_buffer {
  uint32_t    drm_fb_id;
  EGLImageKHR egl_image;
  GLuint      texture;
  GLuint      fbo;
  GLuint      rbo;
};

struct c_renderer;
struct c_renderer_buffer *c_renderer_buffer_create(struct c_renderer *render, uint32_t width, uint32_t height);
void c_renderer_buffer_destroy(struct c_renderer *render, struct c_renderer_buffer *buf);

#endif
