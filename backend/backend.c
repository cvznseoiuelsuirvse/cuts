#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "backend/backend.h"
#include "backend/session.h"
#include "backend/input.h"
#include "backend/drm.h"
#include "util/event_loop.h"
#include "util/log.h"


void c_backend_device_close(struct c_backend *backend, struct c_backend_device *device) {
  libseat_close_device(backend->session->seat, device->id);
  c_log(C_LOG_DEBUG, "closed device: %s", device->path);
  c_list_remove_ptr(&backend->devices, device);
  free(device);
}

struct c_backend_device *c_backend_device_open(struct c_backend *backend, const char *path, enum C_BACKEND_DEV_TYPES type) {
  int fd;
  int id;
  if ((id = libseat_open_device(backend->session->seat, path, &fd)) < 0) {
    c_log(C_LOG_ERROR, "failed to open %s: %s", path, strerror(errno));
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

void c_backend_free(struct c_backend *backend) {
  struct c_backend_device *dev;
  c_list_for_each(backend->devices, dev) {
    c_backend_device_close(backend, dev);
  }
  c_list_destroy(backend->devices);

  if (backend->session) c_session_free(backend->session);
  if (backend->input)   c_input_free(backend->input);
  if (backend->drm)     c_drm_free(backend->drm);

  free(backend);
}

struct c_backend *c_backend_init(struct c_event_loop *loop) {
  struct c_backend *backend = calloc(1, sizeof(*backend));
  if (!backend) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  backend->devices = c_list_new();
  backend->session = c_session_init(loop);
  if (!backend->session) goto error;

  struct c_backend_device *dev_gpu = open_gpu(backend);
  if (!dev_gpu) goto error;

  backend->drm = c_drm_init(dev_gpu->fd);
  if (!backend->drm) goto error;


  backend->input = c_input_init(loop, backend);
  if (!backend->input) goto error;

  return backend;

error:
  c_backend_free(backend);
  return NULL;
}
