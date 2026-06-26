#ifndef CUTS_COMPOSITOR_SCENE_H
#define CUTS_COMPOSITOR_SCENE_H

#include <stdint.h>

#include "compositor/window.h"
#include "wayland/types.h"

#define C_SCENE_MAX_WINDOWS 256

struct c_scene_quad {
	struct c_wl_buffer *buffer;
	int32_t  x, y;
	uint32_t width, height;

  float    border_color[4];
  uint32_t border_width;

};

void c_scene_init(uint32_t width, uint32_t height);
void c_scene_destroy();

void c_scene_add_window(struct c_window *window);
void c_scene_remove_window(struct c_window *window);

int c_scene_collect(struct c_scene_quad *out, int max_quads);
void c_scene_clear();

void c_scene_set_background(float color[4]);

#endif
