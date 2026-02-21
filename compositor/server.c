#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "server.h"
#include "error.h"
#include "proto/wayland.h"
#include "../util/helpers.h"
                                                                      
static const struct wl_interface *__interface[WL_MAX_INTERFACES];
static size_t __ninterfaces = 0;


inline void wl_interface_add(const struct wl_interface *interface) {
  assert(__ninterfaces < WL_MAX_INTERFACES);
  __interface[__ninterfaces++] = interface; 
}

inline const struct wl_interface *wl_interface_get(const char *interface_name) {
  for (size_t i = 0; i < __ninterfaces; i++) {
    const struct wl_interface *interface = __interface[i];
    if (STREQ(interface_name, interface->name)) return interface;
  }
  return NULL;
}

static int wl_connection_read(struct wl_connection *conn, char *buffer, size_t buffer_size) {
  char cmsg[CMSG_SPACE(sizeof(int))];

  struct iovec e[1];
  e[0].iov_base = buffer;
  e[0].iov_len = buffer_size;

  struct msghdr m = {0};
  m.msg_iov = e; 
  m.msg_iovlen = 1; 
  m.msg_control = cmsg; 
  m.msg_controllen = sizeof(cmsg); 

  ssize_t n = recvmsg(conn->client_fd, &m, 0);
  struct cmsghdr *c = CMSG_FIRSTHDR(&m);
  if (c != NULL){
    conn->msg_fd = *(int *)CMSG_DATA(c);
  }

  return n;
}

static int wl_connection_write(struct wl_connection *conn, char *buffer, size_t buffer_size) {
  if (conn->msg_fd > 0) {
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
    c->cmsg_len = CMSG_LEN(sizeof(conn->msg_fd));
    *(int *)CMSG_DATA(c) = conn->msg_fd;

    return sendmsg(conn->client_fd, &m, 0);
  }

  return send(conn->client_fd, buffer, buffer_size, 0);

}

int wl_connection_send(struct wl_connection *conn, struct wl_message *msg, size_t nargs, ...) {
  va_list args;
  va_start(args, nargs);

  char buffer[WL_CONN_BUFFER_SIZE] = {0};
  uint32_t offset = 0;
  write_u32(buffer, &offset, msg->id);
  write_u16(buffer, &offset, msg->op);
  offset += sizeof(uint16_t);

  for (size_t i = 0; i < nargs; i++) {
    char c = msg->signature[i];

    switch (c) {
    case 'i':
    case 'e':
    case 'f':
      write_i32(buffer, &offset, va_arg(args, wl_int));
      break;

    case 'u':
    case 'o':
    case 'n':
      write_u32(buffer, &offset, va_arg(args, wl_uint));
      break;

    case 's':
      write_string(buffer, &offset, va_arg(args, wl_string));
      break;

    case 'a':
      write_array(buffer, &offset, va_arg(args, wl_array), va_arg(args, wl_int));
      break;

    }
  }

  conn->msg_fd = msg->fd;
  *(uint16_t *)(buffer + 6) = offset;
  // printf("SEND BUFFER: ");
  // print_buffer(buffer, offset);
  wl_connection_write(conn, buffer, offset);
  return 0;
}

static int dispatch(struct wl_connection *conn, 
                    wl_object_id object_id, uint16_t op, uint16_t message_size, 
                    char *buffer) {

  struct wl_object *object = wl_object_get(conn, object_id);
  if (!object) return c_error_set(object_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "object not registered");

  const struct wl_interface *iface = object->iface;
  if (op > iface->nrequests) 
    return c_error_set(object_id, WL_DISPLAY_ERROR_INVALID_METHOD, "%s: method %d doesn't exist (%d>%d)", 
                       iface->name, op, op, iface->nrequests);

  
  struct wl_request request = iface->requests[op];
  printf("%lu: %s.%s( ", object_id, iface->name, request.name);

  union wl_arg args[request.nargs + 1];
  args[0].o = object_id;

  uint32_t offset = WL_HEADER_SIZE;
  for (size_t i = 1; i <= request.nargs; i++) {
    uint8_t c = request.signature[i-1];
    assert(c != 'a');

    switch (c) {
      case 'u': 
        args[i].u = read_u32(buffer, &offset);
        printf("%lu ", args[i].u);
        break;

      case 'i': 
        args[i].i = read_i32(buffer, &offset);
        printf("%li ", args[i].i);
        break;

      case 'f': 
        args[i].f = read_f32(buffer, &offset);
        printf("%f ", args[i].f);
        break;

      case 'o': 
        args[i].o = read_u32(buffer, &offset);
        printf("%lu ", args[i].o);
        break;

      case 'n': 
        args[i].n = read_u32(buffer, &offset);
        printf("%lu ", args[i].n);
        break;

      case 's': 
        read_string(buffer, &offset, args[i].s, WL_MAX_STRING_SIZE);
        printf("%s ", args[i].s);
        break;

      case 'e': 
        args[i].e = read_i32(buffer, &offset);
        printf("%d ", args[i].e);
        break;

      case 'F':
        args[i].F = conn->msg_fd;
        printf("%d ", args[i].F);
        break;
    }
  }

  printf(")\n");

  int ret = request.handler(conn, args);
  conn->last_object = object_id;

  return ret;

}

int wl_connection_dispatch(struct wl_connection *conn) {
  int ret;
  char buffer[WL_CONN_BUFFER_SIZE];
  int received = wl_connection_read(conn, buffer, WL_CONN_BUFFER_SIZE);
  if (received == 0) return -2;

  int buffer_offset = 0;
  while (buffer_offset < received) {
    if ((received - buffer_offset) < WL_HEADER_SIZE) return 1;

    uint32_t tmp = 0;
    uint32_t object_id = read_u32(buffer+buffer_offset, &tmp);
    uint16_t op = read_u16(buffer+buffer_offset, &tmp);
    uint16_t message_size = read_u16(buffer+buffer_offset, &tmp);

    // printf("RECV BUFFER: ");
    // print_buffer(buffer+buffer_offset, message_size);

    if ((ret = dispatch(conn, 
                        object_id, op, message_size, buffer+buffer_offset)) != 0) {
      break;
    }
    buffer_offset+=message_size;
  }

  return ret;

}

int wl_object_add(struct wl_connection *conn, wl_new_id id, const struct wl_interface *interface, void *data) {
  if (wl_object_get(conn, id)) return 1;

  struct wl_object new_object = {
    .id = id,
    .iface = interface,
    .data = data,
  };

  c_map_set(conn->objects, id, &new_object, sizeof(struct wl_object));

  if (id < 0xFF000000) 
    c_bitmap_set(conn->client_id_pool, id - 1);

  else
    c_bitmap_set(conn->server_id_pool, id - 1);

  return 0;
}

inline struct wl_object *wl_object_get(struct wl_connection *conn, wl_object_id id) {
  return c_map_get(conn->objects, id);
}

int wl_object_del(struct wl_connection *conn, wl_object_id id) {
  if (!c_map_get(conn->objects, id)) return 1;

  c_map_remove(conn->objects, id);
  if (id < 0xFF000000) 
    c_bitmap_unset(conn->client_id_pool, id - 1);

  else
    c_bitmap_unset(conn->server_id_pool, id - 1);

  return 0;
}

struct wl_connection *wl_connection_init(int client_fd) {
  struct wl_connection *conn = calloc(1, sizeof(struct wl_connection));
  if (!conn) {
    perror("wl_connection_init: calloc(wl_connection): ");
    return NULL;
  }

  conn->objects = c_map_new(512);
  conn->callback_queue = c_map_new(512);
  conn->client_id_pool = c_bitmap_new(4096);
  conn->server_id_pool = c_bitmap_new(4096);
  conn->client_fd = client_fd;

  wl_object_add(conn, 1, (struct wl_interface *)wl_interface_get("wl_display"), NULL);

  return conn;
}

int wl_connection_free(struct wl_connection *conn) {
  c_map_destroy(conn->objects);
  c_bitmap_destroy(conn->client_id_pool);
  c_bitmap_destroy(conn->server_id_pool);
  return 0;
}

