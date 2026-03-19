#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "backend/seat/sock.h"


int seat_send(int fd, struct c_seat_msg_params *params) {
  struct c_seat_msg_header header = params->header;
  size_t buffer_size = C_SEAT_HEADER_SIZE + header.body_size;
  uint8_t buffer[buffer_size];

  memcpy(buffer, &header, C_SEAT_HEADER_SIZE);
  memcpy(buffer + C_SEAT_HEADER_SIZE, params->body, header.body_size);

  if (params->fd > 0) {
    struct msghdr m;
    char cmsg[CMSG_SPACE(sizeof(int))];

    memset(cmsg, 0, CMSG_SPACE(sizeof(int)));
    memset(&m, 0, sizeof(struct msghdr));

    struct iovec e = {buffer, buffer_size};
    m.msg_iov = &e;
    m.msg_iovlen = 1;
    m.msg_control = cmsg;
    m.msg_controllen = CMSG_SPACE(sizeof(int));

    struct cmsghdr *c = CMSG_FIRSTHDR(&m);
    c->cmsg_level = SOL_SOCKET;
    c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(sizeof(params->fd));
    *(int *)CMSG_DATA(c) = params->fd;

    return sendmsg(fd, &m, MSG_NOSIGNAL);
  }

  return send(fd, buffer, buffer_size, MSG_NOSIGNAL);
}

int seat_recv(int fd, struct c_seat_msg_params *params) {
  char cmsg[CMSG_SPACE(sizeof(int))];

  char buffer[sizeof(params->body)];

  struct iovec e[1];
  e[0].iov_base = buffer;
  e[0].iov_len = sizeof(buffer);

  struct msghdr m = {0};
  m.msg_iov = e; 
  m.msg_iovlen = 1; 
  m.msg_control = cmsg; 
  m.msg_controllen = sizeof(cmsg); 

  ssize_t n = recvmsg(fd, &m, 0);
  if (n <= 0) return n;

  struct cmsghdr *c = CMSG_FIRSTHDR(&m);
  if (c != NULL){
    params->fd = *(int *)CMSG_DATA(c);
  }

  memcpy(&params->header, buffer, C_SEAT_HEADER_SIZE);
  assert(params->header.body_size <= sizeof(params->body));

  memcpy(params->body, buffer + C_SEAT_HEADER_SIZE, params->header.body_size);
  return n;
  
}

int seat_send_error(int fd, const char *fmt, ...) {
  int ret = 0;
  va_list args;
  va_start(args, fmt);

  struct c_seat_msg_params params = {0};
  params.header.type = C_SEAT_MSG_ERROR;
  vsnprintf(params.body, sizeof(params.body), fmt, args);
  params.header.body_size = strlen(params.body);

  ret = seat_send(fd, &params);

  return ret;
}
