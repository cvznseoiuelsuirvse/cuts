#ifndef CUTS_BACKEND_SEAT_SOCK_H
#define CUTS_BACKEND_SEAT_SOCK_H

#define C_SEAT_SOCKET_FLAGS SOCK_STREAM | SOCK_CLOEXEC
#define C_SEAT_SOCKET_PATH "/tmp/cuts-seat.sock"

#include <stdint.h>
	
#define C_SEAT_MSG_ACK 			1
#define C_SEAT_MSG_OPEN_SEAT 	2
#define C_SEAT_MSG_CLOSE_SEAT 	3
#define C_SEAT_MSG_OPEN_DEVICE 	4
#define C_SEAT_MSG_CLOSE_DEVICE 5

#define C_SEAT_MSG_ERROR 		0xFF

#define C_SEAT_HEADER_SIZE		sizeof(struct c_seat_msg_header)

struct c_seat_msg_header {
	uint16_t type;
	uint16_t body_size;
};

struct c_seat_msg_params {
	struct c_seat_msg_header header;
	char body[1024];
	int fd;
};

int seat_send(int fd, struct c_seat_msg_params *params);
int seat_send_error(int fd, const char *fmt, ...);
int seat_recv(int fd, struct c_seat_msg_params *params);

#endif
