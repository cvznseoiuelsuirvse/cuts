#ifndef CUTS_RENDER_RENDER_H
#define CUTS_RENDER_RENDER_H

#include <stdint.h>
#include <gbm.h>

#include "output/output.h"
#include "wayland/display.h"
#include "wayland/server.h"
#include "render/types.h"
#include "compositor/scene.h"

struct c_renderer {
	struct c_egl *egl;
	struct c_gles *gl;

	struct c_format *formats;
	size_t format_table_entries;
  int format_table_fd;
};

struct c_renderer *c_renderer_init(struct c_output_manager *mgr, struct c_wl_display *display);
void c_renderer_free(struct c_renderer *renderer);
void c_renderer_draw(struct c_renderer *render, struct c_output *output,
                     struct c_scene_quad *quads, size_t quad_n,
                     float background[4]);
int c_renderer_create_format_table(struct c_renderer *renderer);
#endif
