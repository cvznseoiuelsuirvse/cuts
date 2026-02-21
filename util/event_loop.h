
#ifndef CUTS_SERVER_COMPOSITOR_H
#define CUTS_SERVER_COMPOSITOR_H

#define CUTS_MAX_EPOLL_EVENTS 512

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#include "../util/list.h"

typedef enum c_event_callback_errno {
	C_EVENT_OK,
	C_EVENT_ERROR_FATAL,
	C_EVENT_ERROR_WL_CLIENT_GONE,
	C_EVENT_ERROR_WL_PROTO,
} c_event_callback_errno;

struct c_event_loop {
	int 		 epfd;
	c_list	*resources;
	c_list	*connections;
	struct epoll_event events[CUTS_MAX_EPOLL_EVENTS];
};

typedef c_event_callback_errno(*c_event_callback)(struct c_event_loop *loop, int fd, void *data);

struct c_event_resource {
	int fd;
	void *data;
	c_event_callback callback;
};


struct c_event_loop * c_event_loop_init();
int c_event_loop_run(struct c_event_loop *loop);
int c_event_loop_add(struct c_event_loop *loop, int fd, c_event_callback callback, void *data);
int c_event_loop_del(struct c_event_loop *loop, struct c_event_resource *resource);
void c_event_loop_free(struct c_event_loop *loop);

#endif
