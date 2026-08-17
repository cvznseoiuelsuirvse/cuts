#ifndef CUTS_WAYLAND_DISPLAY_H
#define CUTS_WAYLAND_DISPLAY_H

#include "wayland/server.h"
#include "util/event_loop.h"

#define C_WL_INTERFACE_REGISTER(interface, version, n_requests, destructor,    \
                                ...)                                           \
  struct c_wl_interface interface##_interface = {                              \
      #interface, version,    0,          NULL,                                \
      NULL,       destructor, n_requests, {__VA_ARGS__}};                      \
  __attribute__((constructor)) static void interface##_register(void) {        \
    c_wl_interface_add(&interface##_interface);                                \
  }

typedef int(*c_wl_interface_listener_handler)(struct c_wl_connection *, c_wl_args, void *);
typedef void*(*c_wl_interface_on_bind)(struct c_wl_connection *, struct c_wl_object *, void *);
struct c_wl_interface {
	char name[256];
	c_wl_uint version;

  int is_supported;
  c_wl_interface_on_bind on_bind;
  void *on_bind_userdata;

  int destructor_request;
	size_t 	n_requests;
	struct c_wl_request requests[];
};

void c_wl_interface_add(const struct c_wl_interface *interface);
const struct c_wl_interface *c_wl_interface_get(const char *interface_name);
void c_wl_interface_support(const char *name, c_wl_interface_on_bind on_bind, void *userdata);
size_t c_wl_interface_get_all(const struct c_wl_interface ***interfaces);

typedef void(*c_wl_display_connection_handler)(struct c_wl_connection *, void *);
struct c_wl_display_connection_listener {
  c_wl_display_connection_handler new;
  c_wl_display_connection_handler gone;
  
};
struct c_wl_display;

struct c_wl_display *c_wl_display_init(struct c_event_loop *loop);
void c_wl_display_free(struct c_wl_display *display);

void c_wl_display_add_interface_listener(struct c_wl_display *display,
                                         const char *iface, void *listeners, void *userdata);
void c_wl_display_get_interface_listener(struct c_wl_display *display,
                                         const char *iface, void ***handlers,
                                         void **userdata);

void c_wl_display_add_connection_listener(
    struct c_wl_display *display,
    struct c_wl_display_connection_listener *listener, void *userdata);
#endif
