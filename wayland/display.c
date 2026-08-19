#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "wayland/display.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/log.h"

#define MAX_INTERFACES 2048
static const struct c_wl_interface *__interfaces[MAX_INTERFACES];
static size_t                 __ninterfaces = 0;

enum connection_event_notifiers {
  C_WL_DISPLAY_ON_CONNECTION_NEW,
  C_WL_DISPLAY_ON_CONNECTION_GONE,
};

struct __interface_listeners {
  char iface_name[100];
  int destructor;
  void *listeners;
  void *userdata;
};

struct c_wl_display {
	char 	 socket_path[108];

	struct c_wl_interface *interfaces;
	c_list *interface_listeners;

  c_list *connections;
	struct c_wl_display_connection_listener *connection_listener;
	void *connection_listener_data;
};

static int create_socket(struct c_wl_display *display) {
  int fd;
  const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (!xdg_runtime_dir)
    xdg_runtime_dir = "/tmp";

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    c_log_errno(C_LOG_ERROR, "socket failed");
    return -1;
  }

  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;

  for(size_t i = 0; i < 1000; i++) {
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/wayland-%ld", xdg_runtime_dir, i);
    if (access(addr.sun_path, F_OK) == -1) {
      char env_value[16];
      snprintf(env_value, sizeof(env_value), "wayland-%ld", i);

      if (setenv("WAYLAND_DISPLAY", env_value, 1) < 0) {
        c_log_errno(C_LOG_ERROR, "failed to set WAYLAND_DISPLAY env");
        goto error;
      }
      break;
    }
    addr.sun_path[0] = 0;
    
  }

  if (!*addr.sun_path) {
    c_log(C_LOG_ERROR, "all sockets are taken\n");
    goto error;
  }

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    c_log_errno(C_LOG_ERROR, "bind failed");
    goto error;
  }

  if (listen(fd, 16) == -1) {
    c_log_errno(C_LOG_ERROR, "listen failed");
    goto error;
  }

  snprintf(display->socket_path, sizeof(display->socket_path), "%s", addr.sun_path);
  c_log(C_LOG_INFO, "created wayland socket at %s", display->socket_path);
  return fd;

error:
  unsetenv("WAYLAND_DISPLAY");
  close(fd);
  return -1;
}

inline void c_wl_interface_add(const struct c_wl_interface *interface) {
  assert(__ninterfaces < MAX_INTERFACES);
  __interfaces[__ninterfaces++] = interface; 
}

inline const struct c_wl_interface *c_wl_interface_get(const char *interface_name) {
  for (size_t i = 0; i < __ninterfaces; i++) {
    const struct c_wl_interface *interface = __interfaces[i];
    if (STREQ(interface_name, interface->name)) return interface;
  }
  return NULL;
}

void c_wl_interface_support(const char *name, c_wl_interface_on_bind on_bind,
                            void *userdata) {
  struct c_wl_interface *iface = (struct c_wl_interface *)c_wl_interface_get(name);
  assert(iface);
  iface->on_bind = on_bind;
  iface->on_bind_userdata = userdata;
  iface->is_supported = 1;
}

size_t c_wl_interface_get_all(const struct c_wl_interface ***interfaces) {
  *interfaces = __interfaces;
  return __ninterfaces;
};

void c_wl_display_add_interface_listener(struct c_wl_display *display,
                                         const char *iface, void *listeners,
                                         void *userdata) {
  struct __interface_listeners l;
  snprintf(l.iface_name, sizeof(l.iface_name), "%s", iface);
  l.listeners = listeners;
  l.userdata = userdata;
      
  c_list_push(display->interface_listeners, &l, sizeof(l));
}

void c_wl_display_get_interface_listener(struct c_wl_display *display, const char *iface,
                               void ***handlers, void **userdata) {
  *handlers = NULL;
  *userdata = NULL;

  struct __interface_listeners *l;
  c_list_for_each(display->interface_listeners, l) {
    if (STREQ(l->iface_name, iface)) {
      *handlers = l->listeners;
      *userdata = l->userdata;
      break;
    }
  }
}


void c_wl_display_add_connection_listener(
    struct c_wl_display *display,
    struct c_wl_display_connection_listener *listener, void *userdata) {

  display->connection_listener = listener;
  display->connection_listener_data = userdata;
}

void connection_event_notify(struct c_wl_display *display, struct c_wl_connection *conn,
                             enum connection_event_notifiers notifier) {
#define notify(callback)                                                       \
  if (display->connection_listener->callback) {                                \
    display->connection_listener->callback(conn,                               \
                                           display->connection_listener_data); \
  }

  switch (notifier) {
    case C_WL_DISPLAY_ON_CONNECTION_NEW:  notify(new); break;
    case C_WL_DISPLAY_ON_CONNECTION_GONE: notify(gone); break;
    default: break;
  }
}

C_EVENT_CALLBACK client_epoll_callback(struct c_event_loop *loop, int fd, void *data) {
  struct c_wl_connection *connection = data;
  struct c_wl_display *display = c_wl_connection_get_display(connection);
  int ret;

  ret = c_wl_connection_dispatch(connection);
  if (ret) goto out;

  ret = c_wl_connection_flush(connection);
  if (ret == -1) {
    ret = DISPATCH_CLIENT_ERR;
  }

out:
  switch (ret) {
    case DISPATCH_FATAL_ERR:
      connection_event_notify(display, connection, C_WL_DISPLAY_ON_CONNECTION_GONE);
      c_list_remove(&display->connections, connection);
      c_wl_connection_free(connection);
      return C_EVENT_ERROR_FATAL;

    case DISPATCH_PROTO_ERR:
      c_wl_error_send(connection);
      return C_EVENT_OK;

    case DISPATCH_CLIENT_ERR:
      connection_event_notify(display, connection, C_WL_DISPLAY_ON_CONNECTION_GONE);
      c_list_remove(&display->connections, connection);
      c_wl_connection_free(connection);
      return C_EVENT_ERROR_FD_GONE;

    default:
      return C_EVENT_OK;
  }
}

C_EVENT_CALLBACK server_epoll_callback(struct c_event_loop *loop, int fd, void *data) {
  struct c_wl_display *display = data;
  int client_fd = accept(fd, NULL, NULL);

  if (set_nonblocking(client_fd) == -1) {
    c_log(C_LOG_WARNING, "failed to set client fd to non-blocking");
  }
  
  struct c_wl_connection *connection = c_wl_connection_init(client_fd, display);
  if (!connection) {
    c_log(C_LOG_ERROR, "c_wl_connection_init failed");
    return C_EVENT_ERROR_FATAL;
  }
  

  if (c_event_loop_add(loop, client_fd, client_epoll_callback, connection) == -1) {
    c_log(C_LOG_ERROR, "c_event_loop_add failed");
    return C_EVENT_ERROR_FATAL;
  }

  c_list_push(display->connections, connection, 0);
  connection_event_notify(display, connection, C_WL_DISPLAY_ON_CONNECTION_NEW);

  return C_EVENT_OK;
}

struct c_wl_display *c_wl_display_init(struct c_event_loop *loop) {
  struct c_wl_display *display = calloc(1, sizeof(*display));
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

  c_event_loop_add(loop, fd, server_epoll_callback, display);

  display->interface_listeners = c_list_new();
  display->connections = c_list_new();

  c_wl_interface_support("wl_compositor", NULL, NULL);
  c_wl_interface_support("wl_subcompositor", NULL, NULL);
  c_wl_interface_support("xdg_wm_base", NULL, NULL);
  c_wl_interface_support("wp_viewporter", NULL, NULL);

  return display;

}

void c_wl_display_free(struct c_wl_display *display) {
  if (*display->socket_path) unlink(display->socket_path);

  if (display->connections) {
    struct c_wl_connection *conn;
    c_list_for_each(display->connections, conn)
      c_wl_connection_free(conn);
    c_list_destroy(display->connections);
  }

  if (display->interface_listeners)
    c_list_destroy(display->interface_listeners);

  unsetenv("WAYLAND_DISPLAY");
  free(display);
}


