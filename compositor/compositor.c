#define _GNU_SOURCE
#include <sys/un.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "../backend/drm.h"
#include "proto/wayland.h"
#include "../util/list.h"
#include "server.h"
#include "error.h"
#include "compositor.h"

#define CUTS_MAX_EPOLL_EVENTS 512

enum c_event_resource_type {
	SERVER,
	CLIENT,
	DRM,
};

struct c_event_resource {
	int fd;
	void *data;
	enum c_event_resource_type type;
};

struct c_event_loop {
	int 		 epfd;
	c_list	*resources;
	c_list	*connections;
	struct epoll_event events[CUTS_MAX_EPOLL_EVENTS];
};

static char __socket_path[128] = {0};

static inline int set_nonblocking(int fd) {
  int flags;

  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return flags;

  flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}

static inline void unlink_socket() {
  if (*__socket_path) unlink(__socket_path); 
}

static int create_socket() {
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

  snprintf(__socket_path, sizeof(__socket_path), "%s", addr.sun_path);
  return fd;
}

static struct c_event_loop * c_event_loop_init() {
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

static int c_event_loop_add(struct c_event_loop *loop, int fd, enum c_event_resource_type type, void *data) {
  struct c_event_resource resource = {fd, data, type};
  struct c_event_resource *resource_cpy = c_list_push(loop->resources, &resource, sizeof(struct c_event_resource));

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
  ev.data.ptr = resource_cpy;
  if (epoll_ctl(loop->epfd, EPOLL_CTL_ADD, resource.fd, &ev) == -1) {
    fprintf(stderr, "failed to add event %d\n", resource.fd);
    return -1;
  }
  return 0;
}

static int c_event_loop_del(struct c_event_loop *loop, struct c_event_resource *resource) {
  if (epoll_ctl(loop->epfd, EPOLL_CTL_DEL, resource->fd, NULL) == -1) {
    fprintf(stderr, "failed to del event %d\n", resource->fd);
    return -1;
  }
  close(resource->fd);
  c_list_remove_ptr(&loop->resources, resource);
  return 0;
}


static void c_event_loop_free(struct c_event_loop *loop) {
  struct c_event_resource *resource;
  struct c_list *l = loop->resources;
  c_list_for_each(l, resource) close(resource->fd);
  
  c_list_destroy(loop->resources);
  c_list_destroy(loop->connections);
  free(loop);
}

void c_compositor_free(struct c_compositor *compositor) {
  if (compositor->backend) c_drm_backend_free(compositor->backend);
  if (compositor->loop) c_event_loop_free(compositor->loop);
  unlink_socket();
  c_list_destroy(compositor->surfaces);
  free(compositor);
}

struct c_compositor *c_compositor_init() {
  struct c_compositor *compositor = calloc(1, sizeof(struct c_compositor));
  if (!compositor) return NULL;
  
  compositor->loop = c_event_loop_init();
  if (!compositor->loop) {
    free(compositor);
    return NULL;
  }

  int socket_fd = create_socket();
  if (socket_fd == -1) {
    c_compositor_free(compositor);
    return NULL;
  }

  c_event_loop_add(compositor->loop, socket_fd, SERVER, NULL);


  struct c_drm_backend *backend = c_drm_backend_init();
  if (!backend) {
    c_compositor_free(compositor);
    return NULL;
  }

  compositor->backend = backend;
  c_event_loop_add(compositor->loop, backend->fd, DRM, backend);

  compositor->surfaces = c_list_new();

  return compositor;
}

static void release_surface(struct c_surface *surface) {
  struct wl_connection *conn = surface->conn;
  wl_buffer_release(conn, surface->active->id);
  wl_object_id *wl_callback_id = c_map_get(conn->callback_queue, surface->id);

  if (wl_callback_id) {
    wl_callback_done(conn , *wl_callback_id, WL_SERIAL);
    wl_display_delete_id(conn, 1, *wl_callback_id);
  }
  
  surface->active = NULL;
  surface->x = 0;
  surface->y = 0;
  surface->width = 0;
  surface->height = 0;
}

static void draw_surfaces(struct c_compositor *compositor) {
  struct c_surface *surface;
  c_list *_s = compositor->surfaces;
  c_list_for_each(_s, surface) {
    if (surface->active) {
      struct c_drm_connector *conn;
      c_drm_backend_for_each_connector(compositor->backend, conn) {
        c_drm_framebuffer_draw(conn->back, 0, 0, surface);
      }
      release_surface(surface);
    }
  }
}

inline void c_compositor_update(struct c_compositor *compositor) {
  while (c_drm_backend_page_flip(compositor->backend) == -1) usleep(100);
}

int c_compositor_run_loop(struct c_compositor *compositor) {
  if (!compositor->loop) { 
    fprintf(stderr, "compositor's loop is NULL\n");
    return -1; 
  }

  int ret = 0;
  int n;
  struct c_event_loop *loop = compositor->loop;
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
          if ((ret = set_nonblocking(client_fd)) == -1)                      goto out;
          if ((ret = c_event_loop_add(loop, client_fd, CLIENT, NULL)) == -1) goto out;
        
      } else if ((events[i].events & EPOLLIN) && resource->type == CLIENT) {
        struct wl_connection *conn = resource->data;
        if (!conn) {
          conn = wl_connection_init(resource->fd);
          if (!conn) {
            fprintf(stderr, "failed to wl_connection_init on %d\n", resource->fd);
            continue;
          }
          conn->compositor = compositor;
          resource->data = conn;
        }

        ret = wl_connection_dispatch(conn);
        if (ret == -2) {
          wl_connection_free(conn);
          c_event_loop_del(loop, resource);
          
        } else if (ret == -1)
          c_error_send(conn);
        
      } else if ((events[i].events & EPOLLIN) && resource->type == DRM) {
        struct c_drm_backend *backend = resource->data;
        c_drm_backend_handle_event(backend);

      } else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
        c_event_loop_del(loop, resource);
      }

    }

    draw_surfaces(compositor);
    c_drm_backend_page_flip2(compositor->backend, &compositor->needs_redraw);

  }

out:
  return ret;
}

