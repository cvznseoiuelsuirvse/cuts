#ifndef CUTS_RENDER_GL_H
#define CUTS_RENDER_GL_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "render/render.h"

struct c_gles {
	GLuint program;
	GLuint vbo;
	GLuint vao;
	struct {
		PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
		PFNGLDEBUGMESSAGECONTROLKHRPROC 	glDebugMessageControlKHR;
		PFNGLDEBUGMESSAGECALLBACKKHRPROC 	glDebugMessageCallbackKHR;
	} proc;
};

struct c_egl {
	struct c_gles *gl;
	EGLDisplay  display; 
	EGLContext  context;
	EGLSurface  surface;

	struct {
		int EXT_image_dma_buf_import_modifiers;
	} ext_support;

	struct {
		PFNEGLCREATEIMAGEKHRPROC 		    eglCreateImageKHR;
		PFNEGLDESTROYIMAGEKHRPROC 		    eglDestroyImageKHR;
		PFNEGLDEBUGMESSAGECONTROLKHRPROC    eglControlDebugMessageKHR;
		PFNEGLGETPLATFORMDISPLAYEXTPROC     eglGetPlatformDisplayEXT;
		PFNEGLQUERYDMABUFFORMATSEXTPROC     eglQueryDmaBufFormatsEXT;
		PFNEGLQUERYDMABUFMODIFIERSEXTPROC   eglQueryDmaBufModifiersEXT;
	} proc;

};

void c_egl_free(struct c_egl *egl);
struct c_egl *c_egl_init(struct gbm_device *device, struct gbm_surface *surface);
struct c_format *c_egl_query_formats(struct c_egl *egl, size_t *n_entries);
int c_egl_import_dmabuf(struct c_egl *egl, struct c_dmabuf_params *params, struct c_dmabuf *buf);
void c_egl_swap_buffers(struct c_egl *egl);

#endif
