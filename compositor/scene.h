#ifndef CUTS_COMPOSITOR_SCENE_H
#define CUTS_COMPOSITOR_SCENE_H

#include <stdint.h>

#include "compositor/window.h"
#include "wayland/types.h"

#define C_SCENE_MAX_WINDOWS 16

struct c_scene_quad {
	struct c_wl_buffer *buffer;
	int32_t  x, y;
	uint32_t width, height;

  float uv_offset[2];
  float uv_scale[2];

  float    border_color[4];
  uint32_t border_width;
};

struct c_scene {
	c_list *windows;
  float bg_color[4];
};

struct c_scene *c_scene_init(struct c_output_manager *mgr);
void c_scene_free(struct c_scene *scene);

void c_scene_add_window(struct c_scene *scene, struct c_window *window);
void c_scene_remove_window(struct c_scene *scene, struct c_window *window);
void c_scene_clear(struct c_scene *scene, struct c_output *output);

void c_scene_set_background(struct c_scene *scene, float color[4]);

#endif
