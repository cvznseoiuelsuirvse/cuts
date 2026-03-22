#ifndef CUTS_BACKEND_DRM_H
#define CUTS_BACKEND_DRM_H

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "wayland/types/wayland.h"
#include "backend/input.h"

#define c_drm_for_each_connector(backend, conn)  \
  for (size_t i = 0; i < (backend)->connectors_n && (((conn) = &(backend)->connectors[i]), 1); i++) \

struct c_drm_connector {
	uint32_t 		    id;
	drmModeConnectorPtr conn;
	drmModeModeInfoPtr  info;
	uint32_t		    crtc_id;
	drmModeCrtcPtr	    orig_crtc;
	int 				waiting_for_flip;
};

struct c_output {
	uint32_t width, height;
	char name[64];
	struct c_drm_cursor *cursor;
};

struct c_drm {
	int 	 fd;
	uint32_t buf_id;
	uint32_t buf_id_old;
	struct gbm_device  *gbm_device;

	struct c_drm_connector connector; 
	struct c_output 	   *output;
};

struct c_drm *c_drm_init(int drm_fd, struct c_input *input);
void c_drm_free(struct c_drm *drm);
int c_drm_dev_id(struct c_drm *drm, dev_t *dev_id);
int drm_format_num_planes(uint32_t format);
static inline enum wl_shm_format_enum drm_to_wl_shm_format(uint32_t format) {
	switch (format) {
		case DRM_FORMAT_ARGB8888: return WL_SHM_FORMAT_ARGB8888;
		case DRM_FORMAT_XRGB8888: return WL_SHM_FORMAT_XRGB8888;
		default:				  return format;
	}
}

#endif
