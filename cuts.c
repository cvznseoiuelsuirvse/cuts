#include <signal.h>
#include <stdlib.h>
#include <assert.h>

#include "wayland/display.h"

#include "backend/backend.h"

#include "render/render.h"

#include "util/event_loop.h"
#include "util/log.h"

struct window {
  uint32_t width, height;
  uint32_t x, y;
};

struct cuts {
  struct c_wl_display *display;
  struct c_backend    *backend;
  struct c_render     *render;

  size_t               n_windows;
  c_list 	            *windows;
};

struct cuts *__comp = NULL;


static int on_window_new_cb(struct c_window *window, void *userdata) {
  struct c_render *render = __comp->render;
  uint32_t mon_width = render->drm->output->width;
  uint32_t mon_height = render->drm->output->height;
  uint32_t gap = 15;

  __comp->n_windows += 1;
  c_list_push(__comp->windows, window, 0);

  mon_width -= (__comp->n_windows + 1) * gap;
  uint32_t one_window_width = mon_width / __comp->n_windows;

  int i = 0;
  struct c_window *_w;
  c_list_for_each(__comp->windows, _w) {
    _w->x = i++ * one_window_width;
    _w->x += i * gap;
    _w->y = gap;

    _w->width = one_window_width;
    _w->height = mon_height - gap * 2;
    c_render_window_resize(render, _w);
  }

  return 0;
}



static void cleanup(int err) {
  c_log(C_LOG_DEBUG, "running cleanup");

  if (__comp->render)  c_render_free(__comp->render);
  if (__comp->display) c_wl_display_free(__comp->display);
  if (__comp->backend) c_backend_free(__comp->backend);

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

  __comp->backend = backend;

  struct c_render *render = c_render_init(display, backend->drm);
  if (!render) goto out;

  struct c_render_listener render_listener = {
    .on_window_new = on_window_new_cb,
  };
  c_render_add_listener(render, &render_listener, NULL);

  __comp->render = render;

  ret = c_event_loop_run(display->loop);

out:
  cleanup(ret);
}
