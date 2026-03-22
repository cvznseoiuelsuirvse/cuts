#ifndef CUTS_WAYLAND_SERVER_H
#define CUTS_WAYLAND_SERVER_H

#include <time.h>

#include "wayland/types.h"

#include "util/map.h"
#include "util/list.h"
#include "util/bitmap.h"

#define C_WL_MAX_INTERFACES 2048
#define C_WL_HEADER_SIZE 8
#define C_WL_BUFFER_SIZE 4096
#define C_WL_STRING_SIZE (C_WL_BUFFER_SIZE - C_WL_HEADER_SIZE - 4) // 4 -> string prefix size
  
#define C_WL_INTERFACE_REGISTER(name, interface, version, nrequests, ...)                       \
  struct c_wl_interface name = {interface, version, nrequests, { __VA_ARGS__ }}; \
  __attribute__((constructor))                                  \
  static void name##_register(void) {c_wl_interface_add(&name); }

#define C_WL_CHECK_IF_REGISTERED(id, object) \
  (object) = c_wl_object_get(conn, (id)); \
  if (!(object)) return c_wl_error_set(args[0].u, 0, "object %d not registered", (id))

#define C_WL_CHECK_IF_NOT_REGISTERED(id, object) \
  (object) = c_wl_object_get(conn, (id)); \
  if ((object)) return c_wl_error_set(args[0].u, 0, "object %d already registered", (id))

struct c_wl_connection {
	int 	client_fd;

	c_map  *objects;
	c_list *callback_queue;

	c_bitmap 	*client_id_pool;
	c_bitmap 	*server_id_pool;

	struct c_wl_display *dpy;
};

struct c_wl_object {
	c_wl_object_id id;
	const struct c_wl_interface *iface;
	void *data;
};

union c_wl_arg {
	c_wl_int 	  i;
	c_wl_uint   u;
	c_wl_fixed  f;
	c_wl_new_id n;
	c_wl_array  *a;
	c_wl_fd     F;
	c_wl_enum   e;
	c_wl_object_id o;
	char      s[C_WL_STRING_SIZE];
};

struct c_wl_message {
	c_wl_object_id id;
	uint32_t 	   op;
	const char 	   signature[16];
	const char 	   event_name[128];
};

typedef int (*c_wl_request_handler)(struct c_wl_connection *, union c_wl_arg *, void *);
struct c_wl_request {
	char    name[256];
	c_wl_request_handler handler;
	size_t  nargs;
	char    signature[16];
};

struct c_wl_interface {
	char 	name[256];
	c_wl_uint version;
	size_t 	nrequests;
	struct c_wl_request requests[];
};

void c_wl_interface_add(struct c_wl_interface *interface);
struct c_wl_interface *c_wl_interface_get(const char *interface_name);

int c_wl_object_add(struct c_wl_connection *conn, c_wl_new_id id, const struct c_wl_interface *interface, void *data);
struct c_wl_object *c_wl_object_get(struct c_wl_connection *conn, c_wl_object_id id);
int c_wl_object_del(struct c_wl_connection *conn, c_wl_object_id id);

struct c_wl_connection *c_wl_connection_init(int client_fd, struct c_wl_display *display);
int  c_wl_connection_free(struct c_wl_connection *conn);
int  c_wl_connection_send(struct c_wl_connection *conn, struct c_wl_message *msg, size_t nargs, ...);
int  c_wl_connection_dispatch(struct c_wl_connection *conn);
int  c_wl_connection_callback_add(struct c_wl_connection *conn, c_wl_object_id callback_id, c_wl_object_id target_id);
void c_wl_connection_callback_done(struct c_wl_connection *conn, c_wl_object_id target_id);

#endif
