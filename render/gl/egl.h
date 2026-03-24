#ifndef CUTS_RENDER_GL_EGL_H
#define CUTS_RENDER_GL_EGL_H

#include "render/render.h"

struct c_egl {
	EGLDisplay  display; 
	EGLContext  context;

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
struct c_egl *c_egl_init(struct gbm_device *device);
struct c_format *c_egl_query_formats(struct c_egl *egl, size_t *n_entries);
EGLImageKHR c_egl_create_image_from_dmabuf(struct c_egl *egl, struct c_dmabuf_params *params);
void c_egl_swap_buffers(struct c_egl *egl);

#endif
