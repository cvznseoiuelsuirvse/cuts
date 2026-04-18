#ifndef CUTS_RENDER_GL_GLES_H
#define CUTS_RENDER_GL_GLES_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "render/render.h"

struct c_gles {
	GLuint program;
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
void c_gles_texture_from_dmabuf_image(struct c_gles *gl, struct c_dmabuf *buf);
void c_gles_texture_from_shm(struct c_shm *buf, uint32_t width, uint32_t height);

#endif
