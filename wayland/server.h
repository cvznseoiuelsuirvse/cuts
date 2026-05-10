#ifndef CUTS_WAYLAND_SERVER_H
#define CUTS_WAYLAND_SERVER_H

#define DISPATCH_PROTO_ERR  -1
#define DISPATCH_FATAL_ERR  -2
#define DISPATCH_CLIENT_ERR -3

#include <time.h>

#include "wayland/types.h"
#include "util/map.h"

  
#define C_WL_INTERFACE_REGISTER(name, interface, version, nrequests, ...)                       \
  struct c_wl_interface name = {interface, version, nrequests, { __VA_ARGS__ }}; \
  __attribute__((constructor))                                  \
  static void name##_register(void) {c_wl_interface_add(&name); }

#define C_WL_CHECK_IF_REGISTERED(id, object) \
  (object) = c_wl_object_get(conn, (id)); \
  if (!(object)) c_wl_error_set_and_return(args[0].u, 0, "object %d not registered", (id))

#define C_WL_CHECK_IF_NOT_REGISTERED(id, object) \
  (object) = c_wl_object_get(conn, (id)); \
  if ((object)) c_wl_error_set_and_return(args[0].u, 0, "object %d already registered", (id))

#define c_wl_objects_for_each(conn, obj) \
	__attribute__((unused)) uint64_t __key; \
	c_map_for_each(c_wl_connection_get_objects(conn), __key, obj) \

#define c_wl_error_set_and_return(object_id, code, msg, ...) \
  do { \
  _c_wl_error_set(object_id, code, msg __VA_OPT__(,) __VA_ARGS__); \
  return DISPATCH_PROTO_ERR; \
  } while (0); \

#define c_wl_self(conn, args) c_wl_object_get(conn, args[0].o)
	

struct c_wl_display;
struct c_wl_connection;

struct c_wl_object {
	c_wl_object_id id;
  uint32_t version;
  void *data;
	const struct c_wl_interface *iface;
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

typedef int (*c_wl_request_handler)(struct c_wl_connection *, union c_wl_arg *);
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

int c_wl_object_add(struct c_wl_connection *conn, c_wl_new_id id,
                    uint32_t version, const struct c_wl_interface *interface,
                    void *data);

struct c_wl_object *c_wl_object_get(struct c_wl_connection *conn,
                                    c_wl_object_id id);
int c_wl_object_del(struct c_wl_connection *conn, c_wl_object_id id);

struct c_wl_connection *c_wl_connection_init(int client_fd,
                                             struct c_wl_display *display);
int c_wl_connection_free(struct c_wl_connection *conn);
int c_wl_connection_send(struct c_wl_connection *conn, struct c_wl_message *msg,
                         size_t nargs, ...);
int c_wl_connection_dispatch(struct c_wl_connection *conn);
void c_wl_connection_callback_done(struct c_wl_connection *conn,
                                   c_wl_object_id id);
struct c_wl_display *c_wl_connection_get_dpy(struct c_wl_connection *conn);
struct c_map *c_wl_connection_get_objects(struct c_wl_connection *conn);

int _c_wl_error_set(c_wl_object_id object_id, c_wl_int code, c_wl_string msg,
                    ...);
void c_wl_error_send(struct c_wl_connection *conn);

int c_wl_serial();

#endif
