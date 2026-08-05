#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <unistd.h>
#include <fcntl.h>

#include "output/output.h"
#include "output/cursor.h"
#include "output/drm/drm.h"

#include "render/framebuffer.h"

#include "util/log.h"
#include "util/event_loop.h"

static int open_gpu(struct c_session *session) {
  char gpu_path[128];
  int gpu_found = 0;
  for (int i = 0; i < 10; i++) {
    memset(gpu_path, 0, sizeof(gpu_path));
    snprintf(gpu_path, sizeof(gpu_path), "/dev/dri/card%d", i);
    int fd = open(gpu_path, O_RDWR | O_NONBLOCK);
    if (fd > 0) {
      gpu_found = 1;
      close(fd);
      break;
    }
  }

  if (!gpu_found) {
    c_log(C_LOG_ERROR, "no GPUs found");
    return -1;
  }

  struct c_session_device *dev = c_session_device_open(session, gpu_path);
  if (!dev) return -1;

  return dev->fd;
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

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *userdata) {
  struct c_output *output = userdata;

  output->swapchain.front ^= 1;
  output->waiting_for_flip = 0;

  struct c_wl_surface *surface;
  c_list_for_each(output->frame_surfaces, surface) {
    if (surface->frame) {
      c_wl_connection_callback_done(surface->conn, surface->frame);
      surface->frame = 0;
    }
  }
}

static int handle_drm_event(struct c_output_manager *mgr) {
  drmEventContext ctx = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler,
  };

  if (drmHandleEvent(mgr->drm_fd, &ctx) < 0) {
    c_log_errno(C_LOG_ERROR, "drmHandleEvent failed");
    return -1;
  }
  return 0;
}

static void schedule_pageflip(struct c_output_manager *mgr, struct c_output *output) {
  if (output->need_redraw && !output->waiting_for_flip) {
    mgr->on_redraw(mgr, output, mgr->on_redraw_userdata);
    drmModePageFlip(mgr->drm_fd, output->crtc_id,
                    c_output_backbuffer(output)->drm_fb_id,
                    DRM_MODE_PAGE_FLIP_EVENT, output);
    output->waiting_for_flip = 1;
  }
}

C_EVENT_CALLBACK drm_callback(struct c_event_loop *loop, int fd, void *userdata) {
  struct c_output_manager *mgr = userdata;
  if (handle_drm_event(mgr) == -1) 
    return C_EVENT_ERROR_FATAL;

  struct c_output *output;
  c_list_for_each(mgr->outputs, output)
    schedule_pageflip(mgr, output);

  return C_EVENT_OK;
}

void c_output_damage(struct c_output_manager *mgr, struct c_output *output) {
  output->need_redraw = 1;
  schedule_pageflip(mgr, output);
}

void c_output_set_mode(struct c_output_manager *mgr, struct c_output *output, struct c_output_mode *mode) {
  while (output->waiting_for_flip)
    handle_drm_event(mgr);

  destroy_swapchain(output);

  output->current_mode = mode;
  create_swapchain(output, mgr);

  drmModeSetCrtc(mgr->drm_fd, output->crtc_id,
                 output->swapchain.buffers[output->swapchain.front]->drm_fb_id,
                 0, 0, &output->connector_id, 1, &mode->drm_info);

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
      c_list_destroy(output->frame_surfaces);
      destroy_swapchain(output);

      c_list_destroy(output->modes);

      drmModeSetCrtc(mgr->drm_fd, output->orig_crtc->crtc_id,
                     output->orig_crtc->buffer_id, 0, 0, &output->connector_id,
                     1, &output->orig_crtc->mode);
      drmModeFreeCrtc(output->orig_crtc);

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

  mgr->outputs = c_drm_get_connectors(drm_fd);
  if (!mgr->outputs) {
    c_log(C_LOG_ERROR, "failed to get connectors");
    goto error;
  }


  uint64_t w, h;
  if (drmGetCap(mgr->drm_fd, DRM_CAP_CURSOR_WIDTH, &w) != 0) w = 64;
  if (drmGetCap(mgr->drm_fd, DRM_CAP_CURSOR_HEIGHT, &h) != 0) h = 64;

  struct c_output *output;
  c_list_for_each(mgr->outputs, output) {
    output->cursor = c_cursor_init(mgr, session->input, w, h);
    output->frame_surfaces = c_list_new();
  }

  mgr->cursor_output = c_list_get(mgr->outputs, 0);

  c_event_loop_add(loop, drm_fd, drm_callback, mgr);

  return mgr;

error:
  c_output_manager_free(mgr);
  return NULL;
}
