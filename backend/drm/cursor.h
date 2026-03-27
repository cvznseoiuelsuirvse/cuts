#ifndef CUTS_BACKEND_DRM_CURSOR_H
#define CUTS_BACKEND_DRM_CURSOR_H

#include <stdint.h>
#include <gbm.h>
#include "backend/drm/drm.h"

struct c_cursor {
	uint32_t       width, height;
	double         x,     y;
	uint32_t 	  *image;
	size_t 	 	   image_size;
	struct gbm_bo *gbm_bo;
};

struct c_cursor *c_cursor_init(struct c_drm *drm, struct c_input *input);
void c_cursor_free(struct c_cursor *cursor);
int c_cursor_update(struct c_drm *drm, struct c_cursor *cursor);

#endif
