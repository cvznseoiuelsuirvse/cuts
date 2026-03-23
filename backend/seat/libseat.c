#include <stdlib.h>
#include <libseat.h>

#include "backend/seat/seat.h"
#include "util/log.h"

struct libseat_backend {
  struct libseat *libseat;
  struct c_seat_listener *listener;
  void *listener_data;
};

static void log_libseat(enum libseat_log_level level, const char *format, va_list args) {
  vprintf(format, args);
  printf("\n");
  // c_log(C_LOG_DEBUG, format, args);
}

static void handle_enable(struct libseat *libseat, void *userdata) {
  struct libseat_backend *ls_backend = userdata;
  if (ls_backend->listener->seat_enable)
    ls_backend->listener->seat_enable(NULL, ls_backend->listener_data);
}

static void handle_disable(struct libseat *libseat, void *userdata) {
  struct libseat_backend *ls_backend = userdata;
  if (ls_backend->listener->seat_disable)
    ls_backend->listener->seat_disable(NULL, ls_backend->listener_data);
}

static int _libseat_open(void **backend, struct c_seat_listener *listener, void *listener_data) {
  struct libseat_backend *ls_backend = calloc(1, sizeof(*ls_backend));
  if (!ls_backend) return -1;

  struct libseat_seat_listener *ls_listener = malloc(sizeof(*ls_listener));
  ls_listener->enable_seat = handle_enable;
  ls_listener->disable_seat = handle_disable;
  // struct libseat_seat_listener ls_listener = {
  //   .enable_seat = handle_enable,
  //   .disable_seat = handle_disable,
  // };

  libseat_set_log_handler(log_libseat);
	libseat_set_log_level(LIBSEAT_LOG_LEVEL_INFO);

  struct libseat *seat = libseat_open_seat(ls_listener, ls_backend);
  if (!seat) {
    c_log_errno(C_LOG_ERROR, "libseat_open_seat failed");
    return -1;
  }

  ls_backend->libseat = seat;
  ls_backend->listener = listener;
  ls_backend->listener_data = listener_data;

  *backend = ls_backend;

  return 0;
}

static void _libseat_close(void *backend) {
  struct libseat_backend *b = backend;
  libseat_close_seat(b->libseat);
  free(backend);
}

static int _libseat_device_open(void *backend, const char *path, int *fd) {
  struct libseat_backend *b = backend;
  int device_id = libseat_open_device(b->libseat, path, fd);
  if (device_id < 0) {
    c_log_errno(C_LOG_ERROR, "libseat_open_device failed");
    return -1;
  }
  return device_id;
}

static int _libseat_device_close(void *backend, int id) {
  struct libseat_backend *b = backend;
  if (libseat_close_device(b->libseat, id) < 0) {
    c_log_errno(C_LOG_ERROR, "libseat_open_device failed");
    return -1;
  }
  return 0;
}

static int _libseat_dispatch(void *backend) {
  struct libseat_backend *b = backend;
  c_log(C_LOG_DEBUG, "backend %p", b);
  if (libseat_dispatch(b->libseat, 0) < 0) {
    c_log_errno(C_LOG_ERROR, "libseat_dispatch failed");
    return -1;
  }
  return 0;
}

static int _libseat_get_fd(void *backend) {
  struct libseat_backend *b = backend;
  return libseat_get_fd(b->libseat);
}

const struct c_seat_impl libseat_impl = {
  .open = _libseat_open, 
  .close = _libseat_close, 
  .open_device = _libseat_device_open, 
  .close_device = _libseat_device_close, 
  .dispatch = _libseat_dispatch,
  .get_fd = _libseat_get_fd,
};
