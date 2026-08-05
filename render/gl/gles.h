#ifndef CUTS_RENDER_GL_GLES_H
#define CUTS_RENDER_GL_GLES_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "render/types.h"
#include "compositor/scene.h"

struct c_gles_texture {
  GLuint texture;
  GLenum target;
};

struct c_gles {
	GLuint program;
	GLuint ext_program;

	GLuint vbo;
	GLuint vao;
	struct {
		PFNGLEGLIMAGETARGETTEXTURE2DOESPROC 		  glEGLImageTargetTexture2DOES;
		PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES;
		PFNGLDEBUGMESSAGECONTROLKHRPROC 			  glDebugMessageControlKHR;
		PFNGLDEBUGMESSAGECALLBACKKHRPROC 			  glDebugMessageCallbackKHR;
	} proc;

	struct {
		int OES_EGL_image_external;
	} ext_support;
};

struct c_gles *c_gles_init();
void c_gles_free(struct c_gles *gl);
int c_gles_texture_from_dma(struct c_gles *gl, struct c_dmabuf *buf);
int c_gles_texture_from_raw(struct c_rawbuf *buf);
void c_gles_draw_quad(struct c_gles *gl, struct c_output *output,
                      struct c_scene_quad *quad,
                      struct c_gles_texture *texture);

#endif
