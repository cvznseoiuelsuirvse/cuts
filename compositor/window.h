#ifndef CUTS_COMPOSITOR_WINDOW_H
#define CUTS_COMPOSITOR_WINDOW_H

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

#include "wayland/impl/wayland.h"

enum c_window_states {
	C_WINDOW_FLOAT        = 1 << 1,
	C_WINDOW_FULLSCREEN   = 1 << 2,
};

struct c_window {
  double x;
  double y;
  uint32_t width;
  uint32_t height;
  double scale;

  char **title;
  char **app_id;

  enum c_window_states state;
  uint32_t xdg_states;

  struct c_wl_connection *conn;
  struct c_xdg_surface *surface;
  struct c_wl_surface *focused;
};

struct c_scene;
struct c_window *c_window_new(struct c_wl_connection *connection, struct c_xdg_surface *surface);
void c_window_free(struct c_window *window);

void c_window_activate(struct c_window *window);
void c_window_deactivate(struct c_window *window);
void c_window_focus(struct c_window *window, double mx, double my);
void c_window_unfocus(struct c_window *window);
void c_window_close(struct c_window *window);
void c_window_pointer_move(struct c_window *window, double x, double y);

struct c_wl_surface *c_window_surface_at(struct c_window *window, double x, double y, double *lx, double *ly);

#endif
