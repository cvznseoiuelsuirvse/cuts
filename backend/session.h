#ifndef CUTS_BACKEND_SESSION_H
#define CUTS_BACKEND_SESSION_H

#include <libseat.h>
#include "util/event_loop.h"

struct c_session {
	struct libseat *seat;
	char 			seat_name[128];
	int 			seat_fd;

	int active;
};

struct c_session *c_session_init(struct c_event_loop *loop);
void c_session_free(struct c_session *session);

#endif
