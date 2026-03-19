#ifndef CUTS_BACKEND_SEAT_SEAT_H
#define CUTS_BACKEND_SEAT_SEAT_H

#include <stdint.h>

struct c_seat {
  int fd;
  uint16_t cur_vt;
};

struct c_seat *c_seat_open();
void c_seat_close(struct c_seat *seat);
int c_seat_open_device(struct c_seat *seat, const char *path, int *fd);
int c_seat_close_device(struct c_seat *seat, int fd);

#endif
