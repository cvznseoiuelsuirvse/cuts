#include "server/server.h"
#include "server/error.h"
#include "backend/drm.h"
#include "server/wayland.h"
#include "util/helpers.h"
                                                                      
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static const struct wl_interface *__interface[CUTS_MAX_INTERFACES];
static size_t __ninterfaces = 0;


inline void c_interface_add_global(const struct wl_interface *interface) {
  assert(__ninterfaces < CUTS_MAX_INTERFACES);
  __interface[__ninterfaces++] = interface; 
}

inline const struct wl_interface *c_interface_get_global(const char *interface_name) {
  for (size_t i = 0; i < __ninterfaces; i++) {
    const struct wl_interface *interface = __interface[i];
    if (CUTS_STREQ(interface_name, interface->name)) return interface;
  }
  return NULL;
}

static inline int set_nonblocking(int fd) {
  int flags;

  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return flags;

  flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}

static int wl_connection_read(struct wl_connection *conn, char *buffer, size_t buffer_size) {
  char cmsg[CMSG_SPACE(sizeof(int))];
  struct iovec e = {buffer, buffer_size};
  struct msghdr m = {NULL, 0, &e, 1, cmsg, sizeof(cmsg), 0};

  ssize_t n = recvmsg(conn->client_fd, &m, MSG_DONTWAIT);
  struct cmsghdr *c = CMSG_FIRSTHDR(&m);
  if (c != NULL)
    conn->msg_fd = *(int *)CMSG_DATA(c);

  return n;

}

static int wl_connection_write(struct wl_connection *conn, char *buffer, size_t buffer_size) {
  if (conn->msg_fd > 0) {
    struct msghdr m;
    char cmsg[CMSG_SPACE(sizeof(conn->msg_fd))];

    memset(cmsg, 0, CMSG_SPACE(sizeof(conn->msg_fd)));
    memset(&m, 0, sizeof(struct msghdr));

    struct iovec e = {buffer, buffer_size};
    m.msg_iov = &e;
    m.msg_iovlen = 1;
    m.msg_control = cmsg;
    m.msg_controllen = CMSG_SPACE(sizeof(conn->msg_fd));

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
      write_i32(buffer, &offset, va_arg(args, int32_t));
      break;

    case 'u':
    case 'o':
    case 'n':
      write_u32(buffer, &offset, va_arg(args, uint32_t));
      break;

    case 's':
      write_string(buffer, &offset, va_arg(args, char *));
      break;

    }
  }

  conn->msg_fd = msg->fd;
  *(uint16_t *)(buffer + 6) = offset;
  printf("SEND BUFFER: ");
  print_buffer(buffer, offset);
  wl_connection_write(conn, buffer, offset);
  return 0;
}

static int dispatch_message(struct wl_connection *conn, char *buffer) {
  uint32_t offset = 0;

  uint32_t object_id = read_u32(buffer, &offset);
  uint16_t op = read_u16(buffer, &offset);
  offset+=sizeof(uint16_t);

  struct wl_interface *iface = c_map_get(conn->objects, object_id);
  if (!iface) return c_error_set(object_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "object not registered");
  if (op > iface->nrequests) 
    return c_error_set(object_id, WL_DISPLAY_ERROR_INVALID_METHOD, "method %d doesn't exist", op);

  
  struct wl_request request = iface->requests[op];
  union wl_arg args[request.nargs];

  for (size_t i = 0; i < request.nargs; i++) {
    char c = request.signature[i];
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
        read_string(buffer, &offset, args[i].s, WL_MAX_STRING_SIZE);
        break;
      case 'e': 
        args[i].e = read_i32(buffer, &offset);
        break;
      case 'F':
        args[i].F = conn->msg_fd;
        break;
    }

    offset++;
  }

  return request.handler(conn, args);

}

static int wl_connection_dispatch(struct wl_connection *conn, char *buffer, size_t buffer_size) {
  int ret;

  print_buffer(buffer, buffer_size);
  uint32_t buffer_offset = 0;
  while (buffer_offset < buffer_size) {
    if ((buffer_size - buffer_offset) < WL_HEADER_SIZE) return 1;

    uint32_t tmp = 0;
    read_u32(buffer+buffer_offset, &tmp);
    read_u16(buffer+buffer_offset, &tmp);
    uint16_t message_size = read_u16(buffer+buffer_offset, &tmp);

    printf("buffer_size: %ld buffer_offset: %d message_size: %d\n", buffer_size, buffer_offset, message_size);

    printf("RECV BUFFER: ");
    print_buffer(buffer+buffer_offset, message_size);

    if ((ret = dispatch_message(conn, buffer+buffer_offset)) != 0) {
      break;
    }
    buffer_offset+=message_size;
  }

  return ret;

}

int c_object_add(struct wl_connection *conn, wl_new_id id, const struct wl_interface *interface) {
  struct wl_interface *registered = c_map_get(conn->objects, id);
  if (registered) return 1;

  c_map_set(conn->objects, id, (struct wl_interface *)interface, 0);
  if (id < 0xFF000000) 
    c_bitmap_set(conn->client_id_pool, id - 1);

  else
    c_bitmap_set(conn->server_id_pool, id - 1);

  return 0;
}

inline struct wl_interface *c_object_get(struct wl_connection *conn, wl_object id) {
  return c_map_get(conn->objects, id);
}

int c_object_del(struct wl_connection *conn, wl_object id) {
  struct wl_requests *registered = c_map_get(conn->objects, id);
  if (!registered) return 1;

  c_map_remove(conn->objects, id);
  if (id < 0xFF000000) 
    c_bitmap_unset(conn->client_id_pool, id - 1);

  else
    c_bitmap_unset(conn->server_id_pool, id - 1);

  return 0;
}

struct wl_connection *wl_connection_init(struct c_event_resource *resource) {
  struct wl_connection *conn = calloc(1, sizeof(struct wl_connection));
  if (!conn) {
    perror("wl_connection_init: calloc(wl_connection): ");
    return NULL;
  }

  conn->objects = c_map_new(512);
  conn->client_id_pool = c_bitmap_new(4096);
  conn->server_id_pool = c_bitmap_new(4096);
  conn->client_fd = resource->fd;

  c_object_add(conn, 1, (struct wl_interface *)c_interface_get_global("wl_display"));

  return conn;
}

int wl_connection_free(struct wl_connection *conn) {
  c_map_destroy(conn->objects);
  c_bitmap_destroy(conn->client_id_pool);
  c_bitmap_destroy(conn->server_id_pool);
  return 0;
}

int c_create_socket(struct c_event_resource *sock) {
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

  sock->fd = fd;
  sock->type = SERVER;
  sock->data = malloc(sizeof(addr.sun_path));
  if (!sock->data) {
    fprintf(stderr, "c_create_socket: failed to malloc\n");
    c_unlink_socket(sock);
    return -1;
  }

  snprintf(sock->data, sizeof(addr.sun_path), "%s", addr.sun_path);
  return fd;
}

inline void c_unlink_socket(struct c_event_resource *sock) {
  if (sock->data) { 
    unlink((const char *)sock->data); 
    free(sock->data);
  }
}

struct c_event_loop * c_event_loop_init() {
  struct c_event_loop *loop = calloc(1, sizeof(struct c_event_loop));

  loop->epfd = epoll_create1(0);
  if (loop->epfd == -1) {
    fprintf(stderr, "failed to epoll_create1\n");
    free(loop);
    return NULL;
  }

  loop->resources = c_list_new();
  loop->connections = c_list_new();
  return loop;
}

int c_event_loop_add(struct c_event_loop *loop, struct c_event_resource *resource) {
  struct c_event_resource *resource_cpy = c_list_push(loop->resources, resource, sizeof(struct c_event_resource));

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
  ev.data.ptr = resource_cpy;
  if (epoll_ctl(loop->epfd, EPOLL_CTL_ADD, resource->fd, &ev) == -1) {
    fprintf(stderr, "failed to add event %d\n", resource->fd);
    return -1;
  }
  return 0;
}

int c_event_loop_del(struct c_event_loop *loop, struct c_event_resource *resource) {
  if (epoll_ctl(loop->epfd, EPOLL_CTL_DEL, resource->fd, NULL) == -1) {
    fprintf(stderr, "failed to del event %d\n", resource->fd);
    return -1;
  }
  close(resource->fd);
  c_list_remove_ptr(&loop->resources, resource);
  return 0;
}


void c_event_loop_free(struct c_event_loop *loop) {
  struct c_event_resource *resource;
  struct c_list *l = loop->resources;
  c_list_for_each(l, resource)  {
    if (close(resource->fd) == -1)
      perror("c_event_loop_free");
  } 
  c_list_destroy(loop->resources);
  c_list_destroy(loop->connections);
  free(loop);
}

int c_event_loop_run(struct c_event_loop *loop) {
  int ret = 0;
  int n;
  struct epoll_event *events = loop->events;

  while (1) {
    n = epoll_wait(loop->epfd, loop->events, CUTS_MAX_EPOLL_EVENTS, -1);
    if (n == -1) {
      perror("epoll_wait");
      return -1;
    }
  
    for (int i = 0; i < n; i++) {
      struct c_event_resource *resource = events[i].data.ptr;
      if ((events[i].events & EPOLLIN) && resource->type == SERVER) {
          int client_fd = accept(resource->fd, NULL, NULL);
          if ((ret =set_nonblocking(client_fd)) == -1) goto out;

          struct c_event_resource resource = {0};
          resource.fd = client_fd;
          resource.type = CLIENT;

          if ((ret = c_event_loop_add(loop, &resource)) == -1) goto out;
        
        
      } else if ((events[i].events && EPOLLIN) && resource->type == CLIENT) {
        struct wl_connection *conn = resource->data;
        if (!conn) {
          conn = wl_connection_init(resource);
          if (!conn) {
            fprintf(stderr, "failed to wl_connection_init on %d\n", resource->fd);
            continue;
          }
          resource->data = conn;
        }

        char buffer[WL_CONN_BUFFER_SIZE];
        int received = wl_connection_read(conn, buffer, WL_CONN_BUFFER_SIZE);
        if (received == 0) {
          c_event_loop_del(loop, resource);
          printf("client disconnected\n");

        }
        else if (wl_connection_dispatch(conn, buffer, received) == -1) 
          c_error_send(conn);

      } else if ((events[i].events & EPOLLIN) && resource->type == DRM) {
        struct c_drm_backend *backend = resource->data;
        c_drm_backend_page_flip(backend);

      } else if (events[i].events &  (EPOLLHUP | EPOLLERR)) {
        c_event_loop_del(loop, resource);
      }
        
    }
  }

out:
  return ret;
}

