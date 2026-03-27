#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "wayland/display.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/log.h"

struct __display_event_listener {
  void *userdata;
  struct c_wl_display_listener *listener;
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

C_EVENT_CALLBACK client_epoll_callback(struct c_event_loop *loop, int fd, void *data) {
  struct c_wl_connection *connection = data;
  int ret = c_wl_connection_dispatch(connection);
  if (ret == 1) {
    c_log(C_LOG_DEBUG, "client %d gone (conn %p)", connection->client_fd, connection);
    c_wl_display_notify(connection->dpy, connection, C_WL_DISPLAY_ON_CLIENT_GONE);
    c_wl_connection_free(connection);
    ret = C_EVENT_ERROR_FD_GONE;
  } else if (ret == -1) {
    c_wl_error_send(connection);
    ret = C_EVENT_ERROR_WL_PROTO;
  }

  return ret;

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

  c_wl_display_notify(display, connection, C_WL_DISPLAY_ON_CLIENT_NEW);

  return C_EVENT_OK;
}

void c_wl_display_add_listener(struct c_wl_display *display, struct c_wl_display_listener *listener, void *userdata) {
  struct __display_event_listener l = {
    .userdata = userdata,
    .listener = calloc(1, sizeof(*listener)),
  };
  memcpy(l.listener, listener, sizeof(*listener));
  c_list_push(display->listeners, &l, sizeof(l)); 
}

void c_wl_display_notify(struct c_wl_display *display, void *data, enum c_wl_display_notifer notifier) {
  struct __display_event_listener *l;

  #define notify(callback) \
    c_list_for_each(display->listeners, l) { \
      if (l->listener->callback) {\
        c_log(C_LOG_DEBUG, #callback " %p", data); \
        l->listener->callback(data, l->userdata); \
      } \
    }

  switch (notifier) {
    case C_WL_DISPLAY_ON_CLIENT_NEW:      notify(on_client_new); break;
    case C_WL_DISPLAY_ON_CLIENT_GONE:     notify(on_client_gone); break;

    case C_WL_DISPLAY_ON_SURFACE_NEW:     notify(on_surface_new); break;
    case C_WL_DISPLAY_ON_SURFACE_UPDATE:  notify(on_surface_update); break;
    case C_WL_DISPLAY_ON_SURFACE_DESTROY: notify(on_surface_destroy); break;
    case C_WL_DISPLAY_ON_TOPLEVEL_NEW:    notify(on_toplevel_new); break;
    default: break;
  }

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

  display->listeners = c_list_new();
  display->supported_ifaces = c_list_new();

  c_wl_display_add_supported_interface(display, "wl_compositor", NULL, NULL);
  c_wl_display_add_supported_interface(display, "wl_subcompositor", NULL, NULL);
  c_wl_display_add_supported_interface(display, "xdg_wm_base", NULL, NULL);
  c_wl_display_add_supported_interface(display, "zxdg_decoration_manager_v1", NULL, NULL);

  return display;

}

void c_wl_display_add_supported_interface(struct c_wl_display *display, const char *name, c_wl_display_on_bind on_bind, void *userdata) {
  struct c_wl_interface *iface = c_wl_interface_get(name);
  assert(iface);
  struct c_wl_display_supported_iface i = {
    .iface = iface,
    .on_bind = on_bind,
    .userdata = userdata,
  };
  c_list_push(display->supported_ifaces, &i, sizeof(i));
}

void c_wl_display_free(struct c_wl_display *display) {
  if (display->loop) c_event_loop_free(display->loop);
  if (*display->socket_path) unlink(display->socket_path);
  if (display->listeners) {
    struct __display_event_listener *l;
    c_list_for_each(display->listeners, l)
      free(l->listener);
    c_list_destroy(display->listeners);
  }

  if (display->supported_ifaces)
    c_list_destroy(display->supported_ifaces);

  unsetenv("WAYLAND_DISPLAY");
  
  free(display);
}


