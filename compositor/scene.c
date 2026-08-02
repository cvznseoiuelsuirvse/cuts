#include <assert.h>
#include <stdlib.h>

#include "render/renderer.h"
#include "compositor/window.h"
#include "compositor/scene.h"
#include "output/output.h"

#include "util/log.h"
#include "util/helpers.h"

static void get_surface_buf_size(struct c_wl_surface *surface, uint32_t *width, uint32_t *height) {
  if (surface->active->dma) {
    *width = surface->active->dma->width / surface->active->scale;
    *height = surface->active->dma->height / surface->active->scale;
  } else {
    *width = surface->active->shm->width / surface->active->scale;
    *height = surface->active->shm->height / surface->active->scale;
  }
}

static void collect_surface_tree(struct c_window *window, struct c_wl_surface *surface,
                                  int depth, struct c_scene_quad *out, int *count, int max) {
  if (!surface->active) return;
  if (*count >= max) return;

  uint32_t buf_width, buf_height;
  get_surface_buf_size(surface, &buf_width, &buf_height);

  struct c_scene_quad q = {
    .buffer = surface->active->dma ? (void *)surface->active->dma : (void *)surface->active->shm,
    .buffer_type = surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,

    .width  = window->width,
    .height = window->height,
    .x = window->x,
    .y = window->y,
    .border_width = window->border_width,
    .border_color = HEX_TO_VEC4(window->border_color),
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
  }

  c_log(C_LOG_DEBUG, "%-*s surface#%d %p width=%d height=%d x=%d y=%d depth=%d",
        depth + 1, " ", surface->id, surface, q.width, q.height, q.x, q.y, depth);
  out[(*count)++] = q;

  if (!surface->sub.children || surface->sub.children->size == 0) return;

	struct c_wl_subsurface *sub_s;
	c_list_for_each(surface->sub.children, sub_s) {
		if (!sub_s->surface->active) continue;
		if (*count >= max) return;

    uint32_t s_buf_width, s_buf_height;
    get_surface_buf_size(sub_s->surface, &s_buf_width, &s_buf_height);

    struct c_scene_quad q_sub = {
      .buffer = sub_s->surface->active->dma ? (void *)sub_s->surface->active->dma : (void *)sub_s->surface->active->shm,
      .buffer_type = sub_s->surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,

      .width = s_buf_width,
      .height = s_buf_height,
      .border_width = 0,
      .x = window->x + sub_s->x - win_geom.x + window->border_width,
      .y = window->y + sub_s->y - win_geom.y + window->border_width,
    };

		c_log(C_LOG_DEBUG, "%-*s SUB-surface#%d %p width=%d height=%d x=%d y=%d depth=%d",
		      depth + 3, " ", sub_s->id, sub_s, q.width, q.height, q.x, q.y, depth);

		out[(*count)++] = q_sub;

		if (sub_s->surface->sub.children)
			collect_surface_tree(window, sub_s->surface, depth + 1, out, count, max);
	}
}

static int collect(struct c_scene *scene, struct c_output *output, struct c_scene_quad *out, int max_quads) {
  int count = 0;
  struct c_window *window;
  c_list_for_each(scene->windows, window) {
    collect_surface_tree(window, window->surface->surface, 0, out, &count, max_quads);
  }
	return count;
}



static void on_redraw(struct c_output_manager *mgr, struct c_output *output, void *userdata) {
  struct c_scene *scene = userdata;

	struct c_scene_quad quads[C_SCENE_MAX_WINDOWS];
	int n = collect(scene, output, quads, C_SCENE_MAX_WINDOWS);
  c_renderer_draw(mgr->renderer, output, quads, n, scene->bg_color);
}

struct c_scene *c_scene_init(struct c_output_manager *mgr) {
  struct c_scene *scene = calloc(1, sizeof(*scene));
  if (!scene) {
    c_log_errno(C_LOG_ERROR, "failed to allocate c_scene");
    return NULL;
  }

  c_output_register_on_redraw(mgr, on_redraw, scene);
  scene->windows = c_list_new();

  return scene;
}

void c_scene_free(struct c_scene *scene) {
	if (scene->windows)
    c_list_destroy(scene->windows);
}

void c_scene_add_window(struct c_scene *scene, struct c_window *window) {
  c_list_push(scene->windows, window, 0);
}

void c_scene_remove_window(struct c_scene *scene, struct c_window *window) {
  c_list_remove(&scene->windows, window);
}

void c_scene_clear(struct c_scene *scene, struct c_output *output) {
  c_list_clear(scene->windows);
}

inline void c_scene_set_background(struct c_scene *scene, float color[4]) {
  memcpy(scene->bg_color, color, sizeof(float) * 4);
}
