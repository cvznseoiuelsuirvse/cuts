#ifndef CUTS_BACKEND_SEAT_SEAT_H
#define CUTS_BACKEND_SEAT_SEAT_H

#include <stdint.h>

struct c_seat;
struct c_seat_listener {
	void (*seat_enable)(struct c_seat *seat, void *userdata);
	void (*seat_disable)(struct c_seat *seat, void *userdata);
};

struct c_seat {
  int fd;
  // uint16_t cur_vt;

  void 					 *listener_data;
  struct c_seat_listener *listener;
};

struct c_seat *c_seat_open(struct c_seat_listener *listener, void *userdata);
void c_seat_close(struct c_seat *seat);
int c_seat_dispatch(struct c_seat *seat);
int c_seat_open_device(struct c_seat *seat, const char *path, int *fd);
int c_seat_close_device(struct c_seat *seat, int fd);

#endif
