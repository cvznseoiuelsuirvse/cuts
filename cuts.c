#define _GNU_SOURCE

#include <signal.h>
#include <stdlib.h>
#include <GLES2/gl2.h>

#include "wayland/server.h"
#include "wayland/error.h"
#include "wayland/types/wayland.h"

#include "backend/drm.h"
#include "render/render.h"

#include "util/event_loop.h"
#include "util/log.h"

struct c_compositor {
	int		               needs_redraw;
  c_list 	            *surfaces;
  struct c_wl_display *display;
  struct c_render     *render;
};

struct c_compositor *__comp = NULL;

void release_surface(struct c_wl_surface *surface) {
  struct c_wl_connection *conn = surface->conn;
  wl_buffer_release(conn, surface->active->id);
  c_wl_connection_callback_done(conn, surface->active->id);

  surface->active = NULL;
  surface->damage.x = 0;
  surface->damage.y = 0;
  surface->damage.width = 0;
  surface->damage.height = 0;
  surface->damage.damaged = 0;
}

static void draw_surfaces() {
  struct c_wl_surface *surface;
  c_list *_s = __comp->surfaces;
  c_list_for_each(_s, surface) {
    if (surface->active) {
      release_surface(surface);
    }
  }
}

static c_event_callback_errno render_callback(struct c_event_loop *loop, int fd, void *data) {
  if (c_render_handle_event(data) == -1) 
    return -C_EVENT_ERROR_FATAL;

  return C_EVENT_OK;
}


int wl_surface_commit(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id c_wl_surface_id = args[0].u;
  struct c_wl_object *c_wl_surface = c_wl_object_get(conn, c_wl_surface_id);

  struct c_wl_surface *surface = c_wl_surface->data;

  surface->active = surface->pending;
  surface->pending = NULL;

  c_render_new_page_flip(__comp->render);
  return 0;
}

int wl_compositor_create_surface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id wl_surface_id = args[1].o;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_surface_id, wl_surface);

  struct c_wl_surface *c_wl_surface = calloc(1, sizeof(struct c_wl_surface));
  if (!c_wl_surface) {
    c_log(C_LOG_ERROR, "calloc failed");
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  c_wl_surface->id = wl_surface_id;
  c_wl_surface->conn = conn;
  void *data = c_list_push(__comp->surfaces, c_wl_surface, 0);

  c_wl_object_add(conn, wl_surface_id, c_wl_interface_get("wl_surface"), data);
  return 0;
}


static void cleanup(int err) {
  if (__comp->render)  c_render_free(__comp->render);
  if (__comp->display) c_wl_display_free(__comp->display);
  exit(err);
}

int main() {
  signal(SIGTERM, cleanup);
  signal(SIGINT, cleanup);

  int ret = 0;

  struct c_compositor compositor = {0};
  __comp = &compositor;
  
  struct c_wl_display *display = c_wl_display_init();
  if (!display) 
    goto out;
  compositor.display = display;
  compositor.surfaces = c_list_new();

  struct c_drm_backend *drm = c_drm_backend_init();
  if (!drm)
    goto out;

  struct c_render *render = c_render_init(drm);
  if (!render)
    goto out;
  __comp->render = render;

  // glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  // glClear(GL_COLOR_BUFFER_BIT);
  // c_render_new_page_flip(render);

  c_event_loop_add(display->loop, drm->fd, render_callback, render);

  ret = c_event_loop_run(display->loop);

out:
  cleanup(ret);
}
