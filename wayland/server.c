#include <sys/mman.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <signal.h>

#include "wayland/util.h"
#include "wayland/display.h"
#include "wayland/server.h"
#include "wayland/proto/wayland.h"

#include "util/helpers.h"
#include "util/log.h"
#include "util/malloc.h"
#include "util/bitmap.h"

#define MAX_CMSG_FDS 1024

static char            __error_msg[C_WL_STRING_SIZE] = {0};
static c_wl_int        __error_code = 0;
static c_wl_object_id  __error_object_id = 0;
static int        __serial = 1;

struct c_wl_connection {
	struct c_wl_display *display;
	int 	     client_fd;
	c_map     *objects;
	c_bitmap 	*client_id_pool;
	c_bitmap 	*server_id_pool;
  struct c_wl_connection_event_listener *listener;
  void *listener_data;
};

struct client_message {
  uint32_t object_id;
  uint16_t op;
  uint16_t message_size;
  char    *buffer;
};

struct c_wl_callback {
	c_wl_object_id callback_id;
	c_wl_object_id target_id;
};


static int connection_read(struct c_wl_connection *conn, char *buffer, size_t buffer_size, 
                                int req_fds[MAX_CMSG_FDS], size_t *n_req_fds) {
  char cmsg[CMSG_SPACE(sizeof(int) * MAX_CMSG_FDS)];

  struct iovec e[1];
  e[0].iov_base = buffer;
  e[0].iov_len = buffer_size;

  struct msghdr m = {0};
  m.msg_iov = e; 
  m.msg_iovlen = 1; 
  m.msg_control = cmsg; 
  m.msg_controllen = sizeof(cmsg); 

  struct cmsghdr *cmsghdr;
  ssize_t n = recvmsg(conn->client_fd, &m, 0);
  for (cmsghdr = CMSG_FIRSTHDR(&m); 
       cmsghdr != NULL;
       cmsghdr = CMSG_NXTHDR(&m, cmsghdr)) {
    if (cmsghdr->cmsg_len == 0) continue;

    int *fds = (int *)CMSG_DATA(cmsghdr);
    size_t n_fds = (cmsghdr->cmsg_len - CMSG_LEN(0)) / sizeof(int);

    for (size_t i = 0; i < n_fds; i++) {
      req_fds[(*n_req_fds)++] = fds[i];
    }
  }
  return n;
}

void c_wl_connection_callback_done(struct c_wl_connection *conn, c_wl_object_id id) {
    wl_callback_done(conn, id, c_wl_serial());
    c_wl_object_del(conn, id);
}

static int c_wl_connection_write(struct c_wl_connection *conn, char *buffer, size_t buffer_size, int event_fd) {
  if (event_fd > 0) {
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
    c->cmsg_len = CMSG_LEN(sizeof(event_fd));
    *(int *)CMSG_DATA(c) = event_fd;

    return sendmsg(conn->client_fd, &m, MSG_NOSIGNAL);
  }

  return send(conn->client_fd, buffer, buffer_size, MSG_NOSIGNAL);

}

int c_wl_connection_send(struct c_wl_connection *conn, struct c_wl_message *msg, size_t nargs, ...) {
  va_list args;
  va_start(args, nargs);

  struct c_wl_object *object = c_wl_object_get(conn, msg->id);
  if (!object) {
    c_log(C_LOG_ERROR, "no objects registered with id %d", msg->id);
    raise(SIGTERM);
  }

  char buffer[C_WL_CONN_BUF_SIZE] = {0};
  uint32_t offset = 0;
  write_u32(buffer, &offset, msg->id);
  write_u16(buffer, &offset, msg->op);
  offset += sizeof(uint16_t);

  union c_wl_arg wl_args[nargs];
  c_wl_array *arr;
  int event_fd = 0;

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
      wl_args[i].f = va_arg(args, c_wl_fixed);
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
      event_fd = va_arg(args, c_wl_fd);
      wl_args[i].F = event_fd;
      break;
    }
  }

  *(uint16_t *)(buffer + 6) = offset;

  c_log_wl_event(conn, object, msg->event_name, wl_args, nargs, msg->signature);
  c_wl_connection_write(conn, buffer, offset, event_fd);

  return 0;
}

static int handle_request(struct c_wl_connection *conn,
                          struct c_wl_object *object, c_wl_args args,
                          uint16_t op) {
  struct c_wl_request request = object->iface->requests[op];
  int destructor = object->iface->destructor_request;
  void *userdata = object->listeners.userdata;
  void **handlers = object->listeners.handlers;

  c_wl_interface_listener_handler handler = NULL;
  if (handlers)
     handler = handlers[op];

  int status = 0;

  if (handler && destructor == op) {
    status = handler(conn, args, userdata);
    if (status) goto out;
  } else {
    status |= 1;
  }

  if (request.impl) {
    status = request.impl(conn, args);
    if (status) goto out;
  } else {
    status |= 1 << 1;
  }

  if (handler && destructor != op)
    status = handler(conn, args, userdata);

out:
  return status;
}

static int dispatch(struct c_wl_connection *conn, 
                    c_wl_object_id object_id, uint16_t op, uint16_t message_size, 
                    char *buffer, int **req_fds) {

  struct c_wl_object *object = c_wl_object_get(conn, object_id);
  if (!object) c_wl_error_set_and_return(object_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "object not registered");

  const struct c_wl_interface *iface = object->iface;
  if (op > iface->n_requests)
    c_wl_error_set_and_return(object_id, WL_DISPLAY_ERROR_INVALID_METHOD,
                              "%s: method with op %d does not exist",
                              iface->name, op);

  struct c_wl_request request = iface->requests[op];

  union c_wl_arg args[request.nargs + 1];
  c_wl_array arr = {0};
  args[0].o = object_id;

  uint32_t offset = C_WL_CONN_HEADER_SIZE;
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
        args[i].f = read_u32(buffer, &offset);
        break;

      case 'o': 
        args[i].o = read_u32(buffer, &offset);
        break;

      case 'n': 
        args[i].n = read_u32(buffer, &offset);
        break;

      case 's': 
        read_string(buffer, &offset, args[i].s, C_WL_STRING_SIZE);
        break;

      case 'e': 
        args[i].e = read_i32(buffer, &offset);
        break;

      case 'F':
        args[i].F = **req_fds;
        (*req_fds)++;
        break;

      case 'a':
        arr.size = read_u32(buffer, &offset);
        arr.data = read_array(buffer, &offset, arr.size);
        args[i].a = &arr;
        break;
        
    }
  }


  c_log_wl_request(conn, object, &request, args);

  if (!request.impl) {
    c_log(C_LOG_ERROR, "%s.%s method not implemented", iface->name, request.name);
    return DISPATCH_FATAL_ERR;
  }

  int status = handle_request(conn, object, args, op);
  if (arr.data) free(arr.data);
  return status;

}

int c_wl_connection_dispatch(struct c_wl_connection *conn) {
  int ret;
  char buffer[C_WL_CONN_BUF_SIZE];

  int req_fds[MAX_CMSG_FDS];
  size_t n_req_fds = 0;

  int received = connection_read(conn, buffer, C_WL_CONN_BUF_SIZE, req_fds, &n_req_fds);
  if (received <= 0) return DISPATCH_CLIENT_ERR;

  size_t msg_count = 0;
  struct client_message msgs[1024];

  int buffer_offset = 0;

  size_t n_sync_requests = 0;
  c_wl_object_id sync_requests[16] = {0};

  while (buffer_offset < received) {
    if ((received - buffer_offset) < C_WL_CONN_HEADER_SIZE) return -1;
    if (msg_count > LENGTH(msgs))                    return -1;

    uint32_t tmp = 0;
    uint32_t object_id = 
      read_u32(buffer+buffer_offset, &tmp);
    uint16_t op = 
      read_u16(buffer+buffer_offset, &tmp);
    uint16_t message_size = 
      read_u16(buffer+buffer_offset, &tmp);


    if (object_id == 1 && op == 0 && message_size == C_WL_CONN_HEADER_SIZE + sizeof(uint32_t)) {
      c_wl_object_id wl_callback_id = read_u32(buffer+buffer_offset, &tmp);
      if (c_wl_object_get(conn, wl_callback_id))
        c_wl_error_set_and_return(1, WL_DISPLAY_ERROR_INVALID_OBJECT,
                                  "object %d already registered",
                                  wl_callback_id);

      const struct c_wl_interface *iface = c_wl_interface_get("wl_callback"); 
      c_wl_object_add(conn, wl_callback_id, iface->version, iface, NULL);
      sync_requests[n_sync_requests++] = wl_callback_id;
      dispatch(conn, object_id, op, message_size, buffer+buffer_offset, 0);

    } else {
      struct client_message *msg = &msgs[msg_count++];
      msg->object_id = object_id;
      msg->op = op;
      msg->message_size = message_size;
      msg->buffer = buffer+buffer_offset;
    }

    buffer_offset+=message_size;
  }

  int *req_fds_ptr = req_fds;
  for (size_t i = 0; i < msg_count; i++) {
    struct client_message msg = msgs[i];
    ret = dispatch(conn, msg.object_id, msg.op, msg.message_size, msg.buffer, &req_fds_ptr);
    if (ret == DISPATCH_PROTO_ERR) 
      break;

    if (ret == DISPATCH_FATAL_ERR)
      goto out;
  }

  for (size_t i = 0; i < n_sync_requests; i++)
    c_wl_connection_callback_done(conn, sync_requests[i]);

out:
  return ret;
}

struct c_wl_object *c_wl_object_add(struct c_wl_connection *conn, c_wl_new_id id,
                    uint32_t version, const struct c_wl_interface *interface,
                    void *data) {

  if (c_wl_object_get(conn, id)) return NULL;
  assert(interface);

  struct c_wl_object new_object = {
    .conn = conn,
    .version = version,
    .iface = interface,
    .data = data,
    .listeners = {NULL, NULL},
  };

  c_wl_display_get_interface_listener(conn->display, interface->name,
                                      &new_object.listeners.handlers,
                                      &new_object.listeners.userdata);

  if (0 < id && id < 0xFF000000) {
    c_bitmap_set(conn->client_id_pool, id - 1);

  } else if (id == 0) {
    c_wl_object_id free_id = c_bitmap_get_free(conn->server_id_pool);
    c_bitmap_set(conn->server_id_pool, free_id);
    id = free_id + 0xFF000000;

  } else {
    c_bitmap_set(conn->server_id_pool, id - 1);
  }

  new_object.id = id;
  return c_map_set(conn->objects, id, &new_object, sizeof(struct c_wl_object));
}

void c_wl_object_add_listener(struct c_wl_object *object, void *listeners, void *userdata) {
  object->listeners.handlers = listeners;
  object->listeners.userdata = userdata;
}

void c_wl_object_free_listener(struct c_wl_object *object) {
  free(object->listeners.handlers);
}

inline struct c_wl_object *c_wl_object_get(struct c_wl_connection *conn, c_wl_object_id id) {
  return c_map_get(conn->objects, id);
}

int c_wl_object_del(struct c_wl_connection *conn, c_wl_object_id id) {
  struct c_wl_object *o = c_map_get(conn->objects, id);
  if (!o) return 1;
  
  if (o->data)
    c_unref(o->data);

  c_map_remove(conn->objects, id);
  if (id < 0xFF000000) {
    wl_display_delete_id(conn, 1, id);
    c_bitmap_unset(conn->client_id_pool, id - 1);
  } else {
    c_bitmap_unset(conn->server_id_pool, id - 0xFF000000);
  }

  return 0;
}

struct c_wl_connection *c_wl_connection_init(int client_fd, struct c_wl_display *display) {
  struct c_wl_connection *conn = calloc(1, sizeof(* conn));
  if (!conn) {
    c_log(C_LOG_ERROR, "failed to allocate new connection");
    return NULL;
  }

  conn->objects = c_map_new(1024);
  conn->client_id_pool = c_bitmap_new(1024 * 4);
  conn->server_id_pool = c_bitmap_new(1024 * 4);
  conn->client_fd = client_fd;
  conn->display = display;

  const struct c_wl_interface *iface = c_wl_interface_get("wl_display");
  c_wl_object_add(conn, 1, iface->version, iface, NULL);

  return conn;
}

struct c_wl_display *c_wl_connection_get_display(struct c_wl_connection *conn) {
  return conn->display;
}

struct c_map *c_wl_connection_get_objects(struct c_wl_connection *conn) {
  return conn->objects;
}

int c_wl_connection_free(struct c_wl_connection *conn) {
  for (int i = conn->objects->size - 1; i >= 0; i--) {
    struct c_map_pair *mp = conn->objects->pairs[i];

    while (mp) {
      struct c_map_pair *next = mp->next;
      struct c_wl_object *object = mp->value;

      c_log(C_LOG_DEBUG, "(%p) destroying %s#%d object", conn,
            object->iface->name, object->id);

      if (object->iface->destructor_request >= 0) {
        c_wl_arg arg = {.o = object->id};
        if (handle_request(conn, object, &arg,
                           object->iface->destructor_request) == 3) {
        // returns 3 if no handlers and no implementation for destructor
        // 3 == ((1 << 1) & 1)
          goto unref;
        }
        goto iter_end;
      }

unref:
      if (object->data) {
        c_unref(object->data);
      }

iter_end:
      mp = next;
    }
  }

  c_map_destroy(conn->objects);

  c_bitmap_destroy(conn->client_id_pool);
  c_bitmap_destroy(conn->server_id_pool);
  free(conn);
  return 0;
}


int _c_wl_error_set(c_wl_object_id object_id, c_wl_int code, c_wl_string msg, ...) {
  __error_code = code;
  __error_object_id = object_id;
  
  va_list args;
  va_start(args, msg);
  vsnprintf(__error_msg, C_WL_STRING_SIZE, msg, args);
  va_end(args);
  return -1;
}

void c_wl_error_send(struct c_wl_connection *conn) {
  wl_display_error(conn, 1, __error_object_id, __error_code, __error_msg);
  *__error_msg = 0;
  __error_code = 0;
  __error_object_id = 0;
}


inline int c_wl_serial() {
  __serial %= (1 << 31);
  return __serial++;
}
