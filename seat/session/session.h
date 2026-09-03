#ifndef CUTS_BACKEND_H
#define CUTS_BACKEND_H

#include "seat/input.h"
#include "seat/session/seat.h"
#include "util/list.h"

struct c_session_device {
	int fd;
	int id;
	char path[256];
};

struct c_session {
  struct c_seat *seat;
  struct c_input *input;
  int active;
  c_list *devices;
};

struct c_session *c_session_init(struct c_event_loop *loop, struct c_input_config *config);
void c_session_free(struct c_session *backend);
struct c_session_device *c_session_device_open(struct c_session *backend,
                                               const char *path);
void c_session_device_close(struct c_session *backend,
                            struct c_session_device *device);
#endif
