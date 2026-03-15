#define _GNU_SOURCE

#include <signal.h>
#include <stdlib.h>
#include <assert.h>

#include "wayland/display.h"

#include "backend/backend.h"
#include "backend/input.h"

#include "render/render.h"

#include "util/event_loop.h"
#include "util/log.h"

struct cuts {
	int		               needs_redraw;
  c_list 	            *windows;
  struct c_wl_display *display;
  struct c_backend    *backend;
  struct c_render     *render;
};

struct cuts *__comp = NULL;

static int on_mouse_movement_cb(struct c_input_mouse_event *event, void *userdata) {
  c_log(C_LOG_DEBUG, "mouse movement x=%d y=%d", event->x, event->y);
  return 0;
}

static void cleanup(int err) {
  c_log(C_LOG_DEBUG, "running cleanup");

  if (__comp->render)  c_render_free(__comp->render);
  if (__comp->display) c_wl_display_free(__comp->display);
  if (__comp->backend) c_backend_free(__comp->backend);

  struct c_render_window *window;
  c_list_for_each(__comp->windows, window) {
    struct c_wl_surface *surface = window->surface;
    struct c_wl_buffer *wl_buffer;
    if (surface->active)
      wl_buffer = surface->active;
    else if (surface->pending)
      wl_buffer = surface->pending;
    else
      goto free_surface;
  
    if (wl_buffer->type == C_WL_BUFFER_DMA) {
      free(wl_buffer->dma->dma); 
      free(wl_buffer->dma); 
    }

    free(wl_buffer);
   
  free_surface:
    free(surface);
  }

  c_list_destroy(__comp->windows);
  exit(err);
}

int main() {
  signal(SIGTERM, cleanup);
  signal(SIGINT, cleanup);

  int ret = 0;

  struct cuts compositor = {0};
  __comp = &compositor;
  
  c_log_set_level(C_LOG_INFO | C_LOG_ERROR | C_LOG_DEBUG);

  struct c_wl_display *display = c_wl_display_init();
  if (!display) 
    goto out;

  compositor.display = display;
  compositor.windows = c_list_new();

  struct c_backend *backend = c_backend_init(display->loop);
  if (!backend) goto out;

  struct c_input_listener_mouse input_listener_mouse = {
    .on_mouse_movement = on_mouse_movement_cb,
  };
  c_input_add_listener_mouse(backend->input, &input_listener_mouse, NULL);

  __comp->backend = backend;

  struct c_render *render = c_render_init(display, backend->drm);
  if (!render)
    goto out;
  __comp->render = render;

  ret = c_event_loop_run(display->loop);

out:
  cleanup(ret);
}
