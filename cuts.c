#define _GNU_SOURCE

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "compositor/compositor.h"
#include "backend/drm.h"

struct {
  struct c_compositor *compositor;
} state;


static void cleanup(int err) {
  if (state.compositor) c_compositor_free(state.compositor);
  exit(err);
}

static void draw_rect(struct c_drm_dumb_framebuffer *framebuffer, 
                      uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
                      uint32_t color) {

  for (size_t _y = y; _y < (height + y); _y++) {
    for (size_t _x = x; _x < (width + x); _x++) {
        uint32_t pos = _y * framebuffer->width + _x;
        *((uint32_t *)(framebuffer->buffer) + pos) = color;
    }
  } 
}


int main() {
  signal(SIGTERM, cleanup);
  signal(SIGINT, cleanup);

  int ret = 0;
  memset(&state, 0, sizeof(state));

  struct c_compositor *compositor = c_compositor_init();
  if (!compositor) { 
    fprintf(stderr, "failed to init c_compositor\n");
    goto out;
  }
  state.compositor = compositor;

  // struct c_drm_connector *conn;
  // c_drm_backend_for_each_connector(compositor->backend, conn) {
  //   draw_rect(conn->back, 0, 0, conn->back->width, conn->back->height, 0xffff0000);
  //   draw_rect(conn->front, 0, 0, conn->back->width, conn->back->height, 0xffff0000);
  // }

  ret = c_compositor_run_loop(compositor);

out:
  cleanup(ret);
}
