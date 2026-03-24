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
	uint32_t		    crtc_id;
	drmModeCrtcPtr	    orig_crtc;
	int 				waiting_for_flip;
};

struct c_output_mode {
	uint32_t width, height;
	uint32_t refresh;
	int 	 preferred;
	drmModeModeInfoPtr info;
};
struct c_output {
	char name[64];
	char make[64];
	char model[64];
	uint32_t mm_width, mm_height;
	drmModeSubPixel subpixel;

	c_list *modes;
	struct c_drm_cursor *cursor;
};

struct c_drm {
	int 	 fd;

	struct c_drm_connector connector; 
	struct c_output 	   *output;
};

struct c_drm *c_drm_init(int drm_fd, struct c_input *input);
void c_drm_free(struct c_drm *drm);
int  c_drm_dev_id(struct c_drm *drm, dev_t *dev_id);
struct c_output_mode *c_drm_get_preferred_mode(struct c_drm *drm);
static inline enum wl_shm_format_enum drm_to_wl_shm_format(uint32_t format) {
	switch (format) {
		case DRM_FORMAT_ARGB8888: return WL_SHM_FORMAT_ARGB8888;
		case DRM_FORMAT_XRGB8888: return WL_SHM_FORMAT_XRGB8888;
		default:				  return format;
	}
}

#endif
