#ifndef CUTS_RENDER_RENDER_H
#define CUTS_RENDER_RENDER_H

#include <stdint.h>
#include <gbm.h>

#include "output/output.h"
#include "render/types.h"

struct c_renderer {
	struct c_egl *egl;
	struct c_gles *gl;

	struct c_format *formats;
	size_t format_table_entries;
  int format_table_fd;
};

struct c_renderer *c_renderer_init(struct c_output_manager *mgr);
void c_renderer_free(struct c_renderer *renderer);
int c_renderer_create_format_table(struct c_renderer *renderer);

void c_renderer_begin(struct c_renderer *renderer, struct c_output *output);
int c_renderer_draw(struct c_renderer *render, struct c_output *output, struct c_renderer_quad *quad);
int c_renderer_commit(struct c_renderer *render, struct c_output *output);
#endif
