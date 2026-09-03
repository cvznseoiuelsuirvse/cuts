#include <stdlib.h>

#include "render/renderer.h"
#include "compositor/window.h"
#include "compositor/scene.h"
#include "compositor/matrix.h"
#include "compositor/util.h"
#include "output/output.h"

#include "wayland/impl/wayland.h"
#include "wayland/impl/xdg-shell.h"
#include "wayland/impl/viewporter.h"

#include "util/log.h"
#include "util/mem.h"

struct c_scene {
	c_list *nodes; // struct c_scene_node *
  uint32_t z_state[5];
};

static struct c_output_events output_events;

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

static void create_surface_quad(struct c_scene_surface *surface, struct c_renderer_quad *quad) {
  quad->type = C_RENDERER_BUFFER;

  quad->x = surface->x;
  quad->y = surface->y;

  quad->buffer = NULL;
  
  matrix3_new(quad->transform);
}

static void create_buffer_quad(struct c_scene_buffer *buffer, struct c_renderer_quad *quad) {
  struct c_rawbuf *rawbuf;
  if (quad->buffer)
     rawbuf = quad->buffer;

  else
     rawbuf = c_malloc(sizeof(*rawbuf));

  rawbuf->width  = buffer->width;
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

  matrix3_new(quad->transform);
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

static void collect_window_tree(struct c_window *window, struct c_wl_surface *surface,
                        double scale, double x, double y,
                        c_list *quads) {
  if (!surface || !surface->active.buffer) return;
  struct c_wl_buffer *buffer = surface->active.buffer;

  struct c_renderer_quad q = {
    .type = C_RENDERER_BUFFER,
  };

  if (buffer->dma) {
    q.buffer = buffer->dma;
    q.buffer_type = C_BUFFER_DMA;
  } else {
    q.buffer = buffer->shm;
    q.buffer_type = C_BUFFER_RAW;
  }

  int32_t raw_w, raw_h;
  get_surface_raw_buf_size(surface, &raw_w, &raw_h);

  double src_x = 0, src_y = 0;
  double src_w = raw_w, src_h = raw_h;

  double surf_x = 0, surf_y = 0;
  double surf_w, surf_h;
  get_surface_size(surface, &surf_w, &surf_h);

  if (surface->viewport) {
    struct c_wp_viewport_state *vp = &surface->viewport->active;
    if (vp->src.width > 0 && vp->src.height > 0) {
      src_x = vp->src.x;
      src_y = vp->src.y;
      src_w = vp->src.width;
      src_h = vp->src.height;
      surf_w = src_w;
      surf_h = src_h;
    }

    if (vp->dst.width > 0 && vp->dst.height > 0) {
      surf_w = vp->dst.width;
      surf_h = vp->dst.height;
    }
  }

  if (surf_w <= 0 || surf_h <= 0) return;

  matrix3_new(q.transform);

  matrix3_translate(q.transform, 0.5f, 0.5f);
  matrix3_rotate(q.transform, surface->active.transform);
  matrix3_translate(q.transform, -0.5f, -0.5f);

  matrix3_scale(q.transform, src_w / raw_w, src_h / raw_h);
  matrix3_translate(q.transform, src_x / raw_w, src_y / raw_h);

  if (surface->xdg_surface) {
    double crop_w = surface->xdg_surface->active.geo.width;
    double crop_h = surface->xdg_surface->active.geo.height;

    double crop_x = surface->xdg_surface->active.geo.x;
    double crop_y = surface->xdg_surface->active.geo.y;
    c_log(C_LOG_DEBUG, "%f %f %f %f", crop_w, crop_h, crop_x, crop_y);

    matrix3_translate(q.transform, crop_x / surf_w, crop_y / surf_h);
    matrix3_scale(q.transform, crop_w / surf_w, crop_h / surf_h);

    surf_x = crop_x;
    surf_y = crop_y;
    surf_w = crop_w;
    surf_h = crop_h;

  }


  q.x = window ? window->x : x;
  q.y = window ? window->y : y;
  q.width = surf_w * scale;
  q.height = surf_h * scale;

  c_list_push(quads, &q, sizeof(q));

  if (surface->sub.children) {
    struct c_wl_subsurface *ss;
    c_list_for_each(surface->sub.children, ss) {
      collect_window_tree(NULL, ss->surface, scale,
                          q.x - (ss->x - surf_x) * scale,
                          q.y - (ss->y - surf_y) * scale, quads);
    }
  }

  if (surface->xdg_surface && surface->xdg_surface->children) {
    struct c_xdg_surface *xs;
    c_list_for_each(surface->xdg_surface->children, xs) {
      collect_window_tree(NULL, xs->surface, scale,
                          q.x - (xs->popup.x - xs->active.geo.x) * scale,
                          q.y - (xs->popup.y - xs->active.geo.y) * scale, quads);
    }
  }
}


static int on_redraw(struct c_output_manager *mgr, struct c_output *output, void *userdata) {
  struct c_scene *scene = userdata;
  c_renderer_begin(mgr->renderer, output);

  struct c_scene_node *node;
  c_list_for_each(scene->nodes, node) {
    if (!node->is_visible) continue;

    if (node->type == C_SCENE_NODE_WINDOW) {
      struct c_renderer_quad *quad;
      c_list_for_each(node->quads, quad) {
        c_renderer_draw(mgr->renderer, output, quad);
      }

    } else if (node->type == C_SCENE_NODE_SURFACE) {
      struct c_renderer_quad *quad = node->quad;
      struct c_scene_surface *surf = node->data;
      if (quad->buffer) {
        quad->buffer = NULL;
      }

      struct c_wl_buffer *buffer = surf->surface->active.buffer;
      if (!buffer) continue;

      int32_t w, h;
      get_surface_buf_size(surf->surface, &w, &h);

      if (surf->width)  w = surf->width;
      if (surf->height) h = surf->height;

      if (buffer->shm) {
        quad->buffer = buffer->shm;
        quad->buffer_type = C_BUFFER_RAW;
      } else {
        quad->buffer = buffer->dma;
        quad->buffer_type = C_BUFFER_DMA;
      }

      quad->width = w;
      quad->height = h;

      quad->x = surf->x;
      quad->y = surf->y;

      c_renderer_draw(mgr->renderer, output, node->quad);

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
  node->is_visible = 1;

  if (node->type == C_SCENE_NODE_WINDOW)
    node->quads = c_list_new();

  return node;
}

static void node_insert(struct c_scene *scene, struct c_scene_node *node) {
  c_list *l = scene->nodes;
  for (; l->next; l = l->next) {
     struct c_scene_node *n = l->data;
     if (n->z > node->z) break;
  }
  c_list_insert2(&scene->nodes, l, node, 0);
}

static void node_remove(struct c_scene_node *node) {
  if (node->type == C_SCENE_NODE_WINDOW) {
    unref_window_quads(node);
    c_list_destroy(node->quads);

  } else if (node->type == C_SCENE_NODE_SURFACE) {
    free(node->quad);

  } else if (node->type == C_SCENE_NODE_RECT) {
    free(node->quad);

  } else if (node->type == C_SCENE_NODE_BUFFER) {
    c_unref(node->quad->buffer);
    free(node->quad);
  }

  free(node);
}

struct c_scene *c_scene_init(struct c_output_manager *mgr) {
  struct c_scene *scene = calloc(1, sizeof(*scene));
  if (!scene) {
    c_log_errno(C_LOG_ERROR, "failed to allocate c_scene");
    return NULL;
  }

  output_events.schedule = on_redraw;
  struct c_output *o;
  c_list_for_each(mgr->outputs, o)
    c_output_listen(o, &output_events, scene);

  scene->nodes = c_list_new();

  for (int i = 0; i < 5; i++)
    scene->z_state[i] = C_SCENE_LAYER_SLICE * i + 1;

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
  struct c_scene_node *node = node_create(C_SCENE_NODE_WINDOW, window);
  node->z = scene->z_state[C_SCENE_LAYER_NORMAL]++;
  node->layer = C_SCENE_LAYER_NORMAL;
  node_insert(scene, node);
  return node;
}

struct c_scene_node *c_scene_add_rect(struct c_scene *scene, struct c_scene_rect *rect) {
  struct c_renderer_quad *quad = calloc(1, sizeof(*quad));
  create_rect_quad(rect, quad);

  struct c_scene_node *node = node_create(C_SCENE_NODE_RECT, rect);
  node->quad = quad;
  node->z = scene->z_state[rect->layer]++;
  node->layer = rect->layer;

  node_insert(scene, node);
  return node;
}

struct c_scene_node *c_scene_add_buffer(struct c_scene *scene, struct c_scene_buffer *buf) {
  struct c_renderer_quad *quad = calloc(1, sizeof(*quad));
  create_buffer_quad(buf, quad);

  struct c_scene_node *node = node_create(C_SCENE_NODE_BUFFER, buf);

  node->quad = quad;
  node->z = scene->z_state[buf->layer]++;
  node->layer = buf->layer;

  node_insert(scene, node);
  return node;
}

struct c_scene_node *c_scene_add_surface(struct c_scene *scene, struct c_scene_surface *surface) {
  struct c_renderer_quad *quad = calloc(1, sizeof(*quad));
  create_surface_quad(surface, quad);

  struct c_scene_node *node = node_create(C_SCENE_NODE_SURFACE, surface);
  node->quad = quad;
  node->z = scene->z_state[surface->layer]++;
  node->layer = surface->layer;

  node_insert(scene, node);
  return node;
}

void c_scene_node_update(struct c_scene_node *node) {
  if (node->type == C_SCENE_NODE_WINDOW) {
    unref_window_quads(node);
    c_list_clear(node->quads);

    struct c_window *window = node->data;
    collect_window_tree(window, window->surface->surface, window->scale, window->x, window->y, node->quads);
    ref_window_quads(node);

  } else if (node->type == C_SCENE_NODE_RECT) {
    create_rect_quad(node->data, node->quad);

  } else if (node->type == C_SCENE_NODE_BUFFER) {
    struct c_rawbuf *raw = node->quad->buffer; 
    raw->dirty = 1;
    create_buffer_quad(node->data, node->quad);

  } else if (node->type == C_SCENE_NODE_SURFACE) {
    struct c_renderer_quad *quad = node->quad;
    struct c_scene_surface *surf = node->data;
    create_surface_quad(surf, quad);
  }
}

void c_scene_node_move(struct c_scene_node *node, double dx, double dy) {
  if (node->type == C_SCENE_NODE_WINDOW) {
    struct c_renderer_quad *quad;
    c_list_for_each(node->quads, quad) {
      quad->x += dx; 
      quad->y += dy;
    }

  } else {
    node->quad->x += dx; 
    node->quad->y += dy;
  }

}

void c_scene_node_remove(struct c_scene *scene, struct c_scene_node *node) {
  node_remove(node);
  c_list_remove(&scene->nodes, node);
}

void c_scene_node_raise(struct c_scene *scene, struct c_scene_node *node) {
  node->z = scene->z_state[node->layer]++;
  c_list_remove(&scene->nodes, node);
  node_insert(scene, node);
}
