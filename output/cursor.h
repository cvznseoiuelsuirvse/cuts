#ifndef CUTS_BACKEND_DRM_CURSOR_H
#define CUTS_BACKEND_DRM_CURSOR_H

#include <stdint.h>
#include <gbm.h>

#include "seat/input.h"
#include "output/output.h"

struct c_cursor {
  int drm_fd;
	uint32_t       width, height;
	double         x,     y;
	struct gbm_bo *gbm_bo;
};

struct c_cursor *c_cursor_init(struct c_output_manager *mgr,
                               struct c_input *input, uint32_t width,
                               uint32_t height);
void c_cursor_free(struct c_cursor *cursor);
int c_cursor_update(struct c_output_manager *mgr, struct c_output *output, void *buffer,
                    size_t buffer_size);

#endif
