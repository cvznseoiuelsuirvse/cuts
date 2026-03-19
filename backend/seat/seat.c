#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "backend/seat/sock.h"
#include "backend/seat/seat.h"
#include "backend/seat/util.h"

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

struct c_seat *c_seat_open() {
  int fd = server_connect();
  if (fd < 0) return NULL;
  
  struct c_seat *seat = calloc(1, sizeof(*seat));
  if (!seat) {
    c_log(C_LOG_ERROR, "calloc failed");
    goto error;
  }


  int vt = get_active_vt();
  if (vt < 0) {
    c_log(C_LOG_ERROR, "failed to get active vt");
    goto error_seat;
  }
  seat->fd = fd;
  seat->cur_vt = vt;



  struct c_seat_msg_params params = {0};
  params.header.type = C_SEAT_MSG_OPEN_SEAT;
  params.header.body_size = sizeof(uint16_t);
  *(uint16_t *)params.body = vt;

  if (seat_send(fd, &params) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to send C_SEAT_MSG_OPEN_SEAT message");
    goto error_seat;
  }

  struct c_seat_msg_params resp_params = {0};
  if (seat_recv(fd, &resp_params) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to receive ack message");
    goto error_seat;
  }

  if (resp_params.header.type == C_SEAT_MSG_ERROR) {
    c_log(C_LOG_ERROR, "failed to open a seat: %s", resp_params.body);
    goto error_seat;
  }

  assert(resp_params.header.type == C_SEAT_MSG_ACK);

  return seat;

error_seat:
  free(seat);
error:
  close(fd);
  return NULL; 
}

void c_seat_close(struct c_seat *seat) {
  struct c_seat_msg_params params = {0};
  params.header.type = C_SEAT_MSG_CLOSE_SEAT;
  seat_send(seat->fd, &params);
  free(seat);
}

int c_seat_open_device(struct c_seat *seat, const char *path, int *fd) {
  struct c_seat_msg_params params = {0};
  params.header.type = C_SEAT_MSG_OPEN_DEVICE;
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

  if (resp_params.header.type == C_SEAT_MSG_ERROR) {
    c_log(C_LOG_ERROR, "failed to open %s: %s", path, resp_params.body);
    return -1;
  }

  *fd = resp_params.fd;

  return *(int *)resp_params.body;
}

int c_seat_close_device(struct c_seat *seat, int id) {
  struct c_seat_msg_params params = {0};
  params.header.type = C_SEAT_MSG_CLOSE_DEVICE;
  params.header.body_size = sizeof(id);
  *(int *)params.body = id;

  if (seat_send(seat->fd, &params) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to send C_SEAT_MSG_OPEN_DEVICE message");
    return -1;
  }

  struct c_seat_msg_params resp_params = {0};
  if (seat_recv(seat->fd, &resp_params) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to receive ack message");
    return -1;
  }

  if (resp_params.header.type == C_SEAT_MSG_ERROR) {
    c_log(C_LOG_ERROR, "failed to close device %d: %s", id, resp_params.body);
    return -1;
  }

  assert(resp_params.header.type == C_SEAT_MSG_ACK);

  return 0;
}
