#ifndef CUTS_RENDER_RENDER_H
#define CUTS_RENDER_RENDER_H

#include <stdint.h>

#include "wayland/display.h"
#include "wayland/server.h"
#include "backend/drm/drm.h"
#include "render/types.h"
#include "render/buffer.h"

struct c_render {
	struct c_drm  *drm;
	struct c_egl *egl;
	struct c_gles *gl;

	size_t n_formats;
	struct c_format *formats;
	uint32_t *wl_formats;

	struct {
		int front;
		struct c_render_buffer *buffers[2];
	} swapchain;

};

struct c_render *c_render_init(struct c_event_loop *loop, struct c_wl_display *dpy, struct c_drm *drm);
void c_render_free(struct c_render *render);
void c_render_draw_scene(struct c_render *render);
#endif
