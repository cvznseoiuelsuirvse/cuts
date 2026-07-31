#ifndef CUTS_BACKEND_SEAT_SOCK_H
#define CUTS_BACKEND_SEAT_SOCK_H

#define DIAZEPAM_SOCKET_FLAGS SOCK_STREAM | SOCK_CLOEXEC
#define DIAZEPAM_SOCKET_PATH "/tmp/cuts-seat.sock"

#include <stdint.h>
	
#define DIAZEPAM_MSG_ACK 				1
#define DIAZEPAM_MSG_OPEN_SEAT 		2
#define DIAZEPAM_MSG_CLOSE_SEAT 		3
#define DIAZEPAM_MSG_OPEN_DEVICE 		4
#define DIAZEPAM_MSG_CLOSE_DEVICE 	5
#define DIAZEPAM_MSG_ENABLE_SEAT 		6
#define DIAZEPAM_MSG_DISABLE_SEAT 	7
#define DIAZEPAM_MSG_ERROR 			0xFF

#define DIAZEPAM_HEADER_SIZE		sizeof(struct diazepam_msg_header)

struct diazepam_msg_header {
	uint16_t op;
	uint16_t body_size;
};

struct diazepam_msg_params {
	struct diazepam_msg_header header;
	char body[1024];
	int fd;
};

int seat_send(int fd, struct diazepam_msg_params *params);
int seat_send_error(int fd, const char *fmt, ...);
int seat_recv(int fd, struct diazepam_msg_params *params);

#endif
