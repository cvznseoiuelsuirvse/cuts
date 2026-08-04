#ifndef CUTS_WAYLAND_DISPLAY_H
#define CUTS_WAYLAND_DISPLAY_H

#include "wayland/server.h"
#include "util/event_loop.h"

typedef void*(*c_wl_display_on_bind)(struct c_wl_connection *, struct c_wl_object *, void *);
typedef int(*c_wl_listener_handler)(struct c_wl_connection *, c_wl_args, void *);

struct c_wl_display_supported_iface {
  struct c_wl_interface *iface;
  c_wl_display_on_bind on_bind;
  void *on_bind_userdata;
};

struct c_wl_display {
	char 	 socket_path[108];
	struct c_event_resource *resource;
	c_list *listeners;
	c_list *supported_ifaces;
  c_list *connections;
};

struct c_wl_display *c_wl_display_init(struct c_event_loop *loop);
void c_wl_display_add_supported_interface(struct c_wl_display *display, const char *name, c_wl_display_on_bind on_bind, void *userdata);
void c_wl_display_add_listener(struct c_wl_display *display, const char *iface,
                               void *listeners, size_t listeners_n,
                               void *userdata);
void c_wl_display_get_listener(struct c_wl_display *display, const char *iface,
                               void ***handlers, void **userdata);
void c_wl_display_free(struct c_wl_display *display);

#endif
