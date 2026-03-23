#ifndef CUTS_BACKEND_SESSION_H
#define CUTS_BACKEND_SESSION_H

#include "util/event_loop.h"

struct c_session_seat_impl;
struct c_session {
	struct c_session_seat_impl *impl;
	void *backend;
	int active;
};

struct c_session *c_session_init(struct c_event_loop *loop);
void c_session_free(struct c_session *session);

#endif
