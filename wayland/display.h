#ifndef CUTS_WAYLAND_DISPLAY_H
#define CUTS_WAYLAND_DISPLAY_H

#include "wayland/server.h"

enum c_wl_display_notifier {
	C_WL_DISPLAY_ON_CLIENT_NEW,
	C_WL_DISPLAY_ON_CLIENT_GONE,

	C_WL_DISPLAY_ON_SURFACE_UPDATE,
	C_WL_DISPLAY_ON_SURFACE_DESTROY,

	C_WL_DISPLAY_ON_BUFFER_DESTROY,
};

struct c_wl_display_listener {
	int (*on_client_new)     (struct c_wl_connection *, void *);
	int (*on_client_gone)    (struct c_wl_connection *, void *);

	int (*on_surface_update) (struct c_wl_surface *, void *);
	int (*on_surface_destroy)(struct c_wl_surface *, void *);
	int (*on_buffer_destroy) (struct c_wl_buffer *, void *);
};

typedef void*(*c_wl_display_on_bind)(struct c_wl_connection *, c_wl_object_id, c_wl_uint, void *);

struct c_wl_display_supported_iface {
  struct c_wl_interface *iface;
  c_wl_display_on_bind on_bind;
  void *userdata;
};

struct c_wl_display {
	char 	 socket_path[108];
	struct c_event_resource *resource;
	struct c_event_loop *loop;
	
	c_list *listeners;
	c_list *supported_ifaces;
};

struct c_wl_display *c_wl_display_init();
void c_wl_display_add_supported_interface(struct c_wl_display *display, const char *name, c_wl_display_on_bind on_bind, void *userdata);
void c_wl_display_add_listener(struct c_wl_display *display, struct c_wl_display_listener *listener, void *userdata);
void c_wl_display_notify(struct c_wl_display *display, void *data, enum c_wl_display_notifier notifier);
void c_wl_display_free(struct c_wl_display *display);

#endif
