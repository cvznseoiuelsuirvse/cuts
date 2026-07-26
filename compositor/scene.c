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
  if (!surface->active) return;
  if (*count >= max) return;

  struct c_scene_quad q = {
    .buffer = surface->active,
    .width  = window->width,
    .height = window->height,
    .x = window->x,
    .y = window->y,
    .border_width = window->border_width,
    .border_color = HEX_TO_VEC4(window->border_color),
    .uv_scale = {1.0f, 1.0f},
    .uv_offset = {0.0f},
  };

  struct {
    int32_t x, y;
    uint32_t w, h;
  } win_geom = {0};

  if (surface->xdg_surface && surface->xdg_surface->width > 0) {
    win_geom.x = surface->xdg_surface->x;
    win_geom.y = surface->xdg_surface->y;
    win_geom.w = surface->xdg_surface->width;
    win_geom.h = surface->xdg_surface->height;

    q.uv_offset[0] = (float)win_geom.x / surface->active->width;
    q.uv_offset[1] = (float)win_geom.y / surface->active->height;

    q.uv_scale[0] = (float)win_geom.w / surface->active->width;
    q.uv_scale[1] = (float)win_geom.h / surface->active->height;
  }

  c_log(C_LOG_DEBUG, "uv_scale = %f, %f", q.uv_scale[0], q.uv_scale[1]);
  c_log(C_LOG_DEBUG, "uv_offset = %f, %f", q.uv_offset[0], q.uv_offset[1]);
  c_log(C_LOG_DEBUG, "%-*s surface#%d %p width=%d height=%d x=%d y=%d depth=%d",
        depth + 1, " ", surface->id, surface, q.width, q.height, q.x, q.y, depth);
  out[(*count)++] = q;

  if (!surface->sub.children || surface->sub.children->size == 0) return;
  q.border_width = 0;

	struct c_wl_subsurface *sub_s;
	c_list_for_each(surface->sub.children, sub_s) {
		if (!sub_s->surface->active) continue;
		if (*count >= max) return;

    q.buffer = sub_s->surface->active,

    q.width  = sub_s->surface->active->width;
    q.height = sub_s->surface->active->height;

    q.x = window->x + sub_s->x - win_geom.x;
    q.y = window->y + sub_s->y - win_geom.y;

		c_log(C_LOG_DEBUG, "%-*s SUB-surface#%d %p width=%d height=%d x=%d y=%d depth=%d",
		      depth + 3, " ", sub_s->id, sub_s, q.width, q.height, q.x, q.y, depth);

		out[(*count)++] = q;

		if (sub_s->surface->sub.children)
			collect_surface_tree(window, sub_s->surface, depth + 1, out, count, max);
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

