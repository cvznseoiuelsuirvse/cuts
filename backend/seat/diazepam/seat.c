#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "sock.h"
#include "backend/seat/seat.h"

#include "util/log.h"

struct cutseat_backend {
  int fd;
  struct c_seat_listener *listener;
  void *listener_data;
};

static int server_connect() {
  struct sockaddr_un sock_addr;
  int fd = socket(AF_UNIX, C_SEAT_SOCKET_FLAGS, 0);
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sun_family = AF_UNIX;
  snprintf(sock_addr.sun_path, sizeof(sock_addr.sun_path), "%s", C_SEAT_SOCKET_PATH);

  if (connect(fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    c_log_errno(C_LOG_ERROR, "[cutseat] failed to connect to the socket");
    return -1;
  }
  return fd;
}

static int seat_open(void **backend, struct c_seat_listener *listener, void *listener_data) {
  int fd = server_connect();
  if (fd < 0) return -1;

  struct cutseat_backend *_backend = calloc(1, sizeof(*_backend));
  if (!_backend) {
    c_log(C_LOG_ERROR, "calloc failed");
    return -1;
  }

  _backend->fd = fd;
  _backend->listener = listener;
  _backend->listener_data = listener_data;

  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_OPEN_SEAT;

  if (seat_send(fd, &params) < 0) {
    c_log_errno(C_LOG_ERROR, "[cutseat] failed to send C_SEAT_MSG_OPEN_SEAT message");
    goto error_seat;
  }

  *backend = _backend;
  return 0;

error_seat:
  free(_backend);
  close(fd);
  return -1; 
}

static void seat_close(void *backend) {
  struct cutseat_backend *b = backend;

  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_CLOSE_SEAT;
  seat_send(b->fd, &params);
  free(backend);
}


static int seat_dispatch(void *backend) {
  struct cutseat_backend *b = backend;

  struct c_seat_msg_params params;
  seat_recv(b->fd, &params);

  switch (params.header.op) {
    case C_SEAT_MSG_ENABLE_SEAT: 
      if (b->listener->seat_enable)
        b->listener->seat_enable(NULL, b->listener_data);
      c_log(C_LOG_DEBUG, "[cutseat] seat enabled");
      return 0;

    case C_SEAT_MSG_DISABLE_SEAT: 
      if (b->listener->seat_disable)
        b->listener->seat_disable(NULL, b->listener_data);
      c_log(C_LOG_DEBUG, "[cutseat] seat disabled");
      return 0;

    case C_SEAT_MSG_ERROR: 
      c_log(C_LOG_ERROR, "[cutseat] %s", params.body);
      return -1;

    default: 
      c_log(C_LOG_WARNING, "[cutseat] received unknown op: %d", params.header.op);
      return -1;
  }

}

static int seat_open_device(void *backend, const char *path, int *fd) {
  struct cutseat_backend *b = backend;

  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_OPEN_DEVICE;
  params.header.body_size = strlen(path) + 1;
  snprintf(params.body, sizeof(params.body), "%s", path);

  if (seat_send(b->fd, &params) < 0) {
    c_log_errno(C_LOG_ERROR, "[cutseat] failed to send C_SEAT_MSG_OPEN_DEVICE message");
    return -1;
  }

  struct c_seat_msg_params resp_params = {0};
  int n = seat_recv(b->fd, &resp_params);
  if (n < 0) {
    c_log_errno(C_LOG_ERROR, "[cutseat] failed to receive ack message");
    return -1;
  }

  if (resp_params.header.op == C_SEAT_MSG_ERROR) {
    c_log(C_LOG_ERROR, "[cutseat] failed to open %s: %s", path, resp_params.body);
    return -1;
  }

  *fd = resp_params.fd;

  return *(int *)resp_params.body;
}

static int seat_close_device(void *backend, int id) {
  struct cutseat_backend *b = backend;

  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_CLOSE_DEVICE;
  params.header.body_size = sizeof(id);
  *(int *)params.body = id;

  if (seat_send(b->fd, &params) < 0) {
    c_log_errno(C_LOG_ERROR, "[cutseat] failed to send C_SEAT_MSG_CLOSE_DEVICE message");
    return -1;
  }

  struct c_seat_msg_params resp_params = {0};
  if (seat_recv(b->fd, &resp_params) < 0) {
    c_log_errno(C_LOG_ERROR, "[cutseat] failed to receive ack message");
    return -1;
  }

  if (resp_params.header.op == C_SEAT_MSG_ERROR) {
    c_log(C_LOG_ERROR, "[cutseat] failed to close device %d: %s", id, resp_params.body);
    return -1;
  }

  return 0;
}


static int seat_get_fd(void *backend) {
  struct cutseat_backend *b = backend;
  return b->fd;
}

const struct c_seat_impl cutseat_impl = {
  .open = seat_open,
  .close = seat_close,
  .open_device = seat_open_device,
  .close_device = seat_close_device,
  .dispatch = seat_dispatch,
  .get_fd = seat_get_fd,
};
