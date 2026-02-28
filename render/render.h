#ifndef CUTS_RENDER_RENDER_H
#define CUTS_RENDER_RENDER_H

#include <gbm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct c_render {
	struct c_drm_backend  *drm;
	struct gbm_device  *gbm_device;
	struct gbm_surface *gbm_surface;
	struct gbm_bo 	   *gbm_bo;
	struct gbm_bo 	   *gbm_bo_next;
	struct c_egl       *egl;
	struct c_vulkan    *vk;
};

struct c_render *c_render_init(struct c_drm_backend *backend);
void c_render_free(struct c_render *render);
int c_render_new_page_flip(struct c_render *render);
int c_render_handle_event(struct c_render *render);

#endif
