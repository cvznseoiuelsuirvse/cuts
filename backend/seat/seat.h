#ifndef CUTS_BACKEND_SEAT_SEAT_H
#define CUTS_BACKEND_SEAT_SEAT_H

#include <stdint.h>

struct c_seat;
struct c_seat_listener {
	void (*seat_enable)(struct c_seat *seat, void *userdata);
	void (*seat_disable)(struct c_seat *seat, void *userdata);
};

struct c_seat_impl {
	int  (*open)(void **backend, struct c_seat_listener *listener, void *listener_data);
	void (*close)(void *backend);
	int  (*dispatch)(void *backend);
	int  (*open_device)(void *backend, const char *path, int *fd);
	int  (*close_device)(void *backend, int fd);
	int  (*get_fd)(void *backend);
};

struct c_seat *c_seat_open(struct c_seat_listener *listener, void *userdata);
void c_seat_close(struct c_seat *seat);
int c_seat_dispatch(struct c_seat *seat);
int c_seat_open_device(struct c_seat *seat, const char *path, int *fd);
int c_seat_close_device(struct c_seat *seat, int fd);
int c_seat_get_fd(struct c_seat *seat);

#endif
