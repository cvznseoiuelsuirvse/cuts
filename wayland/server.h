#ifndef CUTS_SERVER_H
#define CUTS_SERVER_H

#include <time.h>

#include "util/map.h"
#include "util/list.h"
#include "util/bitmap.h"

#define WL_MAX_INTERFACES 2048

#define WL_REQUEST __attribute__((weak)) int
#define WL_EVENT int

#define WL_HEADER_SIZE 8
#define WL_CONN_BUFFER_SIZE 2048
#define WL_MAX_STRING_SIZE (WL_CONN_BUFFER_SIZE - WL_HEADER_SIZE - 4) // 4 -> string prefix size
#define WL_INTERFACE_REGISTER(name, interface, version, nrequests, ...)                       \
  const struct wl_interface name = {interface, version, nrequests, { __VA_ARGS__ }}; \
  __attribute__((constructor))                                  \
  static void name##_register(void) {wl_interface_add(&name); }

#define WL_CHECK_IF_REGISTERED(id, object) \
  (object) = wl_object_get(conn, (id)); \
  if (!(object)) return wl_error_set(args[0].u, WL_DISPLAY_ERROR_INVALID_OBJECT, "object %d not registered", (id))

#define WL_CHECK_IF_NOT_REGISTERED(id, object) \
  (object) = wl_object_get(conn, (id)); \
  if ((object)) return wl_error_set(args[0].u, WL_DISPLAY_ERROR_INVALID_OBJECT, "object %d already registered", (id))

#define WL_SERIAL (uint32_t)clock()

typedef long 		  	wl_int;
typedef unsigned long 	wl_uint;
typedef float 			wl_fixed;
typedef unsigned long	wl_object_id;
typedef unsigned long	wl_new_id;
typedef const char	   *wl_string;
typedef uint8_t  	   *wl_array;
typedef int     	    wl_fd;
typedef int 			wl_enum;

struct wl_display {
	char 	 socket_path[108];
	struct c_event_resource *resource;
	struct c_event_loop *loop;
};

struct wl_callback {
	wl_object_id callback_id;
	wl_object_id target_id;
};

struct wl_connection {
	c_map 		*objects;
	c_list 		*callback_queue;

	c_bitmap 	*client_id_pool;
	c_bitmap 	*server_id_pool;

	wl_fd 		 msg_fd;
	int 		 client_fd;
};

struct wl_object {
	wl_object_id id;
	const struct wl_interface *iface;
	void *data;
};

union wl_arg {
	wl_int 	  i;
	wl_uint   u;
	wl_fixed  f;
	wl_new_id n;
	wl_array  a;
	wl_fd     F;
	wl_enum   e;
	wl_object_id o;
	char      s[WL_MAX_STRING_SIZE];
};

struct wl_message {
	wl_object_id  id;
	uint32_t 	  op;
	wl_fd		  fd;
	char 		  signature[16];
};

typedef int (*wl_request_handler)(struct wl_connection *, union wl_arg *);
struct wl_request {
	char   name[256];
	wl_request_handler handler;
	size_t nargs;
	char   signature[16];
};

struct wl_interface {
	char 	name[256];
	wl_uint version;
	size_t 	nrequests;
	struct wl_request requests[];
};


struct wl_shm {
	int 	 	 fd;
	uint8_t		*buffer;
	wl_int		 size;
};

struct wl_buffer {
	struct wl_shm *shm;
	wl_object_id id;

	wl_int 		offset;
	wl_int 		width;
	wl_int 		height;
	wl_int 		stride;
	wl_int 		format;
};

enum wl_surface_roles {
	WL_SURFACE_TOPLEVEL = 1,
	WL_SURFACE_POPUP,
};

struct wl_surface {
	struct wl_buffer 	*pending;
	struct wl_buffer 	*active;
	wl_object_id 		 id;
	wl_int 				 width,  x;
	wl_int 				 height, y;
	int 				 damaged;
	enum wl_surface_roles role;

	struct wl_connection *conn;
};

struct wl_xdg_surface {
	struct wl_surface *surface;
	wl_uint			   serial;
};

struct wl_display *wl_display_init();
void wl_display_free(struct wl_display *display);

void wl_interface_add(const struct wl_interface *interface);
const struct wl_interface *wl_interface_get(const char *interface_name);

int wl_object_add(struct wl_connection *conn, wl_new_id id, const struct wl_interface *interface, void *data);
struct wl_object *wl_object_get(struct wl_connection *conn, wl_object_id id);
int wl_object_del(struct wl_connection *conn, wl_object_id id);

struct wl_connection *wl_connection_init(int client_fd);
int wl_connection_free(struct wl_connection *conn);
int wl_connection_send(struct wl_connection *conn, struct wl_message *msg, size_t nargs, ...);
int wl_connection_dispatch(struct wl_connection *conn);
void wl_connection_callback_add(struct wl_connection *conn, wl_object_id target_id, wl_object_id callback_id);
void wl_connection_callback_done(struct wl_connection *conn, wl_object_id target_id);

#endif
