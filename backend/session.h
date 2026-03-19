#ifndef CUTS_BACKEND_SESSION_H
#define CUTS_BACKEND_SESSION_H

#include "util/event_loop.h"
#include "backend/seat/seat.h"

struct c_session {
	struct c_seat  *seat;
	int active;
};

struct c_session *c_session_init(struct c_event_loop *loop);
void c_session_free(struct c_session *session);

#endif
