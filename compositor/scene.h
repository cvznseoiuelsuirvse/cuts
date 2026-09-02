#ifndef CUTS_COMPOSITOR_SCENE_H
#define CUTS_COMPOSITOR_SCENE_H

#include <stdint.h>

#include "compositor/window.h"
#include "render/types.h"
#include "output/output.h"

#define MAX_QUADS 1024

#define C_SCENE_CLIENT_BUFFER_RAW 1
#define C_SCENE_CLIENT_BUFFER_DMA 2


#define C_SCENE_LAYER_SLICE 0x33333333

enum c_scene_layers {
  C_SCENE_LAYER_BACKGROUND,
  C_SCENE_LAYER_BOTTOM,
  C_SCENE_LAYER_NORMAL,
  C_SCENE_LAYER_TOP,
  C_SCENE_LAYER_OVERLAY,
};

enum c_scene_node_types {
  C_SCENE_NODE_WINDOW,
  C_SCENE_NODE_RECT,
  C_SCENE_NODE_BUFFER,
  C_SCENE_NODE_SURFACE,
};

struct c_scene_buffer {
  enum c_scene_layers layer;
  double x, y;
  uint32_t width, height;
  uint8_t *buffer;
};

struct c_scene_surface {
  struct c_wl_object *obj;
  enum c_scene_layers layer;
  double x, y;
  uint32_t width, height;

  struct c_wl_surface *surface;
};

struct c_scene_rect {
  enum c_scene_layers layer;

  float color[4];
  double x, y;
  uint32_t width, height;
};

struct c_scene_node {
  enum c_scene_node_types type;
  enum c_scene_layers layer;
  union {
    c_list *quads; // struct c_renderer_quad *
    struct c_renderer_quad *quad;
  };
  void *data;
  int is_visible;
  uint64_t z;
};


struct c_scene;
struct c_scene *c_scene_init(struct c_output_manager *mgr);
void c_scene_free(struct c_scene *scene);

struct c_scene_node *c_scene_add_window(struct c_scene *scene, struct c_window *window);
struct c_scene_node *c_scene_add_rect(struct c_scene *scene, struct c_scene_rect *rect);
struct c_scene_node *c_scene_add_buffer(struct c_scene *scene, struct c_scene_buffer *buffer);
struct c_scene_node *c_scene_add_surface(struct c_scene *scene, struct c_scene_surface *surface);

struct c_scene_node;
void c_scene_node_update(struct c_scene_node *node);
void c_scene_node_move(struct c_scene_node *node, double dx, double dy);
void c_scene_node_remove(struct c_scene *scene, struct c_scene_node *node);
void c_scene_node_raise(struct c_scene *scene, struct c_scene_node *node);

#endif
