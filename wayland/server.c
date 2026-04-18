#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>

#include "wayland/display.h"
#include "wayland/server.h"
#include "wayland/types/wayland.h"

#include "util/helpers.h"
#include "util/log.h"
#include "util/bitmap.h"

#define MAX_CMSG_FDS 32

static struct c_wl_interface *__interface[C_WL_MAX_INTERFACES];
static size_t                 __ninterfaces = 0;

static char            __error_msg[C_WL_STRING_SIZE] = {0};
static c_wl_int        __error_code = 0;
static c_wl_object_id  __error_object_id = 0;
static uint32_t        __serial = 1;

struct c_wl_connection {
	int 	     client_fd;
	c_map     *objects;
	c_bitmap 	*client_id_pool;
	c_bitmap 	*server_id_pool;
	struct c_wl_display *dpy;
};

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

struct c_wl_object_data {
  void *data;
  size_t ref_count;
};

inline void c_wl_interface_add(struct c_wl_interface *interface) {
  assert(__ninterfaces < C_WL_MAX_INTERFACES);
  __interface[__ninterfaces++] = interface; 
}

inline struct c_wl_interface *c_wl_interface_get(const char *interface_name) {
  for (size_t i = 0; i < __ninterfaces; i++) {
    struct c_wl_interface *interface = __interface[i];
    if (STREQ(interface_name, interface->name)) return interface;
  }
  return NULL;
}

static int c_wl_connection_read(struct c_wl_connection *conn, char *buffer, size_t buffer_size, 
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
    wl_display_delete_id(conn, 1, id);
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

  char buffer[C_WL_BUFFER_SIZE] = {0};
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

static int dispatch(struct c_wl_connection *conn, 
                    c_wl_object_id object_id, uint16_t op, uint16_t message_size, 
                    char *buffer, int **req_fds) {

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
  if (!request.handler) {
    c_wl_error_set(object_id, WL_DISPLAY_ERROR_IMPLEMENTATION, 
                   "%s.%s method not implemented", iface->name, request.name);
    return -1;
  }

  int ret = request.handler(conn, args);

  if (arr.data) free(arr.data);

  return ret;

}

int c_wl_connection_dispatch(struct c_wl_connection *conn) {
  int ret;
  char buffer[C_WL_BUFFER_SIZE];

  int req_fds[MAX_CMSG_FDS];
  size_t n_req_fds = 0;
  int received = c_wl_connection_read(conn, buffer, C_WL_BUFFER_SIZE, req_fds, &n_req_fds);
  if (received <= 0) return 1;

  size_t msg_count = 0;
  struct c_wl_recv_message msgs[1024];

  int buffer_offset = 0;

  size_t n_sync_requests = 0;
  c_wl_object_id sync_requests[4] = {0};

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
      c_wl_object_id wl_callback_id = read_u32(buffer+buffer_offset, &tmp);
      if (c_wl_object_get(conn, wl_callback_id)) {
        c_wl_error_set(1, WL_DISPLAY_ERROR_INVALID_OBJECT, "object %d already registered", wl_callback_id);
        return -1;
      }

      c_wl_object_add(conn, wl_callback_id, c_wl_interface_get("wl_callback"), NULL);
      sync_requests[n_sync_requests++] = wl_callback_id;
      dispatch(conn, object_id, op, message_size, buffer+buffer_offset, 0);

    } else {
      struct c_wl_recv_message *msg = &msgs[msg_count++];
      msg->object_id = object_id;
      msg->op = op;
      msg->message_size = message_size;
      msg->buffer = buffer+buffer_offset;
    }

    buffer_offset+=message_size;
  }

  int *req_fds_ptr = req_fds;
  for (size_t i = 0; i < msg_count; i++) {
    struct c_wl_recv_message msg = msgs[i];
    ret = dispatch(conn, msg.object_id, msg.op, msg.message_size, msg.buffer, &req_fds_ptr);
    if (ret != 0) 
      break;
  }

  for (size_t i = 0; i < n_sync_requests; i++)
    c_wl_connection_callback_done(conn, sync_requests[i]);

  return ret;
}

int c_wl_object_add(struct c_wl_connection *conn, c_wl_new_id id, const struct c_wl_interface *interface, struct c_wl_object_data *data) {
  if (c_wl_object_get(conn, id)) return -1;
  assert(interface);

  if (data)
    data->ref_count++;

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

struct c_wl_object_data *c_wl_object_data_create(void *data) {
  struct c_wl_object_data *odata = calloc(1, sizeof(*odata));
  if (!odata) return NULL;
  odata->data = data;
  return odata;
}

void *c_wl_object_data_get(struct c_wl_connection *conn, c_wl_object_id id) {
  struct c_wl_object *o = c_map_get(conn->objects, id);
  if (!o) return NULL;
  return o->data->data;
}

void *c_wl_object_data_get2(struct c_wl_object *object) {
  return object->data->data;
}

void c_wl_object_data_set(struct c_wl_object *object, void *data) {
  struct c_wl_object_data *odata = c_wl_object_data_create(data);
  odata->ref_count++;
  object->data = odata;
}

void c_wl_object_data_unref(struct c_wl_object *object) {
  object->data->ref_count--;
}

void c_wl_object_data_ref(struct c_wl_object *object) {
  object->data->ref_count++;
}

inline struct c_wl_object *c_wl_object_get(struct c_wl_connection *conn, c_wl_object_id id) {
  return c_map_get(conn->objects, id);
}

int c_wl_object_del(struct c_wl_connection *conn, c_wl_object_id id) {
  struct c_wl_object *o = c_map_get(conn->objects, id);
  if (!o) return 1;
  
  if (o->data) {
    if (--o->data->ref_count == 0) {
      free(o->data->data);
      free(o->data);
    }
  }

  c_map_remove(conn->objects, id);
  if (id < 0xFF000000) 
    c_bitmap_unset(conn->client_id_pool, id - 1);

  else
    c_bitmap_unset(conn->server_id_pool, id - 1);

  return 0;
}

struct c_wl_connection *c_wl_connection_init(int client_fd, struct c_wl_display *display) {
  struct c_wl_connection *conn = calloc(1, sizeof(struct c_wl_connection));
  if (!conn) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  conn->objects = c_map_new(512);
  conn->client_id_pool = c_bitmap_new(4096);
  conn->server_id_pool = c_bitmap_new(4096);
  conn->client_fd = client_fd;
  conn->dpy = display;

  c_wl_object_add(conn, 1, (struct c_wl_interface *)c_wl_interface_get("wl_display"), NULL);

  return conn;
}

struct c_wl_display *c_wl_connection_get_dpy(struct c_wl_connection *conn) {
  return conn->dpy;
}

struct c_map *c_wl_connection_get_objects(struct c_wl_connection *conn) {
  return conn->objects;
}

int c_wl_connection_free(struct c_wl_connection *conn) {
  c_wl_object_id id;
  struct c_wl_object *o;
  c_map_for_each(conn->objects, id, o)
    if (o->data) {
      c_log(C_LOG_DEBUG, "%-30s %d %p", o->iface->name, o->data->ref_count, o->data);
      if (--o->data->ref_count == 0) {
        SWITCH_STR(o->iface->name);
          CASE_STR("wl_buffer") {
            c_wl_display_notify(conn->dpy, o->data->data, C_WL_DISPLAY_ON_BUFFER_DESTROY);
            SWITCH_STR_BREAK;
          }

          CASE_STR("xdg_toplevel") {
            struct c_wl_surface *toplevel = o->data->data;
            free(toplevel->xdg.children);
            SWITCH_STR_BREAK;
          }

        SWITCH_STR_END

        free(o->data->data);
        free(o->data);
      }
  }
  c_map_destroy(conn->objects);

  c_bitmap_destroy(conn->client_id_pool);
  c_bitmap_destroy(conn->server_id_pool);
  free(conn);
  return 0;
}


int c_wl_error_set(c_wl_object_id object_id, c_wl_int code, c_wl_string msg, ...) {
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
  return __serial++;
}
