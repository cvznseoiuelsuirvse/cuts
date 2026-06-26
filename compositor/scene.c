#include "compositor/window.h"
#include "compositor/scene.h"
#include "util/log.h"
#include "util/helpers.h"

struct {
	c_list  *windows;
	uint32_t width;
	uint32_t height;
} __scene;

extern float __gl_bg_color[4];

static void collect_surface_tree(struct c_window *window, struct c_wl_surface *surface,
                                  int depth, struct c_scene_quad *out, int *count, int max) {
	int is_float = window->state & C_WINDOW_FLOAT;

	if (!(surface->role == C_WL_SURFACE_ROLE_XDG_TOPLEVEL) ||
	    !surface->sub.children || surface->sub.children->size == 0) {

		if (!surface->active) return;
		if (*count >= max) return;

		struct c_scene_quad q = {
			.buffer = surface->active,
			.width  = is_float ? surface->active->width  : window->width,
			.height = is_float ? surface->active->height : window->height,
			.x = window->x,
			.y = window->y,
      .border_width = window->border_width,
      .border_color = HEX_TO_VEC4(window->border_color),
		};

		c_log(C_LOG_DEBUG, "%-*s 0(depth:%d) surface %p width=%d height=%d x=%d y=%d",
		      depth * 3, " ", depth, surface, q.width, q.height, q.x, q.y);

		out[(*count)++] = q;
		return;
	}

	int i = 0;
	struct c_wl_subsurface *ss;
	c_list_for_each(surface->sub.children, ss) {
		if (!ss->surface->active) continue;
		if (*count >= max) return;

		struct c_scene_quad q = { 
      .buffer = ss->surface->active,
      .border_width = window->border_width,
      .border_color = HEX_TO_VEC4(window->border_color),
    };

		if (i == 0 && depth == 0) {
			q.width  = window->width;
			q.height = window->height;
			q.x = window->x;
			q.y = window->y;
		} else {
			q.width  = ss->surface->active->width;
			q.height = ss->surface->active->height;
			q.x = window->x + ss->x;
			q.y = window->y + ss->y;
		}

		if (is_float) {
			q.x -= q.width  / 2;
			q.y -= q.height / 2;
		}

		c_log(C_LOG_DEBUG, "[SUB] %-*s %d(depth:%d) surface %p width=%d height=%d x=%d y=%d",
		      depth * 3 + 2, "-", i + 1, depth, ss, q.width, q.height, q.x, q.y);

		out[(*count)++] = q;

		if (ss->surface->sub.children)
			collect_surface_tree(window, ss->surface, depth + 1, out, count, max);

		i++;
	}
}

void c_scene_init(uint32_t width, uint32_t height) {
	__scene.windows = c_list_new();
	__scene.width   = width;
	__scene.height  = height;
}

void c_scene_destroy() {
	if (__scene.windows) c_list_destroy(__scene.windows);
}

void c_scene_add_window(struct c_window *window) {
	if (__scene.windows)
		c_list_push(__scene.windows, window, 0);
}

void c_scene_remove_window(struct c_window *window) {
	if (__scene.windows)
		c_list_remove_ptr(&__scene.windows, window);
}

void c_scene_clear() {
  if (__scene.windows)
    c_list_clear(__scene.windows);
}

int c_scene_collect(struct c_scene_quad *out, int max_quads) {
	int count = 0;
	struct c_window *window;
	c_list_for_each(__scene.windows, window)
		collect_surface_tree(window, window->surface, 0, out, &count, max_quads);
	return count;
}

void c_scene_set_background(float color[4]) {
  memcpy(__gl_bg_color, color, sizeof(float) * 4);
}

