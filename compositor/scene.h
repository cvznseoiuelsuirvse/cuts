#ifndef CUTS_COMPOSITOR_SCENE_H
#define CUTS_COMPOSITOR_SCENE_H

#include <stdint.h>

#include "compositor/window.h"
#include "render/types.h"
#include "output/output.h"
#include "util/list.h"

#define MAX_QUADS 1024

struct c_scene_buffer {
  double x, y;
  uint32_t width, height;
  uint8_t *buffer;
  struct c_rawbuf raw;  
};

struct c_scene_rect {
  float color[4];
  double x, y;
  uint32_t width, height;
};


struct c_scene;
struct c_scene *c_scene_init(struct c_output_manager *mgr);
void c_scene_free(struct c_scene *scene);

struct c_scene_node *c_scene_add_window(struct c_scene *scene, struct c_window *window);
struct c_scene_node *c_scene_add_rect(struct c_scene *scene, struct c_scene_rect *rect);
struct c_scene_node *c_scene_add_buffer(struct c_scene *scene, struct c_scene_buffer *buffer);

struct c_scene_node;
typedef void (*collect_quads_function)(struct c_scene_node *node, c_list *quads);
void c_scene_node_update(struct c_scene_node *node);
void c_scene_node_remove(struct c_scene *scene, struct c_scene_node *node);
void c_scene_node_raise(struct c_scene *scene, struct c_scene_node *node);
void c_scene_node_set_visibility(struct c_scene_node *node, int is_visible);
void c_scene_node_set_collect(struct c_scene_node *node, collect_quads_function func);
void *c_scene_node_data(struct c_scene_node *node);

#endif
