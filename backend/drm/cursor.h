#ifndef CUTS_BACKEND_DRM_CURSOR_H
#define CUTS_BACKEND_DRM_CURSOR_H

#include <stdint.h>
#include "backend/drm/drm.h"

struct c_drm_cursor {
	uint32_t width, height;
	uint32_t x, y;
	struct gbm_bo 	   *gbm_bo;
	struct gbm_bo 	   *gbm_bo_next;
};

struct c_drm_cursor *c_drm_cursor_init(struct c_drm *drm, struct c_input *input);
void c_drm_cursor_free(struct c_drm_cursor *cursor);
int c_drm_cursor_write(struct c_drm *drm, struct c_drm_cursor *cursor, uint32_t *buffer, size_t buffer_size);

#endif
