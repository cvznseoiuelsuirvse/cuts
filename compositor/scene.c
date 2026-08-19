#include <stdlib.h>

#include "render/renderer.h"
#include "compositor/window.h"
#include "compositor/scene.h"
#include "output/output.h"

#include "util/log.h"
#include "util/malloc.h"

enum c_scene_node_types {
  C_SCENE_WINDOW,
  C_SCENE_RECT,
  C_SCENE_BUFFER,
};

struct c_scene_node {
  enum c_scene_node_types type;
  union {
    c_list *quads;
    struct c_renderer_quad *quad;
  };
  void *data;
  int is_hidden;
  collect_quads_function collect;
};

struct c_scene {
	c_list *nodes;
};

static void unref_window_quads(struct c_scene_node *node) {
  struct c_renderer_quad *quad;
  c_list_for_each(node->quads, quad)
    c_unref(quad->buffer);
}

static void ref_window_quads(struct c_scene_node *node) {
  struct c_renderer_quad *quad;
  c_list_for_each(node->quads, quad)
    c_ref(quad->buffer);
}

static void create_buffer_quad(struct c_scene_buffer *buffer, struct c_renderer_quad *quad) {
  struct c_rawbuf *rawbuf = &buffer->raw;
  rawbuf->width = buffer->width;
  rawbuf->height = buffer->height;
  rawbuf->stride = rawbuf->width * 4;
  rawbuf->format = DRM_FORMAT_ABGR8888;
  rawbuf->base_ptr = buffer->buffer;

  quad->type = C_RENDERER_BUFFER;
  quad->buffer = rawbuf;
  quad->buffer_type = C_BUFFER_RAW;

  quad->x = buffer->x;
  quad->y = buffer->y;

  quad->width = buffer->width;
  quad->height = buffer->height;

  quad->uv_scale[0] = 1.0f;
  quad->uv_scale[1] = 1.0f;
}

static void create_rect_quad(struct c_scene_rect *rect, struct c_renderer_quad *quad) {
  quad->type = C_RENDERER_SOLID;
  quad->x = rect->x;
  quad->y = rect->y;
  quad->width = rect->width;
  quad->height = rect->height;

  quad->color[0] = rect->color[0] / 255.0f;
  quad->color[1] = rect->color[1] / 255.0f;
  quad->color[2] = rect->color[2] / 255.0f;
  quad->color[3] = rect->color[3] / 255.0f;
}

static int on_redraw(struct c_output_manager *mgr, struct c_output *output, void *userdata) {
  struct c_scene *scene = userdata;

  c_renderer_begin(mgr->renderer, output);

  struct c_scene_node *node;
  struct c_renderer_quad *quad;
  c_list_for_each(scene->nodes, node) {
    if (node->is_hidden) continue;

    if (node->type == C_SCENE_WINDOW) {
      c_list_for_each(node->quads, quad)
        c_renderer_draw(mgr->renderer, output, quad);
    } else {
      c_renderer_draw(mgr->renderer, output, node->quad);
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
  switch (node->type) {
    case C_SCENE_WINDOW:
      c_list_destroy(node->quads);
      break;

    case C_SCENE_RECT:
      free(node->quad);
      break;

    case C_SCENE_BUFFER:
      free(node->quad);
      free(((struct c_scene_buffer *)node->data)->raw.texture);
      break;
  }

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

struct c_scene_node *c_scene_add_buffer(struct c_scene *scene, struct c_scene_buffer *buf) {
  struct c_renderer_quad *quad = calloc(1, sizeof(*quad));
  create_buffer_quad(buf, quad);

  struct c_scene_node *node = node_create(C_SCENE_BUFFER, buf);
  c_list_push(scene->nodes, node, 0);
  node->quad = quad;

  return node;
}

void c_scene_node_update(struct c_scene_node *node) {
  switch (node->type) {
    case C_SCENE_WINDOW:
      unref_window_quads(node);
      c_list_clear(node->quads);

      if (node->collect) {
          node->collect(node, node->quads);
          ref_window_quads(node);
      }
      break;

    case C_SCENE_RECT:
      create_rect_quad(node->data, node->quad);
      break;

    case C_SCENE_BUFFER:
      ((struct c_scene_buffer *)node->data)->raw.dirty = 1;
      create_buffer_quad(node->data, node->quad);
      break;
  }
}

void c_scene_node_remove(struct c_scene *scene, struct c_scene_node *node) {
  if (node->type == C_SCENE_WINDOW)
    unref_window_quads(node);

  node_remove(node);
  c_list_remove(&scene->nodes, node);
}

void c_scene_node_raise(struct c_scene *scene, struct c_scene_node *node) {
  c_list_remove(&scene->nodes, node);
  c_list_push(scene->nodes, node, 0);
}

void c_scene_node_set_visibility(struct c_scene_node *node, int is_visible) {
  node->is_hidden = !is_visible;
}

void *c_scene_node_data(struct c_scene_node *node) {
  return node->data;
}

void c_scene_node_set_collect(struct c_scene_node *node, collect_quads_function func) {
  node->collect = func;
}
