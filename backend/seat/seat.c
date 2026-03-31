#include <stdlib.h>

#include "backend/seat/seat.h"

#include "util/log.h"
#include "util/helpers.h"

struct c_seat {
  const struct c_seat_impl *impl;
  void *backend;
};

extern const struct c_seat_impl cutseat_impl;
extern const struct c_seat_impl libseat_impl;


struct c_seat *c_seat_open(struct c_seat_listener *listener, void *listener_data) {
  struct c_seat *seat = calloc(1, sizeof(*seat));
  if (!seat) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

	char *backend_type = getenv("CUTS_SEAT_BACKEND");
  if (backend_type) {
    if (STREQ(backend_type, "libseat")) {
      seat->impl = &libseat_impl;
    } else if (STREQ("cuts", backend_type)) {
      seat->impl = &cutseat_impl;
    } else {
      c_log(C_LOG_ERROR, "invalid CUTS_SEAT_BACKEND value: %s. expected 'cuts' or 'libinput'", backend_type);
      goto error;
    }
    c_log(C_LOG_INFO, "using '%s' seat backend", backend_type);
  } else {
    seat->impl = &cutseat_impl;
    c_log(C_LOG_INFO, "using 'cuts' seat backend", backend_type);
  }

  if (seat->impl->open(&seat->backend, listener, listener_data) < 0) goto error;
  return seat;

error:
  free(seat);
  return NULL;
}

void c_seat_close(struct c_seat *seat) {
  seat->impl->close(seat->backend);
  free(seat);
}

int c_seat_dispatch(struct c_seat *seat) {
  return seat->impl->dispatch(seat->backend);
}

int c_seat_open_device(struct c_seat *seat, const char *path, int *fd) {
  return seat->impl->open_device(seat->backend, path, fd);
}

int c_seat_close_device(struct c_seat *seat, int id) {
  return seat->impl->close_device(seat->backend, id);
}

int c_seat_get_fd(struct c_seat *seat) {
  return seat->impl->get_fd(seat->backend);
}
