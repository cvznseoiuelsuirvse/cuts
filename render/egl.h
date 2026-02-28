#ifndef CUTS_RENDER_GL_H
#define CUTS_RENDER_GL_H

#include "render/render.h"
#include "wayland/server.h"

struct c_format_table_pair {
  uint32_t format;
  uint32_t pad;
  uint64_t modifer;
};

struct c_egl {
	EGLDisplay  display; 
	EGLContext  context;
	EGLImageKHR image;
	EGLSurface  surface;
	struct {
		size_t pairs_n;
		struct c_format_table_pair *pairs;	
	} format_table;

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
		PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

	} proc;

};

void c_egl_free(struct c_egl *egl);
struct c_egl *c_egl_init(struct gbm_device *device, struct gbm_surface *surface);
int c_egl_get_format_table_fd(struct c_egl *egl);
int c_egl_import_dmabuf(struct c_egl *egl, struct c_wl_buffer *buffer);


#endif
