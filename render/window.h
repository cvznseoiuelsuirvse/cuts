#ifndef CUTS_RENDER_WINDOW_H
#define CUTS_RENDER_WINDOW_H

#include <stdint.h>

#include "wayland/types/wayland.h"
#include "wayland/types.h"
#include "backend/input.h"

#define POINTER_INSIDE(px, py, area) \
	(((area)->x + (area)->width + (area)->y + (area)->x == 0) || \
	((area)->x <= (px) && (px) <= (area)->x + (area)->width &&   \
	 (area)->y <= (py) && (py) <= (area)->y + (area)->height)) \

enum c_window_states {
	C_WINDOW      = 	  1 << 0,
	C_WINDOW_FLOAT = 	  1 << 1,
	C_WINDOW_FULLSCREEN = 1 << 2,
};

struct c_window {
  struct c_wl_surface *wl_surface;
  uint32_t width, height;
  int32_t x, y;

  char title[256];
  char app_id[256];

  enum c_window_states state;
};

void c_window_resize(struct c_window *window, uint32_t width, uint32_t height, int activate);
void c_window_move(struct c_window *window);
void c_window_hide(struct c_window *window);
void c_window_activate(struct c_window *window);
void c_window_focus(struct c_window *window, c_wl_object_id wl_keyboard, c_wl_object_id wl_pointer);
void c_window_deactivate(struct c_window *window);
void c_window_unfocus(struct c_window *window, c_wl_object_id wl_keyboard, c_wl_object_id wl_pointer);
void c_window_close(struct c_window *window);

void c_window_pointer_move(struct c_window *window, c_wl_object_id wl_pointer, double px, double py);
void c_window_pointer_button(struct c_window *window, c_wl_object_id wl_pointer, enum c_input_mouse_buttons button, int pressed);
void c_window_pointer_scroll(struct c_window *window, c_wl_object_id wl_pointer, double axis,
                           enum wl_pointer_axis_source_enum axis_source, int axis_discrete);

#endif
