#include <assert.h>
#include <stdlib.h>

#include "render/renderer.h"
#include "compositor/window.h"
#include "compositor/scene.h"
#include "output/output.h"

#include "util/log.h"
#include "util/helpers.h"

#define MAX_QUADS 1024

struct surface_geom {
  float x, y;
  uint32_t width, height;
};

static void get_surface_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  if (surface->active->dma) {
    *width = surface->active->dma->width / surface->active->scale;
    *height = surface->active->dma->height / surface->active->scale;
  } else {
    *width = surface->active->shm->width / surface->active->scale;
    *height = surface->active->shm->height / surface->active->scale;
  }
}

static void collect_surface_tree(struct c_window *window, struct c_wl_surface *surface,
                                 double x, double y,
                                 struct c_scene_quad *out, int *count, int max, int depth) {
  if (!surface->active) return;
  if (*count >= max) return;

  int32_t buf_width, buf_height;
  get_surface_buf_size(surface, &buf_width, &buf_height);

  struct c_scene_quad q = {
    .buffer = surface->active->dma ? (void *)surface->active->dma : (void *)surface->active->shm,
    .buffer_type = surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,
    .uv_scale = {1.0f, 1.0f},
    .uv_offset = {0.0f, 0.0f},
  };

  double base_x, base_y;

  if (window) {
    q.width = window->width;
    q.height = window->height;
    q.x = window->x;
    q.y = window->y;
    q.border_width = window->border_width;
    memcpy(q.border_color, (float[4])HEX_TO_VEC4(window->border_color), sizeof(q.border_color));
    base_x = window->x + window->border_width;
    base_y = window->y + window->border_width;
  } else {
    q.width = buf_width;
    q.height = buf_height;
    q.x = x;
    q.y = y;
    base_x = x;
    base_y = y;
  }

  double w_x, w_y;
  uint32_t w_w, w_h;

  if (surface->xdg_surface && surface->xdg_surface->width > 0) {
    w_x = surface->xdg_surface->x;
    w_y = surface->xdg_surface->y;
    w_w = surface->xdg_surface->width;
    w_h = surface->xdg_surface->height;

    q.uv_offset[0] = (float)w_x / buf_width;
    q.uv_offset[1] = (float)w_y / buf_height;

    q.uv_scale[0] = (float)w_w / buf_width;
    q.uv_scale[1] = (float)w_h / buf_height;
  }

  c_log(C_LOG_DEBUG, "%*s surface#%d (%d) %p %dx%d x=%f y=%f", depth, " ",
        surface->id, q.buffer_type, surface, q.width, q.height, q.x, q.y);

  out[(*count)++] = q;

  if (surface->sub.children) {
    struct c_wl_subsurface *sub_s;
    c_list_for_each(surface->sub.children, sub_s) {
      if (!sub_s->surface->active) continue;
      if (*count >= max) return;

      get_surface_buf_size(sub_s->surface, &buf_width, &buf_height);

      struct c_scene_quad q_sub = {
        .buffer = sub_s->surface->active->dma ? (void *)sub_s->surface->active->dma : (void *)sub_s->surface->active->shm,
        .buffer_type = sub_s->surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,

        .width = buf_width,
        .height = buf_height,

        .border_width = 0,

        .x = base_x + sub_s->x - w_x,
        .y = base_y + sub_s->y - w_y,

        .uv_scale = {1.0f, 1.0f},
        .uv_offset = {0.0f, 0.0f},
      };

      c_log(C_LOG_DEBUG, "%*s SUB-surface#%d (%d) %p %dx%d x=%f y=%f",
            depth + 2, " ", sub_s->id, q_sub.buffer_type, sub_s, q_sub.width,
            q_sub.height, q_sub.x, q_sub.y);

      out[(*count)++] = q_sub;

      collect_surface_tree(NULL, sub_s->surface, q_sub.x, q_sub.y, out, count, max, depth + 1);
    }
  }

  if (surface->xdg_surface && surface->xdg_surface->children) {
    struct c_xdg_surface *xdg_s;
    c_list_for_each(surface->xdg_surface->children, xdg_s) {
      if (!xdg_s->surface->active) continue;
      if (*count >= max) return;

      get_surface_buf_size(xdg_s->surface, &buf_width, &buf_height);

      struct c_scene_quad q_sub = {
        .buffer = xdg_s->surface->active->dma
                      ? (void *)xdg_s->surface->active->dma
                      : (void *)xdg_s->surface->active->shm,
        .buffer_type = xdg_s->surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,

        .width = xdg_s->popup.positioner.width ? xdg_s->popup.positioner.width : buf_width,
        .height = xdg_s->popup.positioner.height ? xdg_s->popup.positioner.height : buf_height,

        .border_width = 0,

        .x = base_x + xdg_s->x + xdg_s->popup.x,
        .y = base_y + xdg_s->y + xdg_s->popup.y,

        .uv_scale = {1.0f, 1.0f},
        .uv_offset = {0.0f, 0.0f},
      };

      c_log(C_LOG_DEBUG, "%*s XDG-surface#%d (%d) %p %dx%d x=%f y=%f", depth,
            " ", xdg_s->id, q_sub.buffer_type, xdg_s, q_sub.width, q_sub.height,
            q_sub.x, q_sub.y);

      out[(*count)++] = q_sub;

      collect_surface_tree(NULL, xdg_s->surface, q_sub.x, q_sub.y, out, count, max, depth + 1);
    }
  }
}

static int collect(struct c_scene *scene, struct c_output *output, struct c_scene_quad *out, int max_quads) {
  int count = 0;
  struct c_window *window;
  c_list_for_each(scene->windows, window) {
    collect_surface_tree(window, window->surface->surface, 0, 0, out, &count, max_quads, 0);
  }
	return count;
}

static void on_redraw(struct c_output_manager *mgr, struct c_output *output, void *userdata) {
  struct c_scene *scene = userdata;

	struct c_scene_quad quads[MAX_QUADS];
	int n = collect(scene, output, quads, MAX_QUADS);
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
