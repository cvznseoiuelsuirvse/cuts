#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

#include "wayland/error.h"
#include "wayland/server.h"
#include "wayland/types/wayland.h"

#include "util/helpers.h"
#include "util/log.h"
#include "util/event_loop.h"

#ifdef CUTS_LOGS
#define c_wl_printf(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define c_wl_printf(fmt, ...)
#endif


static struct c_wl_interface *__interface[C_WL_MAX_INTERFACES];
static size_t __ninterfaces = 0;

struct c_wl_recv_message {
  uint32_t object_id;
  uint16_t op;
  uint16_t message_size;
  char    *buffer;
};

struct c_wl_callback {
	c_wl_object_id callback_id;
	c_wl_object_id target_id;
};

inline void c_wl_interface_add(struct c_wl_interface *interface) {
  assert(__ninterfaces < C_WL_MAX_INTERFACES);
  __interface[__ninterfaces++] = interface; 
}

inline struct c_wl_interface *c_wl_interface_get(const char *interface_name) {
  for (size_t i = 0; i < __ninterfaces; i++) {
    struct c_wl_interface *interface = __interface[i];
    // printf("%s %s %d\n", interface_name, interface->name, STREQ(interface_name, interface->name));
    if (STREQ(interface_name, interface->name)) return interface;
  }
  return NULL;
}


static int create_socket(struct c_wl_display *display) {
  int fd;
  const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (!xdg_runtime_dir) {
    fprintf(stderr, "XDG_RUNTIME_DIR env not set\n");
    return -1;
  }

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    return -1;
  }

  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;

  for(size_t i = 0; i < 1000; i++) {
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/wayland-%ld", xdg_runtime_dir, i);
    if (access(addr.sun_path, F_OK) == -1) break;
    addr.sun_path[0] = 0;
  }

  if (!*addr.sun_path) {
    fprintf(stderr, "all sockets are taken\n");
    close(fd);
    return -1;
  }

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("c_create_socket");
    close(fd);
    return -1;
  }

  if (listen(fd, 16) == -1) {
    perror("c_create_socket");
    close(fd);
    return -1;
  }

  snprintf(display->socket_path, sizeof(display->socket_path), "%s", addr.sun_path);
  c_log(C_LOG_INFO, "Created wayland socket at %s", display->socket_path);
  return fd;
}
// static inline void unlink_socket() {
//   if (*__socket_path) unlink(__socket_path); 
// }

static inline int set_nonblocking(int fd) {
  int flags;

  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return flags;

  flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}

static c_event_callback_errno client_epoll_callback(struct c_event_loop *loop, int fd, void *data) {
  struct c_wl_connection *connection = data;
  int ret = c_wl_connection_dispatch(connection);
  if (ret == 1) {
    c_wl_connection_free(connection);
    ret = C_EVENT_ERROR_WL_CLIENT_GONE;
  } else if (ret == -1) {
    c_wl_error_send(connection);
    ret = C_EVENT_ERROR_WL_PROTO;
  }

  return -ret;

}

static c_event_callback_errno server_epoll_callback(struct c_event_loop *loop, int fd, void *data) {
  int client_fd = accept(fd, NULL, NULL);

  if (set_nonblocking(client_fd) == -1) {
    c_log(C_LOG_WARNING, "failed to set client fd to non-blocking");
  }
  
  struct c_wl_connection *connection = c_wl_connection_init(client_fd);
  if (!connection) {
    c_log(C_LOG_ERROR, "c_wl_connection_init failed");
    return -C_EVENT_ERROR_FATAL;
  }
  

  if (c_event_loop_add(loop, client_fd, client_epoll_callback, connection) == -1) {
    c_log(C_LOG_ERROR, "c_event_loop_add failed");
    return -C_EVENT_ERROR_FATAL;
  }

  return C_EVENT_OK;
}


struct c_wl_display *c_wl_display_init() {
  struct c_event_loop *loop = c_event_loop_init();
  if (!loop) {
    c_log(C_LOG_ERROR, "c_event_loop_init() failed");
    return NULL;
  }

  struct c_wl_display *display = calloc(1, sizeof(struct c_wl_display));
  if (!display) {
    c_log(C_LOG_ERROR, "failed to calloc");
    return NULL;
  }

  int fd = create_socket(display);
  if (fd == -1) {
    c_log(C_LOG_ERROR, "failed to set client fd to non-blocking");
    free(display);
    return NULL;
  }

  display->loop = loop;
  c_event_loop_add(loop, fd, server_epoll_callback, display);

  return display;

}

void c_wl_display_free(struct c_wl_display *display) {
  if (display->loop) c_event_loop_free(display->loop);
  if (*display->socket_path) unlink(display->socket_path);
  free(display);
}

static int c_wl_connection_read(struct c_wl_connection *conn, char *buffer, size_t buffer_size) {
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
    conn->req_fd = *(int *)CMSG_DATA(c);
  }

  return n;
}

int c_wl_connection_callback_add(struct c_wl_connection *conn, c_wl_object_id callback_id, c_wl_object_id target_id) {
  if (c_wl_object_get(conn, callback_id)) return -1;

  struct c_wl_callback callback = {callback_id, target_id};
  c_list_push(conn->callback_queue, &callback, sizeof(struct c_wl_callback));
  c_wl_object_add(conn, callback_id, c_wl_interface_get("wl_callback"), NULL);
  return 0;
}

void c_wl_connection_callback_done(struct c_wl_connection *conn, c_wl_object_id target_id) {
  struct c_wl_callback *callback = NULL;

  if (!target_id)
    callback = c_list_get_last(conn->callback_queue);
  else {
    struct c_wl_callback *c;
    c_list *_c = conn->callback_queue;
    c_list_for_each(_c, c) {
      if (c->target_id == target_id) {
        callback = c;
        break;
      }
    }
  }

  if (callback) {
    wl_callback_done(conn, callback->callback_id, C_WL_SERIAL);
    c_wl_object_del(conn, callback->callback_id);
    wl_display_delete_id(conn, 1, callback->callback_id);
    c_list_remove_ptr(&conn->callback_queue, callback);
  }
}


static int c_wl_connection_write(struct c_wl_connection *conn, char *buffer, size_t buffer_size) {
  if (conn->event_fd > 0) {
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
    c->cmsg_len = CMSG_LEN(sizeof(conn->event_fd));
    *(int *)CMSG_DATA(c) = conn->event_fd;

    return sendmsg(conn->client_fd, &m, MSG_NOSIGNAL);
  }

  return send(conn->client_fd, buffer, buffer_size, MSG_NOSIGNAL);

}

int c_wl_connection_send(struct c_wl_connection *conn, struct c_wl_message *msg, size_t nargs, ...) {
  va_list args;
  va_start(args, nargs);

  struct c_wl_object *object = c_wl_object_get(conn, msg->id);

  char buffer[C_WL_CONN_BUFFER_SIZE] = {0};
  uint32_t offset = 0;
  write_u32(buffer, &offset, msg->id);
  write_u16(buffer, &offset, msg->op);
  offset += sizeof(uint16_t);

  union c_wl_arg wl_args[nargs];
  c_wl_array *arr;

  for (size_t i = 0; i < nargs; i++) {
    char c = msg->signature[i];

    switch (c) {
    case 'i':
      wl_args[i].i = va_arg(args, c_wl_int);
      write_i32(buffer, &offset, wl_args[i].i);
      break;

    case 'e':
      wl_args[i].e = va_arg(args, c_wl_enum);
      write_i32(buffer, &offset, wl_args[i].e);
      break;

    case 'f':
      wl_args[i].f = va_arg(args, uint32_t);
      write_i32(buffer, &offset, wl_args[i].f);
      break;

    case 'u':
      wl_args[i].u = va_arg(args, c_wl_uint);
      write_u32(buffer, &offset, wl_args[i].u);
      break;

    case 'o':
      wl_args[i].o = va_arg(args, c_wl_object_id);
      write_u32(buffer, &offset, wl_args[i].o);
      break;

    case 'n':
      wl_args[i].n = va_arg(args, c_wl_new_id);
      write_u32(buffer, &offset, wl_args[i].n);
      break;

    case 's':
      snprintf(wl_args[i].s, sizeof(wl_args[i].s), "%s", va_arg(args, c_wl_string));
      write_string(buffer, &offset, wl_args[i].s);
      break;

    case 'a':
      arr = va_arg(args, c_wl_array*);
      write_array(buffer, &offset, arr->data, arr->size);
      wl_args[i].a = arr;
      break;

    case 'F':
      conn->event_fd = va_arg(args, c_wl_fd);
      wl_args[i].F = conn->event_fd;
      break;
    }
  }

  *(uint16_t *)(buffer + 6) = offset;

  c_wl_print_event(conn->client_fd, object, msg->event_name, wl_args, nargs, msg->signature);
  c_wl_connection_write(conn, buffer, offset);

  return 0;
}

static int dispatch(struct c_wl_connection *conn, 
                    c_wl_object_id object_id, uint16_t op, uint16_t message_size, 
                    char *buffer) {

  struct c_wl_object *object = c_wl_object_get(conn, object_id);
  if (!object) return c_wl_error_set(object_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "object not registered");

  const struct c_wl_interface *iface = object->iface;
  if (op > iface->nrequests) 
    return c_wl_error_set(object_id, WL_DISPLAY_ERROR_INVALID_METHOD, "method does not exist", 
                       iface->name, op, op, iface->nrequests);

  
  struct c_wl_request request = iface->requests[op];

  union c_wl_arg args[request.nargs + 1];
  c_wl_array arr = {0};
  args[0].o = object_id;

  uint32_t offset = C_WL_HEADER_SIZE;
  for (size_t i = 1; i <= request.nargs; i++) {
    uint8_t c = request.signature[i-1];
    assert(c != 'a');

    switch (c) {
      case 'u': 
        args[i].u = read_u32(buffer, &offset);
        break;

      case 'i': 
        args[i].i = read_i32(buffer, &offset);
        break;

      case 'f': 
        args[i].f = read_f32(buffer, &offset);
        break;

      case 'o': 
        args[i].o = read_u32(buffer, &offset);
        break;

      case 'n': 
        args[i].n = read_u32(buffer, &offset);
        break;

      case 's': 
        read_string(buffer, &offset, args[i].s, C_WL_MAX_STRING_SIZE);
        break;

      case 'e': 
        args[i].e = read_i32(buffer, &offset);
        break;

      case 'F':
        args[i].F = conn->req_fd;
        break;

      case 'a':
        arr.size = read_u32(buffer, &offset);
        arr.data = read_array(buffer, &offset, arr.size);
        args[i].a = &arr;
        break;
        
    }
  }


  if (!request.handler) {
    c_wl_error_set(object_id, WL_DISPLAY_ERROR_IMPLEMENTATION, 
                   "%s.%s method not implemented", iface->name, request.name);
    return -1;
  }

  c_wl_print_request(conn->client_fd, object, &request, args);
  int ret = request.handler(conn, args, request.handler_data);

  if (arr.data) free(arr.data);

  return ret;

}

int c_wl_connection_dispatch(struct c_wl_connection *conn) {
  int ret;
  char buffer[C_WL_CONN_BUFFER_SIZE];

  int received = c_wl_connection_read(conn, buffer, C_WL_CONN_BUFFER_SIZE);
  if (received <= 0) return 1;

  // print_buffer(buffer, received);
  // printf("\nreceived %d\n", received);

  size_t msg_count = 0;
  struct c_wl_recv_message msgs[1024];

  int buffer_offset = 0;
  c_wl_object_id last_object = 0;

  while (buffer_offset < received) {
    if ((received - buffer_offset) < C_WL_HEADER_SIZE) return -1;
    if (msg_count > LENGTH(msgs))                    return -1;

    uint32_t tmp = 0;
    uint32_t object_id = 
      read_u32(buffer+buffer_offset, &tmp);
    uint16_t op = 
      read_u16(buffer+buffer_offset, &tmp);
    uint16_t message_size = 
      read_u16(buffer+buffer_offset, &tmp);


    if (object_id == 1 && op == 0 && message_size == C_WL_HEADER_SIZE + sizeof(uint32_t)) {
      c_wl_object_id c_wl_callback_id = read_u32(buffer+buffer_offset, &tmp);
      if (c_wl_connection_callback_add(conn, c_wl_callback_id, last_object) == -1) {
        c_wl_error_set(1, WL_DISPLAY_ERROR_INVALID_OBJECT, "object %d already registered", c_wl_callback_id);
        return -1;
      }

      dispatch(conn, object_id, op, message_size, buffer+buffer_offset);
    } else {
      struct c_wl_recv_message *msg = &msgs[msg_count++];
      msg->object_id = object_id;
      msg->op = op;
      msg->message_size = message_size;
      msg->buffer = buffer+buffer_offset;
    }

    last_object = object_id;
    buffer_offset+=message_size;
  }

  for (size_t i = 0; i < msg_count; i++) {
    struct c_wl_recv_message msg = msgs[i];
    ret = dispatch(conn, msg.object_id, msg.op, msg.message_size, msg.buffer);
    if (ret != 0) 
      break;
  }

  c_wl_connection_callback_done(conn, 0);

  return ret;
}

int c_wl_object_add(struct c_wl_connection *conn, c_wl_new_id id, const struct c_wl_interface *interface, void *data) {
  if (c_wl_object_get(conn, id)) return -1;
  assert(interface);

  struct c_wl_object new_object = {
    .id = id,
    .iface = interface,
    .data = data,
  };

  c_map_set(conn->objects, id, &new_object, sizeof(struct c_wl_object));

  if (0 < id && id < 0xFF000000) 
    c_bitmap_set(conn->client_id_pool, id - 1);
  else if (id == 0) {
    c_wl_object_id free_id = c_bitmap_get_free(conn->server_id_pool);
    c_bitmap_set(conn->server_id_pool, free_id);
    return free_id + 0xFF000000;
  } else
    c_bitmap_set(conn->server_id_pool, id - 1);

  return id;
}

inline struct c_wl_object *c_wl_object_get(struct c_wl_connection *conn, c_wl_object_id id) {
  return c_map_get(conn->objects, id);
}

int c_wl_object_del(struct c_wl_connection *conn, c_wl_object_id id) {
  if (!c_map_get(conn->objects, id)) return 1;

  c_map_remove(conn->objects, id);
  if (id < 0xFF000000) 
    c_bitmap_unset(conn->client_id_pool, id - 1);

  else
    c_bitmap_unset(conn->server_id_pool, id - 1);

  return 0;
}

struct c_wl_connection *c_wl_connection_init(int client_fd) {
  struct c_wl_connection *conn = calloc(1, sizeof(struct c_wl_connection));
  if (!conn) {
    perror("calloc");
    return NULL;
  }

  conn->objects = c_map_new(512);
  conn->callback_queue = c_list_new();
  conn->client_id_pool = c_bitmap_new(4096);
  conn->server_id_pool = c_bitmap_new(4096);
  conn->client_fd = client_fd;

  c_wl_object_add(conn, 1, (struct c_wl_interface *)c_wl_interface_get("wl_display"), NULL);

  return conn;
}

int c_wl_connection_free(struct c_wl_connection *conn) {
  c_map_destroy(conn->objects);
  c_list_destroy(conn->callback_queue);
  c_bitmap_destroy(conn->client_id_pool);
  c_bitmap_destroy(conn->server_id_pool);
  return 0;
}
