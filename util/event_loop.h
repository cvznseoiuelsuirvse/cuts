
#ifndef CUTS_SERVER_COMPOSITOR_H
#define CUTS_SERVER_COMPOSITOR_H

#define CUTS_MAX_EPOLL_EVENTS 512

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#include "../util/list.h"

typedef enum c_event_callback_status {
	C_EVENT_OK,
	C_EVENT_ERROR_FATAL,
	C_EVENT_ERROR_FD_GONE,
	C_EVENT_ERROR_WL_PROTO,
} c_event_callback_status;

#define C_EVENT_CALLBACK static c_event_callback_status

struct c_event_loop {
	int 		 epfd;
	c_list	*resources;
	c_list	*connections;
	struct epoll_event events[CUTS_MAX_EPOLL_EVENTS];
};

typedef c_event_callback_status(*c_event_callback)(struct c_event_loop *loop, int fd, void *userdata);

struct c_event_resource {
	int fd;
	c_event_callback callback;
	void *userdata;
};


struct c_event_loop * c_event_loop_init();
int c_event_loop_run(struct c_event_loop *loop);
int c_event_loop_add(struct c_event_loop *loop, int fd, c_event_callback callback, void *userdata);
int c_event_loop_del(struct c_event_loop *loop, struct c_event_resource *resource);
void c_event_loop_free(struct c_event_loop *loop);

#endif
