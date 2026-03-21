#ifndef CUTS_BACKEND_DRM_CURSOR_H
#define CUTS_BACKEND_DRM_CURSOR_H

#include <stdint.h>
#include "backend/drm/drm.h"

struct c_drm_cursor {
	uint32_t width, height;
	double x, y;
	uint32_t *image;
	size_t 	  image_size;
	struct gbm_bo 	   *gbm_bo;
};

struct c_drm_cursor *c_drm_cursor_init(struct c_drm *drm, struct c_input *input);
void c_drm_cursor_free(struct c_drm_cursor *cursor);
int c_drm_cursor_update(struct c_drm *drm, struct c_drm_cursor *cursor);

#endif
