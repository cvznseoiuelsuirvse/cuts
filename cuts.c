#define _GNU_SOURCE

#include <signal.h>
#include <stdlib.h>
#include <assert.h>

#include "wayland/server.h"
#include "wayland/types/wayland.h"
#include "wayland/types/xdg-shell.h"

#include "backend/drm.h"
#include "render/render.h"

#include "util/event_loop.h"
#include "util/log.h"

struct cuts_window {
  struct c_wl_surface *surface; // managed by wayland backend
  uint32_t width, height;
  uint32_t x, y;
};

struct cuts {
	int		               needs_redraw;
  c_list 	            *windows;
  struct c_wl_display *display;
  struct c_render     *render;
};

struct cuts *__comp = NULL;

#define CUTS_GL_COLOR 0.8f, 0.1f, 0.2f, 1.0f
#define clear_color()   \
  glClearColor(CUTS_GL_COLOR); \
  glClear(GL_COLOR_BUFFER_BIT)


void release_surface(struct c_wl_surface *surface) {
  struct c_wl_connection *conn = surface->conn;
  c_wl_connection_callback_done(conn, surface->id);
  wl_buffer_release(conn, surface->active->id);

  surface->pending = surface->active;
  surface->active = NULL;
}

static void draw_window(struct cuts_window *window) {
  printf("draw_window\n");
  struct c_rect rect = {
    .width = window->width,
    .height = window->height,
    .x = window->x,
    .y = window->y,
    .texture = window->surface->active->dma->dma->texture,
  };
  draw(__comp->render, &rect);
  release_surface(window->surface);
}

static void draw_windows() {
  printf("draw_windows\n");
  struct cuts_window *window;
  c_list *_w = __comp->windows;

  c_list_for_each(_w, window) {
    if (window->surface->active) {
      draw_window(window);
    }
  }

}

static void render_frame() {
  printf("render_frame\n");
  if (__comp->render->drm->connector->waiting_for_flip) {
    __comp->needs_redraw = 1;
    return;
  }

  clear_color();
  draw_windows();

  if (!c_render_new_page_flip(__comp->render)) 
    __comp->needs_redraw = 0;
  
}

static c_event_callback_errno render_callback(struct c_event_loop *loop, int fd, void *data) {
  if (c_render_handle_event(data) == -1) 
    return -C_EVENT_ERROR_FATAL;

  if (__comp->needs_redraw)
    render_frame();

  return C_EVENT_OK;
}

int on_surface_new_cb(struct c_wl_surface *surface) {
  struct cuts_window window = {.surface = surface};
  printf("%p\n", surface);
  c_list_push(__comp->windows, &window, sizeof(window));
  return 0;
}

int on_window_new_cb(struct c_wl_surface *surface, c_wl_object_id xdg_toplevel_id) {
  struct cuts_window *window = NULL;
  c_list *_w = __comp->windows;
  c_list_for_each(_w, window) {
    if (window->surface == surface) {
      uint32_t mon_width = __comp->render->drm->connector->mode->hdisplay;
      uint32_t mon_height = __comp->render->drm->connector->mode->vdisplay;
      uint32_t gap = 15;

      window->x = gap;
      window->y = gap;

      window->width = mon_width - gap * 2;
      window->height = mon_height - gap * 2;

      c_wl_array arr = {0, NULL};
      xdg_toplevel_configure(surface->conn, xdg_toplevel_id, window->width, window->height, &arr);
      return 0;
    }
  }
  return -1;
}

int on_window_update_cb(struct c_wl_surface *surface) {
  render_frame();
  return 0;
}


static void cleanup(int err) {
  if (__comp->render)  c_render_free(__comp->render);
  if (__comp->display) c_wl_display_free(__comp->display);


  struct cuts_window *window;
  c_list *_w = __comp->windows;
  c_list_for_each(_w, window) {
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

    free(window);
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
  
  struct c_wl_display *display = c_wl_display_init();
  if (!display) 
    goto out;

  display->callbacks.on_surface_new = on_surface_new_cb;
  display->callbacks.on_window_new = on_window_new_cb;
  display->callbacks.on_window_update = on_window_update_cb;
  compositor.display = display;
  compositor.windows = c_list_new();

  struct c_drm_backend *drm = c_drm_backend_init();
  if (!drm)
    goto out;

  struct c_render *render = c_render_init(drm);
  if (!render)
    goto out;
  __comp->render = render;


  clear_color();
  c_render_new_page_flip(render);

  c_event_loop_add(display->loop, drm->fd, render_callback, render);

  ret = c_event_loop_run(display->loop);

out:
  cleanup(ret);
}
