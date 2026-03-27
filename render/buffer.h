#ifndef CUTS_RENDER_BUFFER_H
#define CUTS_RENDER_BUFFER_H

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

struct c_render_buffer {
  uint32_t    drm_fb_id;
  EGLImageKHR egl_image;
  GLuint      texture;
  GLuint      fbo;
};

struct c_render;
struct c_render_buffer *c_render_buffer_create(struct c_render *render, 
                                               uint32_t width, uint32_t height, uint32_t format, 
                                               uint64_t *modifiers, size_t n_modifiers); 
void c_render_buffer_destroy(struct c_render *render, struct c_render_buffer *buf);

#endif
