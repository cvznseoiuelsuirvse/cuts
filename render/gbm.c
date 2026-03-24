#include <gbm.h>
#include <stdint.h>

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include <GL/gl.h>

struct c_render_buffer {
  struct gbm_bo *gbm_bo;
  uint32_t drm_fb_id;
  EGLImageKHR egl_image;
  GLuint texture;
  GLuint fbo;
  int busy;
};


struct c_swapchain *c_render_buffer_create(struct gbm_device *gbm_device, uint32_t width, uint32_t height, uint32_t format) {

}
