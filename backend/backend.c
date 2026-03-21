#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "backend/backend.h"
#include "backend/input.h"
#include "backend/drm/drm.h"
#include "util/event_loop.h"
#include "util/log.h"


static struct c_backend_device *open_gpu(struct c_backend *backend) {
  char gpu_path[128];
  int gpu_found = 0;
  for (int i = 0; i < 10; i++) {
    memset(gpu_path, 0, sizeof(gpu_path));
    snprintf(gpu_path, sizeof(gpu_path), "/dev/dri/card%d", i);
    int fd = open(gpu_path, O_RDWR | O_NONBLOCK);
    if (fd > 0) {
      gpu_found = 1;
      close(fd);
      break;
    }
  }

  if (!gpu_found) {
    c_log(C_LOG_ERROR, "no GPUs found");
    return NULL;
  }

  struct c_backend_device *dev = c_backend_device_open(backend, gpu_path, C_BACKEND_DEV_DRM);
  if (!dev) return NULL;

  return dev;
}

static void seat_enable_cb(struct c_seat *seat, void *userdata) {
  ((struct c_backend *)userdata)->active = 1;
  c_log(C_LOG_DEBUG, "seat enabled");
}

static void seat_disable_cb(struct c_seat *seat, void *userdata) {
  ((struct c_backend *)userdata)->active = 0;
  c_log(C_LOG_DEBUG, "seat disabled");
}

C_EVENT_CALLBACK seat_event_cb(struct c_event_loop *loop, int fd, void *userdata) {
  struct c_seat *seat = userdata;
  if (c_seat_dispatch(seat) < 0)
    return C_EVENT_ERROR_FATAL;

  return C_EVENT_OK;
  
}

void c_backend_device_close(struct c_backend *backend, struct c_backend_device *device) {
  if (c_seat_close_device(backend->seat, device->id) < 0) {
    c_log(C_LOG_WARNING, "failed to close device: %s", device->path);

  } else {
    c_log(C_LOG_DEBUG, "closed device: %s", device->path);
    c_list_remove_ptr(&backend->devices, device);
    free(device);
  }
}

struct c_backend_device *c_backend_device_open(struct c_backend *backend, const char *path, enum C_BACKEND_DEV_TYPES type) {
  int fd;
  int id;
  if ((id = c_seat_open_device(backend->seat, path, &fd)) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to open %s", path);
    return NULL;
  }

  struct c_backend_device *dev = calloc(1, sizeof(*dev));
  if (!dev) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  dev->fd = fd;
  dev->id = id;
  snprintf(dev->path, sizeof(dev->path), "%s", path);

  const char *dev_type_str;
  switch (type) {
    case C_BACKEND_DEV_DRM:
      dev_type_str = "DRM";
      break;

    case C_BACKEND_DEV_INPUT:
      dev_type_str = "INPUT";
      break;

    default:
      dev_type_str = "UNKNOWN";
      break;
  }

  c_log(C_LOG_INFO, "opened new %s device: %s", dev_type_str, path);

  c_list_push(backend->devices, dev, 0);
  return dev;
}

void c_backend_free(struct c_backend *backend) {
  struct c_backend_device *dev;
  c_list_for_each(backend->devices, dev)
    c_backend_device_close(backend, dev);
  
  c_list_destroy(backend->devices);

  if (backend->input)   c_input_free(backend->input);
  if (backend->drm)     c_drm_free(backend->drm);
      
  if (backend->seat)    c_seat_close(backend->seat);

  free(backend);
}

struct c_backend *c_backend_init(struct c_event_loop *loop) {
  struct c_backend *backend = calloc(1, sizeof(*backend));
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

  if (c_seat_dispatch(backend->seat) == -1) goto error;

  c_event_loop_add(loop, backend->seat->fd, seat_event_cb, backend->seat);

  struct c_backend_device *dev_gpu = open_gpu(backend);
  if (!dev_gpu) goto error;

  backend->input = c_input_init(loop, backend);
  if (!backend->input) goto error;


  backend->drm = c_drm_init(dev_gpu->fd, backend->input);
  if (!backend->drm) goto error;

  return backend;

error:
  c_backend_free(backend);
  return NULL;
}
