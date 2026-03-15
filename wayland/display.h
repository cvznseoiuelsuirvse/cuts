#ifndef CUTS_WAYLAND_DISPLAY_H
#define CUTS_WAYLAND_DISPLAY_H

#include "wayland/server.h"

enum c_wl_display_notifer {
	C_WL_DISPLAY_ON_SURFACE_NEW,
	C_WL_DISPLAY_ON_SURFACE_DESTROY,
	C_WL_DISPLAY_ON_SURFACE_UPDATE,
	C_WL_DISPLAY_ON_WINDOW_NEW,
	C_WL_DISPLAY_ON_WINDOW_CLOSE,
};

struct c_wl_display_listener {
	int (*on_surface_new)    (struct c_wl_surface *, void *);
	int (*on_surface_destroy)(struct c_wl_surface *, void *);
	int (*on_surface_update) (struct c_wl_surface *, void *);
	int (*on_window_new)	 (struct c_wl_surface *, void *);
	int (*on_window_close)	 (struct c_wl_surface *, void *);
};

struct c_wl_display {
	char 	 socket_path[108];
	struct c_event_resource *resource;
	struct c_event_loop *loop;
	
	c_list *listeners; // struct c_wl_display_listener;
};

void c_wl_display_free(struct c_wl_display *display);
struct c_wl_display *c_wl_display_init();
void c_wl_display_add_listener(struct c_wl_display *display, struct c_wl_display_listener *listener, void *userdata);
void c_wl_display_notify(struct c_wl_display *display, struct c_wl_surface *surface, enum c_wl_display_notifer notifier);

#endif
