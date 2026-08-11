#include <assert.h>
#include <stdlib.h>

#include "render/renderer.h"
#include "compositor/window.h"
#include "compositor/scene.h"
#include "output/output.h"

#include "util/log.h"

#define MAX_QUADS 1024

static void get_surface_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  if (surface->active->dma) {
    *width = surface->active->dma->width / surface->active->scale;
    *height = surface->active->dma->height / surface->active->scale;
  } else {
    *width = surface->active->shm->width / surface->active->scale;
    *height = surface->active->shm->height / surface->active->scale;
  }
}

static void collect_window_tree(struct c_window *window, struct c_wl_surface *surface,
                                 double x, double y,
                                 c_list *quads, int depth) {
  if (!surface->active) return;

  int32_t buf_width, buf_height;
  get_surface_buf_size(surface, &buf_width, &buf_height);

  struct c_renderer_quad q = {
    .type = C_RENDERER_BUFFER,
    .buffer = surface->active->dma ? (void *)surface->active->dma : (void *)surface->active->shm,
    .buffer_type = surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,
  };

  double base_x, base_y;

  if (window) {
    q.width = window->width;
    q.height = window->height;
    q.x = window->x;
    q.y = window->y;
    base_x = window->x;
    base_y = window->y;
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

    q.width  /= ((double)w_w / buf_width);
    q.height /= ((double)w_h / buf_height);

    q.x -= w_x * ((double)buf_width  / w_w);
    q.y -= w_y * ((double)buf_height / w_h);
  }

  c_log(C_LOG_DEBUG, "%*s surface#%d (%d) %p %dx%d x=%f y=%f", depth, " ",
        surface->id, q.buffer_type, surface, q.width, q.height, q.x, q.y);

  c_list_push(quads, &q, sizeof(q));

  if (surface->sub.children) {
    struct c_wl_subsurface *sub_s;
    c_list_for_each(surface->sub.children, sub_s) {
      if (!sub_s->surface->active) continue;

      get_surface_buf_size(sub_s->surface, &buf_width, &buf_height);

      struct c_renderer_quad q_sub = {
        .type = C_RENDERER_BUFFER,
        .buffer = sub_s->surface->active->dma ? (void *)sub_s->surface->active->dma : (void *)sub_s->surface->active->shm,
        .buffer_type = sub_s->surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,

        .width = buf_width,
        .height = buf_height,

        .x = base_x + sub_s->x - w_x,
        .y = base_y + sub_s->y - w_y,
      };

      c_log(C_LOG_DEBUG, "%*s SUB-surface#%d (%d) %p %dx%d x=%f y=%f",
            depth + 2, " ", sub_s->id, q_sub.buffer_type, sub_s, q_sub.width,
            q_sub.height, q_sub.x, q_sub.y);

      c_list_push(quads, &q_sub, sizeof(q_sub));
      collect_window_tree(NULL, sub_s->surface, q_sub.x, q_sub.y, quads, depth + 1);
    }
  }

  if (surface->xdg_surface && surface->xdg_surface->children) {
    struct c_xdg_surface *xdg_s;
    c_list_for_each(surface->xdg_surface->children, xdg_s) {
      if (!xdg_s->surface->active) continue;

      get_surface_buf_size(xdg_s->surface, &buf_width, &buf_height);

      struct c_renderer_quad q_sub = {
        .type = C_RENDERER_BUFFER,
        .buffer = xdg_s->surface->active->dma
                      ? (void *)xdg_s->surface->active->dma
                      : (void *)xdg_s->surface->active->shm,
        .buffer_type = xdg_s->surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,

        .width = xdg_s->popup.positioner.width ? xdg_s->popup.positioner.width : buf_width,
        .height = xdg_s->popup.positioner.height ? xdg_s->popup.positioner.height : buf_height,

        .x = base_x + xdg_s->x + xdg_s->popup.x,
        .y = base_y + xdg_s->y + xdg_s->popup.y,
      };

      c_log(C_LOG_DEBUG, "%*s XDG-surface#%d (%d) %p %dx%d x=%f y=%f", depth,
            " ", xdg_s->id, q_sub.buffer_type, xdg_s, q_sub.width, q_sub.height,
            q_sub.x, q_sub.y);

      c_list_push(quads, &q_sub, sizeof(q_sub));
      collect_window_tree(NULL, xdg_s->surface, q_sub.x, q_sub.y, quads, depth + 1);
    }
  }
}

static void create_rect_quad(struct c_scene_rect *rect, struct c_renderer_quad *quad) {
  quad->type = C_RENDERER_SOLID;
  quad->x = rect->x;
  quad->y = rect->y;
  quad->width = rect->width;
  quad->height = rect->height;
  quad->color[0] = rect->color[0];
  quad->color[1] = rect->color[1];
  quad->color[2] = rect->color[2];
  quad->color[3] = rect->color[3];

}

static int on_redraw(struct c_output_manager *mgr, struct c_output *output, void *userdata) {
  struct c_scene *scene = userdata;

  c_renderer_begin(mgr->renderer, output);

  struct c_scene_node *node;
  struct c_renderer_quad *quad;
  c_list_for_each(scene->nodes, node) {
    if (node->is_hidden) continue;

    switch (node->type) {
      case C_SCENE_WINDOW:
        c_list_for_each(node->quads, quad)
          c_renderer_draw(mgr->renderer, output, quad);
        break;

      case C_SCENE_RECT:
        c_renderer_draw(mgr->renderer, output, node->quad);
        break;
    }
  }

  return c_renderer_commit(mgr->renderer, output);
}

static struct c_scene_node *node_create(enum c_scene_node_types type, void *data) {
  struct c_scene_node *node = calloc(1, sizeof(*node));
  node->type = type;
  node->data = data;

  if (node->type == C_SCENE_WINDOW)
    node->quads = c_list_new();

  return node;
}

static void node_remove(struct c_scene_node *node) {
  if (node->type == C_SCENE_WINDOW)
    c_list_destroy(node->quads);
  else
    free(node->quad);
  free(node);
}

struct c_scene *c_scene_init(struct c_output_manager *mgr) {
  struct c_scene *scene = calloc(1, sizeof(*scene));
  if (!scene) {
    c_log_errno(C_LOG_ERROR, "failed to allocate c_scene");
    return NULL;
  }

  c_output_register_on_redraw(mgr, on_redraw, scene);
  scene->nodes = c_list_new();

  return scene;
}

void c_scene_free(struct c_scene *scene) {
	if (scene->nodes) {
    struct c_scene_node *node;
    c_list_for_each(scene->nodes, node) {
      node_remove(node);
    }
    c_list_destroy(scene->nodes);
  }
  free(scene);
}

struct c_scene_node *c_scene_add_window(struct c_scene *scene, struct c_window *window) {
  struct c_scene_node *node = node_create(C_SCENE_WINDOW, window);
  c_list_push(scene->nodes, node, 0);

  collect_window_tree(window, window->surface->surface, window->x, window->y,
                      node->quads, 0);
  return node;
}

struct c_scene_node *c_scene_add_rect(struct c_scene *scene, struct c_scene_rect *rect) {
  struct c_renderer_quad *quad = calloc(1, sizeof(*quad));
  create_rect_quad(rect, quad);

  struct c_scene_node *node = node_create(C_SCENE_RECT, rect);
  c_list_push(scene->nodes, node, 0);
  node->quad = quad;

  return node;
}

void c_scene_node_update(struct c_scene_node *node) {
  struct c_window *window;
  struct c_scene_rect *rect;

  switch (node->type) {
    case C_SCENE_WINDOW:
      window = node->data;
      c_list_clear(node->quads);
      collect_window_tree(window, window->surface->surface, window->x,
                          window->y, node->quads, 0);
      break;

    case C_SCENE_RECT:
      rect = node->data;
      create_rect_quad(rect, node->quad);

      break;
  }
}

void c_scene_node_remove(struct c_scene *scene, struct c_scene_node *node) {
  node_remove(node);
  c_list_remove(&scene->nodes, node);
}

void c_scene_node_raise(struct c_scene *scene, struct c_scene_node *node) {
  c_list_remove(&scene->nodes, node);
  c_list_push(scene->nodes, node, 0);
}
