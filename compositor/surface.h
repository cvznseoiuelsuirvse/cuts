#ifndef CUTS_COMPOSITOR_SURFACE_H
#define CUTS_COMPOSITOR_SURFACE_H

#include <xkbcommon/xkbcommon.h>
#include "wayland/impl/wayland.h"

void c_surface_enter(struct c_wl_surface *surface, double mx, double my);
void c_surface_enter_pointer(struct c_wl_surface *surface, double mx, double my);
void c_surface_enter_keyboard(struct c_wl_surface *surface);

void c_surface_leave(struct c_wl_surface *surface);
void c_surface_leave_pointer(struct c_wl_surface *surface);
void c_surface_leave_keyboard(struct c_wl_surface *surface);

void c_surface_pointer_move(struct c_wl_connection *conn, double x, double y);
void c_surface_pointer_button(struct c_wl_connection *conn, uint32_t button, int pressed);
void c_surface_pointer_scroll(struct c_wl_connection *conn, double axis, double axis120,
                             enum wl_pointer_axis_source_enum axis_source,
                             enum wl_pointer_axis_enum axis_orient,
                             int axis_discrete);
void c_surface_keyboard_key(struct c_wl_connection *conn, int32_t key, int pressed, 
		xkb_mod_mask_t mods_depressed, xkb_mod_mask_t mods_latched, xkb_mod_mask_t mods_locked, 
		xkb_layout_index_t group, int send_mods);

#endif
