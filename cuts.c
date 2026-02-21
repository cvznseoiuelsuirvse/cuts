#define _GNU_SOURCE

#include <signal.h>
#include <stdlib.h>

#include "wayland/server.h"
#include "wayland/error.h"
#include "wayland/protocols/wayland.h"
#include "backend/drm.h"
#include "util/event_loop.h"

struct c_compositor {
  struct wl_display *display;
  struct c_drm_backend *backend;
  c_list 	*surfaces;
	int		 needs_redraw;
};

struct c_compositor *__comp = NULL;

static void release_surface(struct wl_surface *surface) {
  struct wl_connection *conn = surface->conn;
  wl_buffer_release(conn, surface->active->id);
  wl_connection_callback_done(conn, surface->id);

  surface->active = NULL;
  surface->x = 0;
  surface->y = 0;
  surface->width = 0;
  surface->height = 0;
}

static void draw_surfaces() {
  struct wl_surface *surface;
  c_list *_s = __comp->surfaces;
  c_list_for_each(_s, surface) {
    if (surface->active) {
      struct c_drm_connector *conn;
      c_drm_backend_for_each_connector(__comp->backend, conn) {
        c_drm_framebuffer_draw(conn->back, 0, 0, surface);
      }
      release_surface(surface);
    }
  }
}


void draw_rect(struct c_drm_dumb_framebuffer *framebuffer, 
                      uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
                      uint32_t color) {

  for (size_t _y = y; _y < (height + y); _y++) {
    for (size_t _x = x; _x < (width + x); _x++) {
        uint32_t pos = _y * framebuffer->width + _x;
        *((uint32_t *)(framebuffer->buffer) + pos) = color;
    }
  } 
}

static c_event_callback_errno backend_callback(struct c_event_loop *loop, int fd, void *data) {
  draw_surfaces();
  if (c_drm_backend_handle_event((struct c_drm_backend *)data) == -1) 
    return -C_EVENT_ERROR_FATAL;
  return C_EVENT_OK;
}


int wl_surface_commit(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id wl_surface_id = args[0].u;
  struct wl_object *wl_surface = wl_object_get(conn, wl_surface_id);

  struct wl_surface *surface = wl_surface->data;

  surface->active = surface->pending;
  surface->pending = NULL;

  c_drm_backend_page_flip(__comp->backend);
  return 0;
}

int wl_compositor_create_surface(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id new_id = args[1].n;
  struct wl_object *wl_surface;
  WL_CHECK_IF_NOT_REGISTERED(new_id, wl_surface);

  struct wl_surface surface = {0};
  surface.conn = conn;
  surface.id = new_id;
  void *data = c_list_push(__comp->surfaces, &surface, sizeof(struct wl_surface));

  wl_object_add(conn, new_id, wl_interface_get("wl_surface"), data);
  return 0;
}


static void cleanup(int err) {
  if (__comp->backend) c_drm_backend_free(__comp->backend);
  if (__comp->display) wl_display_free(__comp->display);
  exit(err);
}

int main() {
  signal(SIGTERM, cleanup);
  signal(SIGINT, cleanup);

  int ret = 0;

  struct c_compositor compositor = {0};
  __comp = &compositor;

  struct wl_display *display = wl_display_init();
  if (!display) 
    goto out;
  compositor.display = display;

  struct c_drm_backend *backend = c_drm_backend_init();
  if (!backend)
    goto out;
  compositor.backend = backend;

  compositor.surfaces = c_list_new();

  c_event_loop_add(display->loop, backend->fd, backend_callback, backend);


  // struct c_drm_connector *conn;
  // c_drm_backend_for_each_connector(compositor->backend, conn) {
  //   draw_rect(conn->back, 0, 0, conn->back->width, conn->back->height, 0xffff0000);
  //   draw_rect(conn->front, 0, 0, conn->back->width, conn->back->height, 0xffff0000);
  // }

  ret = c_event_loop_run(display->loop);

out:
  printf("asdgasdgasdg\n");
  cleanup(ret);
}
