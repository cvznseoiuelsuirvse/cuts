#ifndef CUTS_WAYLAND_SERVER_H
#define CUTS_WAYLAND_SERVER_H

#include <time.h>

#include "util/map.h"
#include "util/list.h"
#include "util/bitmap.h"


#define C_WL_MAX_INTERFACES 2048

#define C_WL_REQUEST __attribute__((weak)) int
#define C_WL_EVENT int

#define C_WL_HEADER_SIZE 8
#define C_WL_CONN_BUFFER_SIZE 4096
#define C_WL_MAX_STRING_SIZE (C_WL_CONN_BUFFER_SIZE - C_WL_HEADER_SIZE - 4) // 4 -> string prefix size
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

#define C_WL_SERIAL (uint32_t)clock()

typedef long 		  		c_wl_int;
typedef unsigned long 		c_wl_uint;
typedef float 				c_wl_fixed;
typedef unsigned long		c_wl_object_id;
typedef unsigned long		c_wl_new_id;
typedef const char	   	   *c_wl_string;
typedef int     	    	c_wl_fd;
typedef int 				c_wl_enum;

typedef struct c_wl_array {
	c_wl_uint  size;
	void      *data;
} c_wl_array;

typedef unsigned short      c_wl_u16;
typedef unsigned long       c_wl_u32;
typedef unsigned long long  c_wl_u64;

struct c_wl_display {
	char 	 socket_path[108];
	struct c_event_resource *resource;
	struct c_event_loop *loop;
};

struct c_wl_connection {
	c_map  *objects;
	c_list *callback_queue;

	c_bitmap 	*client_id_pool;
	c_bitmap 	*server_id_pool;

	c_wl_fd req_fd;
	c_wl_fd event_fd;

	int 	client_fd;
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
	char      s[C_WL_MAX_STRING_SIZE];
};

struct c_wl_message {
	c_wl_object_id id;
	c_wl_u32 	   op;
	const char 		   signature[16];
	const char 		   event_name[128];
};

typedef int (*c_wl_request_handler)(struct c_wl_connection *, union c_wl_arg *, void *);
struct c_wl_request {
	char   name[256];
	c_wl_request_handler handler;
	void *handler_data;
	size_t nargs;
	char   signature[16];
};

struct c_wl_interface {
	char 	name[256];
	c_wl_uint version;
	size_t 	nrequests;
	struct c_wl_request requests[];
};


struct c_wl_shm {
	int 	 	 fd;
	uint8_t		*buffer;
	c_wl_int	 size;
};

enum c_wl_surface_roles {
	C_WL_SURFACE_TOPLEVEL = 1,
	C_WL_SURFACE_POPUP,
};


struct c_wl_dmabuf {
	c_wl_object_id params_id;
	int used;

	c_wl_fd   fd;
	c_wl_uint plane_idx;
	c_wl_uint format;
	c_wl_uint stride;
	c_wl_uint offset;  
	c_wl_u64  modifier;
	c_wl_int  flags;
};

struct c_wl_buffer {
	c_wl_object_id id;

	c_wl_int 		width;
	c_wl_int 		height;
	c_wl_int 		format;
	c_wl_int 		stride;
	c_wl_int 		offset;

	struct c_wl_shm    *shm;
	struct c_wl_dmabuf *dmabuf;
};

struct c_wl_region {
	c_wl_object_id id;
	c_wl_int width,  x;
	c_wl_int height, y;
};

struct c_wl_surface {
	c_wl_object_id  id;
	c_wl_uint 		serial;
	enum c_wl_surface_roles role;

	struct {
		c_wl_int width,  x;
		c_wl_int height, y;
		int		 damaged;
	} damage;

	struct {
		c_wl_int width,  max_width,  min_width,  x;
		c_wl_int height, max_height, min_height, y;
		char title[256];
		char app_id[256];
	} xdg;

	struct c_wl_region *opaque;
	struct c_wl_region *input;

	struct c_wl_buffer 	*pending;
	struct c_wl_buffer 	*active;

	struct c_wl_connection *conn;
};

struct c_wl_display *c_wl_display_init();
void c_wl_display_free(struct c_wl_display *display);

void c_wl_interface_add(struct c_wl_interface *interface);
struct c_wl_interface *c_wl_interface_get(const char *interface_name);

int c_wl_object_add(struct c_wl_connection *conn, c_wl_new_id id, const struct c_wl_interface *interface, void *data);
struct c_wl_object *c_wl_object_get(struct c_wl_connection *conn, c_wl_object_id id);
int c_wl_object_del(struct c_wl_connection *conn, c_wl_object_id id);

struct c_wl_connection *c_wl_connection_init(int client_fd);
int  c_wl_connection_free(struct c_wl_connection *conn);
int  c_wl_connection_send(struct c_wl_connection *conn, struct c_wl_message *msg, size_t nargs, ...);
int  c_wl_connection_dispatch(struct c_wl_connection *conn);
int  c_wl_connection_callback_add(struct c_wl_connection *conn, c_wl_object_id callback_id, c_wl_object_id target_id);
void c_wl_connection_callback_done(struct c_wl_connection *conn, c_wl_object_id target_id);

#endif
