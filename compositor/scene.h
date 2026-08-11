#ifndef CUTS_COMPOSITOR_SCENE_H
#define CUTS_COMPOSITOR_SCENE_H

#include <stdint.h>

#include "compositor/window.h"
#include "output/output.h"
#include "util/list.h"

#define MAX_QUADS 1024

enum c_scene_node_types {
  C_SCENE_WINDOW,
  C_SCENE_RECT,
};

struct c_scene_rect {
  double x, y;
  uint32_t width, height;
  float color[4];
};

struct c_scene_node {
  enum c_scene_node_types type;
  union {
    c_list *quads;
    struct c_renderer_quad *quad;
  };
  void *data;
  int is_hidden;
};

struct c_scene {
	c_list *nodes;
};

struct c_scene *c_scene_init(struct c_output_manager *mgr);
void c_scene_free(struct c_scene *scene);

struct c_scene_node *c_scene_add_window(struct c_scene *scene, struct c_window *window);
struct c_scene_node *c_scene_add_rect(struct c_scene *scene, struct c_scene_rect *rect);
// struct c_scene_node *c_scene_add_buffer(struct c_scene *scene, void *buffer);

void c_scene_node_update(struct c_scene_node *node);
void c_scene_node_remove(struct c_scene *scene, struct c_scene_node *node);
void c_scene_node_raise(struct c_scene *scene, struct c_scene_node *node);

#endif
