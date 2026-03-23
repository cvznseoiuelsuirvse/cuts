#ifndef CUTS_RENDER_WINDOW_H
#define CUTS_RENDER_WINDOW_H

#include <stdint.h>

enum c_window_states {
	C_WINDOW_FLOAT = 	  1,
	C_WINDOW_FULLSCREEN = 2,
};

struct c_window {
  struct c_wl_surface *wl_surface;
  uint32_t width, height;
  uint32_t x, y;

  char title[256];
  char app_id[256];

  enum c_window_states state;
};

void c_window_resize(struct c_window *window, uint32_t width, uint32_t height, int activate);
void c_window_move(struct c_window *window);
void c_window_hide(struct c_window *window);
void c_window_activate(struct c_window *window);
void c_window_deactivate(struct c_window *window);
void c_window_close(struct c_window *window);

#endif
