#ifndef CUTS_SERVER_H
#define CUTS_SERVER_H

#include "util/list.h"
#include "util/map.h"
#include "util/bitmap.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#define CUTS_MAX_EPOLL_EVENTS 512
#define CUTS_MAX_INTERFACES 1024
#define CUTS_STREQ(s1, s2) (strcmp((s1), (s2)) == 0)

#define WL_REQUEST __attribute__((weak)) int
#define WL_EVENT int

#define WL_CONN_BUFFER_SIZE 1024
#define WL_HEADER_SIZE 8
#define WL_CONN_SYNC_QUEUE_SIZE 128
#define WL_MAX_STRING_SIZE (WL_CONN_BUFFER_SIZE - WL_HEADER_SIZE - 4) // 4 -> string prefix size
#define WL_INTERFACE_REGISTER(name, interface, version, nrequests, ...)                       \
  const struct wl_interface name = {interface, version, nrequests, { __VA_ARGS__ }}; \
  __attribute__((constructor))                                  \
  static void name##_register(void) {c_interface_add_global(&name); }

typedef long 		  	wl_int;
typedef unsigned long 	wl_uint;
typedef float 			wl_fixed;
typedef unsigned long	wl_object;
typedef unsigned long	wl_new_id;
typedef const char	   *wl_string;
typedef char     	   *wl_array;
typedef int     	    wl_fd;
typedef int 			wl_enum;

typedef unsigned short	__wl_u16;

enum c_event_resource_type {
	SERVER,
	CLIENT,
	DRM,
};

struct c_event_resource {
	int fd;
	void *data;
	enum c_event_resource_type type;
};

struct c_event_loop {
	int 		 epfd;
	c_list	*resources;
	c_list	*connections;
	struct epoll_event events[CUTS_MAX_EPOLL_EVENTS];
};


struct wl_connection {
	c_map 		*objects;
	c_bitmap 	*client_id_pool;
	c_bitmap 	*server_id_pool;
	wl_fd 		 msg_fd;
	int 		 client_fd;
};

union wl_arg {
	wl_int 	  i;
	wl_uint   u;
	wl_fixed  f;
	wl_object o;
	wl_new_id n;
	char      s[WL_MAX_STRING_SIZE];
	wl_array  a;
	wl_fd     F;
	wl_enum   e;
};

struct wl_message {
	wl_object 	  id;
	__wl_u16 	  op;
	wl_fd		  fd;
	char 		  signature[16];
};

struct wl_event {};

typedef int (*wl_request_handler)(struct wl_connection *, union wl_arg *);
struct wl_request {
	char   name[256];
	wl_request_handler handler;
	size_t nargs;
	char   signature[16];
};

struct wl_interface {
	char name[256];
	wl_uint version;
	size_t nrequests;
	struct wl_request requests[];
};



void c_interface_add_global(const struct wl_interface *interface);
const struct wl_interface *c_interface_get_global(const char *interface_name);

int c_object_add(struct wl_connection *conn, wl_new_id id, const struct wl_interface *interface);
struct wl_interface *c_object_get(struct wl_connection *conn, wl_object id);
int c_object_del(struct wl_connection *conn, wl_object id);

int  c_create_socket(struct c_event_resource *sock);
void c_unlink_socket(struct c_event_resource *sock);
int wl_connection_send(struct wl_connection *conn, struct wl_message *msg, size_t nargs, ...);

struct c_event_loop *c_event_loop_init();
void c_event_loop_free(struct c_event_loop *loop);
int  c_event_loop_run(struct c_event_loop *loop);
int  c_event_loop_add(struct c_event_loop *loop, struct c_event_resource *resource);


#endif
