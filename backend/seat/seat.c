#include <stdlib.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/un.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "backend/seat/sock.h"
#include "backend/seat/seat.h"

#include "util/log.h"

static int server_connect() {
  struct sockaddr_un sock_addr;
  int fd = socket(AF_UNIX, C_SEAT_SOCKET_FLAGS, 0);
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sun_family = AF_UNIX;
  snprintf(sock_addr.sun_path, sizeof(sock_addr.sun_path), "%s", C_SEAT_SOCKET_PATH);

  if (connect(fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to connect to the socket");
    return -1;
  }
  return fd;
}

int c_seat_dispatch(struct c_seat *seat) {
  struct c_seat_msg_params params;
  seat_recv(seat->fd, &params);

  switch (params.header.op) {
    case C_SEAT_MSG_ENABLE_SEAT: 
      if (seat->listener->seat_enable)
        seat->listener->seat_enable(seat, seat->listener_data);
      return 0;

    case C_SEAT_MSG_DISABLE_SEAT: 
      if (seat->listener->seat_disable)
        seat->listener->seat_disable(seat, seat->listener_data);
      return 0;

    case C_SEAT_MSG_ERROR: 
      c_log(C_LOG_ERROR, "[cuts-seat] %s", params.body);
      return -1;

    default: 
      c_log(C_LOG_WARNING, "received unknown op: %d", params.header.op);
      return -1;
  }

}

struct c_seat *c_seat_open(struct c_seat_listener *listener, void *usedata) {
  int fd = server_connect();
  if (fd < 0) return NULL;
  
  struct c_seat *seat = calloc(1, sizeof(*seat));
  if (!seat) {
    c_log(C_LOG_ERROR, "calloc failed");
    goto error;
  }


  struct stat st;
  if (fstat(0, &st) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to get current vt");
    goto error_seat;
  }

  seat->fd = fd;
  seat->listener = listener;
  seat->listener_data = usedata;

  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_OPEN_SEAT;

  if (seat_send(fd, &params) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to send C_SEAT_MSG_OPEN_SEAT message");
    goto error_seat;
  }

  return seat;

error_seat:
  free(seat);
error:
  close(fd);
  return NULL; 
}

void c_seat_close(struct c_seat *seat) {
  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_CLOSE_SEAT;
  seat_send(seat->fd, &params);
  free(seat);
}

int c_seat_open_device(struct c_seat *seat, const char *path, int *fd) {
  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_OPEN_DEVICE;
  params.header.body_size = strlen(path) + 1;
  snprintf(params.body, sizeof(params.body), "%s", path);

  if (seat_send(seat->fd, &params) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to send C_SEAT_MSG_OPEN_DEVICE message");
    return -1;
  }

  struct c_seat_msg_params resp_params = {0};
  int n = seat_recv(seat->fd, &resp_params);
  if (n < 0) {
    c_log_errno(C_LOG_ERROR, "failed to receive ack message");
    return -1;
  }

  if (resp_params.header.op == C_SEAT_MSG_ERROR) {
    c_log(C_LOG_ERROR, "[cuts-seat] failed to open %s: %s", path, resp_params.body);
    return -1;
  }

  *fd = resp_params.fd;

  return *(int *)resp_params.body;
}

int c_seat_close_device(struct c_seat *seat, int id) {
  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_CLOSE_DEVICE;
  params.header.body_size = sizeof(id);
  *(int *)params.body = id;

  if (seat_send(seat->fd, &params) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to send C_SEAT_MSG_CLOSE_DEVICE message");
    return -1;
  }

  struct c_seat_msg_params resp_params = {0};
  if (seat_recv(seat->fd, &resp_params) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to receive ack message");
    return -1;
  }

  if (resp_params.header.op == C_SEAT_MSG_ERROR) {
    c_log(C_LOG_ERROR, "[cuts-seat] failed to close device %d: %s", id, resp_params.body);
    return -1;
  }


  return 0;
}
