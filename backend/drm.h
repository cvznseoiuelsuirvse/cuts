#ifndef CUTS_BACKEND_DRM_H
#define CUTS_BACKEND_DRM_H

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#define c_drm_backend_for_each_connector(backend, conn)  \
  for (size_t i = 0; i < (backend)->connectors_n && (((conn) = &(backend)->connectors[i]), 1); i++) \

struct c_dumb_framebuffer {
	uint32_t id;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t handle;
	uint64_t size;
	uint64_t offset;
	uint8_t *buffer;
};

struct c_drm_connector {
	uint32_t 		    id;
	drmModeConnectorPtr drmModeConn;
	drmModeModeInfoPtr  mode;
	uint32_t		    crtc_id;
	drmModeCrtcPtr	    orig_crtc;
	int 				waiting_for_flip;
	// struct c_dumb_framebuffer *front;
	// struct c_dumb_framebuffer *back;
};

struct c_drm_backend {
	int 	 fd;
	uint32_t buf_id;
	/* single monitor setup for now */
	struct c_drm_connector *connector; 
};

struct c_drm_backend *c_drm_backend_init();
void c_drm_backend_free(struct c_drm_backend *drm);
int c_drm_backend_dev_id(struct c_drm_backend *drm, dev_t *dev_id);
// int  c_drm_backend_page_flip(struct c_drm_backend *backend);
// void c_drm_backend_page_flip2(struct c_drm_backend *backend, int *needs_redraw);

#endif
