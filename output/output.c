#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <unistd.h>
#include <fcntl.h>

#include "wayland/impl/wayland.h"

#include "output/output.h"
#include "output/cursor.h"
#include "output/drm/drm.h"
#include "output/drm/syncobj.h"

#include "render/framebuffer.h"
#include "render/gl/egl.h"

#include "util/log.h"
#include "util/event_loop.h"

static int open_gpu(struct c_session *session) {
  char gpu_path[128];
  int gpu_found = 0;
  for (int i = 0; i < 10; i++) {
    memset(gpu_path, 0, sizeof(gpu_path));
    snprintf(gpu_path, sizeof(gpu_path), "/dev/dri/card%d", i);
    int fd = open(gpu_path, O_RDWR | O_CLOEXEC);
    if (fd > 0) {
      gpu_found = 1;
      close(fd);
      break;
    }
  }

  if (!gpu_found) {
    c_log(C_LOG_ERROR, "no GPUs found");
    goto error;
  }

  struct c_session_device *dev = c_session_device_open(session, gpu_path);
  if (!dev) {
    c_log(C_LOG_ERROR, "failed to open GPU device");
    goto error;
  }


  if (drmSetClientCap(dev->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
    c_log_errno(C_LOG_ERROR, "failed to set DRM_CLIENT_CAP_UNIVERSAL_PLANES cap");
    goto error_set_cap;
  }

  if (drmSetClientCap(dev->fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
    c_log_errno(C_LOG_ERROR, "failed to set DRM_CLIENT_CAP_ATOMIC cap");
    goto error_set_cap;
  }

  return dev->fd;

error_set_cap:
  c_session_device_close(session, dev);

error:
  return -1;
}


static int create_swapchain(struct c_output *output, struct c_output_manager *mgr) {
  struct c_output_mode *mode = output->current_mode;
  uint32_t width = mode->width;
  uint32_t height = mode->height;

  output->swapchain.buffers[0] = c_framebuffer_create(mgr->renderer, mgr->drm_fd, mgr->gbm_device, width, height);
  if (!(output->swapchain.buffers[0])) {
    c_log(C_LOG_ERROR, "failed to create swapchain buffer[0]");
    return -1;
  }

  output->swapchain.buffers[1] = c_framebuffer_create(mgr->renderer, mgr->drm_fd, mgr->gbm_device, width, height);
  if (!(output->swapchain.buffers[1])) {
    c_log(C_LOG_ERROR, "failed to create swapchain buffer[1]");
    return -1;
  }

  output->swapchain.front = 0;

  return 0;
}


static void destroy_swapchain(struct c_output *output) {
  if (output->swapchain.buffers[0])
    c_framebuffer_destroy(output->swapchain.buffers[0]);

  if (output->swapchain.buffers[1])
    c_framebuffer_destroy(output->swapchain.buffers[1]);
}

static void page_flip_handler(int fd, unsigned int sequence,
                              unsigned int tv_sec, unsigned int tv_usec,
                              unsigned int crtc_id, void *userdata) {

  struct c_output *output = userdata;

  output->swapchain.front ^= 1;
  output->waiting_for_flip = 0;

  struct c_wl_object *wl_surface;
  struct c_wl_surface *surface;

  c_list_for_each(output->active_surfaces, wl_surface) {
    if ((surface = wl_surface->data)) {
      for (size_t i = 0; i < surface->frames_n; i++) {
        c_wl_connection_callback_done(wl_surface->conn, surface->frames[i]);
      }

      if (surface->frames_n)
        c_wl_connection_flush(wl_surface->conn);

      surface->frames_n = 0;
    }
  }
}

static int handle_drm_event(struct c_output_manager *mgr) {
  drmEventContext ctx = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler2 = page_flip_handler,
  };

  if (drmHandleEvent(mgr->drm_fd, &ctx) < 0) {
    c_log_errno(C_LOG_ERROR, "drmHandleEvent failed");
    return -1;
  }
  return 0;
}

static int schedule_pageflip(struct c_output_manager *mgr, struct c_output *output) {
  int ret = 0;
  if (output->need_redraw && !output->waiting_for_flip) {
    if (mgr->on_redraw(mgr, output, mgr->on_redraw_userdata)) return -1;

    int fence_fd;
    if (mgr->renderer->egl->ext_support.KHR_fence_sync && output->timeline) {
      fence_fd = c_drm_export_sync_file(output->timeline); 
      if (fence_fd == -1) {
        c_log(C_LOG_ERROR, "failed to export sync file");
        ret = -1;
        goto out;
      }

    } else {
      fence_fd = -1;
    }

    if (c_drm_atomic_commit(mgr->drm_fd, output, output->current_mode,
                            DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT,
                            output, fence_fd)) {
      c_log(C_LOG_ERROR, "failed to atomic commit");
      ret = -1;
    }

    if (fence_fd > -1) close(fence_fd);
    output->waiting_for_flip = 1;
  }

out:
  return ret;
}

C_EVENT_CALLBACK drm_callback(struct c_event_loop *loop, int fd, void *userdata) {
  struct c_output_manager *mgr = userdata;
  if (handle_drm_event(mgr) == -1) 
    return C_EVENT_ERROR_FATAL;

  struct c_output *output;
  c_list_for_each(mgr->outputs, output)
    if (schedule_pageflip(mgr, output)) return C_EVENT_ERROR_FATAL;

  return C_EVENT_OK;
}

int c_output_damage(struct c_output_manager *mgr, struct c_output *output) {
  output->need_redraw = 1;
  return schedule_pageflip(mgr, output);
}

void c_output_set_mode(struct c_output_manager *mgr, struct c_output *output, struct c_output_mode *mode) {
  while (output->waiting_for_flip)
    handle_drm_event(mgr);

  destroy_swapchain(output);

  output->current_mode = mode;
  create_swapchain(output, mgr);

  c_drm_atomic_commit(mgr->drm_fd, output, mode, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL, -1);

  glViewport(0, 0, mode->width, mode->height);
  c_output_damage(mgr, output);
}

void c_output_register_on_redraw(struct c_output_manager *mgr, on_redraw_handler handler, void *userdata) {
  mgr->on_redraw = handler;
  mgr->on_redraw_userdata = userdata;
}

void c_output_manager_free(struct c_output_manager *mgr) {
  if (mgr->outputs) {
    struct c_output *output;
    c_list_for_each(mgr->outputs, output) {
      c_cursor_free(output->cursor);
      c_list_destroy(output->active_surfaces);
      destroy_swapchain(output);
      c_drm_free_output(mgr->drm_fd, output);
    }

    c_list_destroy(mgr->outputs);
  }

  if (mgr->renderer)
    c_renderer_free(mgr->renderer);

  if (mgr->gbm_device)
    gbm_device_destroy(mgr->gbm_device);

  free(mgr);
}

struct c_output_manager *c_output_manager_init(struct c_session *session,
                                               struct c_event_loop *loop,
                                               struct c_wl_display *display) {
  struct c_output_manager *mgr = calloc(1, sizeof(*mgr));
  if (!mgr) {
    c_log_errno(C_LOG_ERROR, "failed to allocate c_output_manager");
    return NULL;
  }

  int drm_fd = open_gpu(session);
  if (drm_fd == -1) {
    c_log(C_LOG_ERROR, "failed to open a GPU");
    goto error;
  }

  mgr->outputs = c_drm_get_outputs(drm_fd);
  if (!mgr->outputs) {
    c_log(C_LOG_ERROR, "failed to get outputs");
    goto error;
  }

  mgr->drm_fd = drm_fd;
  mgr->gbm_device = gbm_create_device(drm_fd);
  if (!mgr->gbm_device) {
    c_log_errno(C_LOG_ERROR, "failed to create gbm device");
    goto error;
  }

  mgr->renderer = c_renderer_init(mgr, display);
  if (!mgr->renderer) {
    c_log(C_LOG_ERROR, "failed to initialize renderer");
    goto error;
  }

  uint64_t w, h;
  if (drmGetCap(mgr->drm_fd, DRM_CAP_CURSOR_WIDTH, &w) != 0) w = 64;
  if (drmGetCap(mgr->drm_fd, DRM_CAP_CURSOR_HEIGHT, &h) != 0) h = 64;

  struct c_output *output;
  c_list_for_each(mgr->outputs, output) {
    output->cursor = c_cursor_init(mgr, session->input, w, h);
    output->active_surfaces = c_list_new();
  }

  mgr->cursor_output = c_list_get(mgr->outputs, 0);

  c_event_loop_add(loop, drm_fd, drm_callback, mgr);

  return mgr;

error:
  c_output_manager_free(mgr);
  return NULL;
}
