#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "backend/session.h"
#include "util/log.h"

static void enable_seat(struct libseat *seat, void *userdata) {
  struct c_session *session = userdata;
  session->active = 1;
  session->seat = seat;
}

static void disable_seat(struct libseat *seat, void *userdata) {
  struct c_session *session = userdata;
  session->active = 0;
  session->seat = NULL;
  libseat_disable_seat(seat);
}

C_EVENT_CALLBACK libseat_dispatch_handler(struct c_event_loop *loop, int fd, void *data) {
  struct c_session *session = data;
  if (libseat_dispatch(session->seat, 0) == -1) {
    c_log(C_LOG_ERROR, "libseat_dispatch failed: %s", strerror(errno));
    c_session_free(session);
    return C_EVENT_ERROR_FATAL;
  }

  return C_EVENT_OK;
}

void c_session_free(struct c_session *session) {
  if (session->seat) libseat_close_seat(session->seat);
  free(session);
}

struct c_session *c_session_init(struct c_event_loop *loop) {
  struct c_session *session = calloc(1, sizeof(*session));
  if (!session) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  struct libseat_seat_listener listener = {
    .enable_seat = enable_seat,
    .disable_seat = disable_seat,
  };

  struct libseat *seat = libseat_open_seat(&listener, session);
  if (!seat) {
    c_log(C_LOG_ERROR, "libseat_open_seat failed: %s", strerror(errno));
    goto error;
  }
  session->seat = seat;

  const char *seat_name = libseat_seat_name(seat);
  c_log(C_LOG_INFO, "opened seat %s", seat_name);
  snprintf(session->seat_name, sizeof(session->seat_name), "%s", seat_name);

  int seat_fd = libseat_get_fd(seat);
  c_event_loop_add(loop, seat_fd, libseat_dispatch_handler, session);
  session->seat_fd = seat_fd;

  if (libseat_dispatch(seat, 0) == -1) {
    c_log(C_LOG_ERROR, "libseat_dispatch failed: %s", strerror(errno));
    goto error;
  }

  return session;

error:
  c_session_free(session);
  return NULL;

}

