#include <stdlib.h>

#include "wayland/proto/wayland.h"

#include "seat/session/session.h"
#include "seat/input.h"

#include "util/event_loop.h"
#include "util/log.h"


static void seat_enable_cb(struct c_seat *seat, void *userdata) {
  ((struct c_session *)userdata)->active = 1;
}

static void seat_disable_cb(struct c_seat *seat, void *userdata) {
  ((struct c_session *)userdata)->active = 0;
}

C_EVENT_CALLBACK seat_event_cb(struct c_event_loop *loop, int fd, void *userdata) {
  struct c_seat *seat = userdata;
  if (c_seat_dispatch(seat) < 0)
    return C_EVENT_ERROR_FATAL;

  return C_EVENT_OK;
  
}

static int input_open_restricted(const char *path, int flags, void *userdata) {
  struct c_session *backend = userdata;
  struct c_session_device *dev = c_session_device_open(backend, path);
  if (!dev) return -1;
  return dev->fd;
}

static void input_close_restricted(int fd, void *userdata) {
  struct c_session *backend = userdata;
  struct c_session_device *dev;
  c_list_for_each(backend->devices, dev) {
    if (dev->fd == fd) {
      c_session_device_close(backend, dev);
      break;
    }
  }
}

static void *on_wl_seat_bind(struct c_wl_connection *conn, c_wl_object_id new_id, c_wl_uint version, void *userdata) {
  struct c_input *input = userdata;
  wl_seat_name(conn, new_id, "seat0");
  wl_seat_capabilities(conn, new_id, input->capabilities);
  return NULL;
}

void c_session_device_close(struct c_session *backend, struct c_session_device *device) {
  if (c_seat_close_device(backend->seat, device->id) < 0) {
    c_log(C_LOG_WARNING, "failed to close device: %s", device->path);

  } else {
    c_log(C_LOG_DEBUG, "closed device: %s", device->path);
    c_list_remove(&backend->devices, device);
  }
}

struct c_session_device *c_session_device_open(struct c_session *backend, const char *path) {
  int fd;
  int id;
  if ((id = c_seat_open_device(backend->seat, path, &fd)) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to open %s", path);
    return NULL;
  }

  struct c_session_device dev = {0};
  dev.fd = fd;
  dev.id = id;
  snprintf(dev.path, sizeof(dev.path), "%s", path);

  c_log(C_LOG_DEBUG, "opened new device: %s", path);

  return c_list_push(backend->devices, &dev, sizeof(dev));
}

void c_session_free(struct c_session *backend) {
  if (backend->input)   c_input_free(backend->input);
  if (backend->seat)    c_seat_close(backend->seat);
  c_list_destroy(backend->devices);

  free(backend);
}

struct c_session *c_session_init(struct c_event_loop *loop, struct c_wl_display *display) {
  struct c_session *backend = calloc(1, sizeof(*backend));
  if (!backend) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  struct c_seat_listener seat_listener = {
    .seat_enable = seat_enable_cb,
    .seat_disable = seat_disable_cb,
  };

  backend->devices = c_list_new();
  backend->seat = c_seat_open(&seat_listener, backend);
  if (!backend->seat) goto error;

  c_event_loop_add(loop, c_seat_get_fd(backend->seat), seat_event_cb, backend->seat);
  if (c_seat_dispatch(backend->seat) == -1) goto error;

  struct c_input_libinput_interface libinput_interface = {
    .open_restricted = input_open_restricted,
    .close_restricted = input_close_restricted,
    .userdata = backend,
  };

  backend->input = c_input_init(loop, &libinput_interface);
  if (!backend->input) goto error;

  c_wl_display_add_supported_interface(display, "wl_seat", on_wl_seat_bind, backend->input);

  return backend;

error:
  c_session_free(backend);
  return NULL;
}
