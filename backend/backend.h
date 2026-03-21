#ifndef CUTS_BACKEND_H
#define CUTS_BACKEND_H

#include "util/event_loop.h"
#include "backend/seat/seat.h"
#include "util/list.h"

enum C_BACKEND_DEV_TYPES {
	C_BACKEND_DEV_DRM = 1,
	C_BACKEND_DEV_INPUT,
};

struct c_backend_device {
	int fd;
	int id;
	char path[256];
};

struct c_backend {
  struct c_drm      *drm;
  struct c_input    *input;
  struct c_seat 	*seat;
	
  int active;
  c_list *devices; // struct c_backend_device
};

struct c_backend *c_backend_init(struct c_event_loop *loop);
void c_backend_free(struct c_backend *backend);
struct c_backend_device *c_backend_device_open(struct c_backend *backend, const char *path, enum C_BACKEND_DEV_TYPES type);
void c_backend_device_close(struct c_backend *backend, struct c_backend_device *device);
#endif
