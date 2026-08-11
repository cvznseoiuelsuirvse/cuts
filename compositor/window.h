#ifndef CUTS_COMPOSITOR_WINDOW_H
#define CUTS_COMPOSITOR_WINDOW_H

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

#include "wayland/proto/wayland.h"
#include "output/output.h"

enum c_window_states {
	C_WINDOW_FLOAT        = 1 << 1,
	C_WINDOW_FULLSCREEN   = 1 << 2,
};

struct c_window {
  double x;
  double y;
  uint32_t width;
  uint32_t height;

  char **title;
  char **app_id;

  enum c_window_states state;

  struct c_wl_connection *conn;
  struct c_xdg_surface *surface;
};

void c_window_focus(struct c_window *window, double hotspot_x, double hotspot_y);
void c_window_activate(struct c_window *window);
void c_window_deactivate(struct c_window *window);
void c_window_unfocus(struct c_window *window);
void c_window_close(struct c_window *window);

void c_window_pointer_move(struct c_window *window, double x, double y);
void c_window_pointer_button(struct c_window *window, uint32_t button, int pressed);
void c_window_pointer_scroll(struct c_window *window, double axis, double axis120,
                           enum wl_pointer_axis_source_enum axis_source, int axis_discrete);

void c_window_keyboard_key(struct c_window *window, int32_t key, int pressed, 
		xkb_mod_mask_t mods_depressed, xkb_mod_mask_t mods_latched, xkb_mod_mask_t mods_locked, 
		xkb_layout_index_t group, int send_mods);

void c_window_move_to_output(struct c_window *window, struct c_output *output);

#endif
