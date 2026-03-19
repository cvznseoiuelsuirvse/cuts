#include <stdlib.h>
#include <stdlib.h>

#include "backend/session.h"
#include "backend/seat/seat.h"
#include "util/log.h"

void c_session_free(struct c_session *session) {
  if (session->seat) c_seat_close(session->seat);
  free(session);
}

// struct c_session *c_session_init(struct c_event_loop *loop) {
//   c_seat_open();
//   return NULL;
// }
  
  
struct c_session *c_session_init(struct c_event_loop *loop) {
  struct c_session *session = calloc(1, sizeof(*session));
  if (!session) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  struct c_seat *seat = c_seat_open();
  if (!seat)
    goto error;
  
  session->seat = seat;

  return session;

error:
  c_session_free(session);
  return NULL;

}

