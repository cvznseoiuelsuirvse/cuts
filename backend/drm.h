#ifndef CUTS_BACKEND_DRM_H
#define CUTS_BACKEND_DRM_H

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

/* why would you need 16+ outputs */
#define C_DRM_MAX_CONNECTORS 16

struct c_drm_dumb_framebuffer {
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
	drmModeConnectorPtr conn;
	drmModeModeInfoPtr  mode;
	uint32_t		    crtc_id;
	drmModeCrtcPtr	    orig_crtc;
	struct c_drm_dumb_framebuffer *front;
	struct c_drm_dumb_framebuffer *back;
};

struct c_drm_backend {
	int                 fd;
	drmModeResPtr       res;

	size_t				   connectors_n;
	struct c_drm_connector connectors[C_DRM_MAX_CONNECTORS];
};

struct c_drm_backend *c_drm_backend_init();
void c_drm_backend_free(struct c_drm_backend *backend);
int c_drm_backend_page_flip(struct c_drm_backend *backend);

int c_drm_connector_set_crtc(int fd, struct c_drm_connector *conn, struct c_drm_dumb_framebuffer *fb);

#endif
