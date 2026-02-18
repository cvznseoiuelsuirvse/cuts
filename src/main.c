#define _GNU_SOURCE
#include "server/server.h"
#include "backend/drm.h"

#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>

struct {
  struct c_event_loop *loop;
  struct c_event_resource serv;
  struct c_drm_backend *backend;
} state;


static void cleanup(int err) {
  if (state.serv.fd > 0) c_unlink_socket(&state.serv);
  if (state.loop) c_event_loop_free(state.loop);
  if (state.backend) c_drm_backend_free(state.backend);
  exit(err);
}



int main() {
  signal(SIGTERM, cleanup);
  signal(SIGINT, cleanup);

  int ret = 0;
  memset(&state, 0, sizeof(state));

  state.loop = c_event_loop_init();
  if (!state.loop) {
    fprintf(stderr, "failed to event_loop_init\n");
    return 1;
  }

  state.backend = c_drm_backend_init();
  if (!state.backend) {
    fprintf(stderr, "failed to init drm backend\n");
    goto out;
  }


  struct c_event_resource backend_resource;
  backend_resource.fd = state.backend->fd;
  backend_resource.data = state.backend;
  backend_resource.type = DRM;

  c_event_loop_add(state.loop, &backend_resource);

  // for (size_t i = 0; i < state.backend->connectors_n; i++) {
  //   struct c_drm_connector conn = state.backend->connectors[i];
  //   uint32_t width = conn.front->width;
  //   uint32_t height = conn.front->height;
  //
  //   draw_rect(conn.front, 0, 0, width, height, 0xffff0000);
  // }
  //
  if (c_create_socket(&state.serv) == -1) { 
    fprintf(stderr, "failed to create socket\n");
    goto out;
  }

  c_event_loop_add(state.loop, &state.serv);
  ret = c_event_loop_run(state.loop);

out:
  cleanup(ret);
}
