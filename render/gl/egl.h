#ifndef CUTS_RENDER_GL_EGL_H
#define CUTS_RENDER_GL_EGL_H

#include <stdio.h>
#include <gbm.h>
#include "render/types.h"

struct c_egl {
	EGLDisplay  display; 
	EGLContext  context;

	struct {
		int EXT_image_dma_buf_import;
		int EXT_image_dma_buf_import_modifiers;
    int ANDROID_native_fence_sync;
    int KHR_fence_sync;
	} ext_support;

	struct {
		PFNEGLCREATEIMAGEKHRPROC 		      eglCreateImageKHR;
		PFNEGLDESTROYIMAGEKHRPROC 		    eglDestroyImageKHR;
    PFNEGLCREATESYNCKHRPROC           eglCreateSyncKHR;
    PFNEGLDESTROYSYNCKHRPROC          eglDestroySyncKHR;
    PFNEGLWAITSYNCKHRPROC             eglWaitSyncKHR;
    PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
		PFNEGLDEBUGMESSAGECONTROLKHRPROC  eglControlDebugMessageKHR;
		PFNEGLGETPLATFORMDISPLAYEXTPROC   eglGetPlatformDisplayEXT;
		PFNEGLQUERYDMABUFFORMATSEXTPROC   eglQueryDmaBufFormatsEXT;
		PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT;
	} proc;

};

struct c_egl *c_egl_init(struct gbm_device *device);
void c_egl_free(struct c_egl *egl);

struct c_format *c_egl_query_formats(struct c_egl *egl, size_t *n_entries);
EGLImageKHR c_egl_create_image_from_dmabuf(struct c_egl *egl, struct c_dmabuf *dmabuf);


EGLSyncKHR c_egl_create_sync(struct c_egl *egl);
void c_egl_destroy_sync(struct c_egl *egl, EGLSyncKHR sync);
EGLint c_egl_dup_fence_fd(struct c_egl *egl, EGLSyncKHR sync);

#endif
