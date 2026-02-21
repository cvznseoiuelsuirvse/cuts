
#ifndef CUTS_SERVER_COMPOSITOR_H
#define CUTS_SERVER_COMPOSITOR_H

#define CUTS_MAX_EPOLL_EVENTS 512

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#include "../util/list.h"

struct c_compositor {
	struct c_event_loop	 *loop;
	struct c_drm_backend *backend;

	int		needs_redraw;
	c_list 	*surfaces;
};

struct c_compositor *c_compositor_init();
void c_compositor_free(struct c_compositor *compositor);
int  c_compositor_run_loop(struct c_compositor *compositor);
void c_compositor_update(struct c_compositor *compositor);

#endif
