#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <gbm.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "render/render.h"
#include "render/egl.h"

#include "backend/drm.h"
#include "wayland/types/wayland.h"

#include "util/log.h"
#include "util/event_loop.h"

#include "wayland/types/xdg-shell.h"

#define CUTS_GL_COLOR 0.8f, 0.1f, 0.2f, 1.0f
#define clear_color()   \
  glClearColor(CUTS_GL_COLOR); \
  glClear(GL_COLOR_BUFFER_BIT)

struct vert_pos {
  float tl_x, tl_y;
  float bl_x, bl_y;
  float br_x, br_y;
  float tr_x, tr_y;
};

static int __needs_redraw = 0;
static c_list *__windows;

static int add_fb(struct c_render *render) {
  struct gbm_bo *bo = gbm_surface_lock_front_buffer(render->gbm_surface);
  if (!bo) { 
    c_log(C_LOG_ERROR, "gbm_surface_lock_front_buffer failed: %s", strerror(errno));
    return -1; 
  }
  render->gbm_bo_next = bo;

  uint32_t width = gbm_bo_get_width(bo);
  uint32_t height = gbm_bo_get_height(bo);
  uint32_t format = gbm_bo_get_format(bo);
  uint32_t handles[4] = {gbm_bo_get_handle(bo).u32};
  uint32_t pitches[4] = {gbm_bo_get_stride(bo)};
  uint32_t offsets[4] = {gbm_bo_get_offset(bo, 0)};

  render->drm->buf_id_old = render->drm->buf_id;
  if (drmModeAddFB2(render->drm->fd, 
                    width, height, format, 
                    handles, pitches, offsets, &render->drm->buf_id, 0) != 0) {
    c_log(C_LOG_ERROR, "drmModeAddFB2 failed: %s", strerror(errno));
    return -1;
  }

  return 0;

}

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *user_data) {
  c_log(C_LOG_DEBUG, "page_flip_handler");
  struct c_render *render = user_data;

  if (render->gbm_bo) {
    gbm_surface_release_buffer(render->gbm_surface, render->gbm_bo);
  }

  render->gbm_bo = render->gbm_bo_next;
  render->gbm_bo_next = NULL;
      
  render->drm->connector.waiting_for_flip = 0;

}

int c_render_handle_event(struct c_render *render) {
  drmEventContext ctx = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler,
  };

  if (drmHandleEvent(render->drm->fd, &ctx) < 0) {
    perror("c_drm_page_flip drmHandleEvent");
    return -1;
  }

  return 0;
}

int c_render_new_page_flip(struct c_render *render) {
  c_log(C_LOG_DEBUG, "c_render_new_page_flip");
  if (!render->drm->connector.waiting_for_flip) {
    c_egl_swap_buffers(render->egl);

    if (add_fb(render) != 0) return -1;
    if (drmModePageFlip(render->drm->fd, render->drm->connector.crtc_id, 
                        render->drm->buf_id, DRM_MODE_PAGE_FLIP_EVENT, render) != 0) {
      c_log(C_LOG_ERROR, "drmModePageFlip failed: %s", strerror(errno));
      return -1;
    }

    if (render->drm->buf_id_old)
      drmModeRmFB(render->drm->fd, render->drm->buf_id_old);

    render->drm->connector.waiting_for_flip = 1;
  }

  return 0;
}


static int gbm_init(struct c_render *render) {
  struct c_drm *drm = render->drm;
  render->gbm_device = gbm_create_device(drm->fd);
  if (!render->gbm_device) {
    c_log(C_LOG_ERROR, "gbm_create_device failed: %s", strerror(errno));
    return -1;
  }

  uint32_t width =  drm->connector.mode.width;
  uint32_t height = drm->connector.mode.height;
  render->gbm_surface = gbm_surface_create(render->gbm_device, 
                                           width, height, GBM_FORMAT_XRGB8888, 
                                           GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    
  if (!render->gbm_surface) {
    c_log(C_LOG_ERROR, "gbm_surface_create failed");
    return -1;
  }

  // render->gbm_bo = gbm_bo_create(render->gbm_device, width, height, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  //
  // if (!render->gbm_bo) {
  //   gbm_device_destroy(render->gbm_device);
  //   c_log(C_LOG_ERROR, "gbm_bo_create failed");
  //   return -1;
  // }

  return 0;
}

int c_render_get_ft_fd(struct c_render *render) {
  int fd = memfd_create("format_table", MFD_CLOEXEC);
  if (!fd) {
    c_log(C_LOG_ERROR, "memfd_create failed");
    return -1;
  }

  for (size_t i = 0; i < render->formats.n_entries; i++) {
    struct c_format format = render->formats.entries[i];
    struct {
      uint32_t format;
      uint32_t pad;
      uint64_t modifier;
    } entry = {
      .format = format.drm_format,
      .modifier = format.modifier
    };
    write(fd, &entry, sizeof(entry));
  }

  return fd;
}

int c_render_destroy_dmabuf(struct c_render *render, struct c_dmabuf *buf) {
  eglDestroyImage(render->egl->display, buf->image);
  glDeleteTextures(1, &buf->texture);

  return 0;
}

int c_render_import_dmabuf(struct c_render *render, struct c_dmabuf_params *params, struct c_dmabuf *buf) {
  assert(render->egl);

  struct c_format *format = NULL;
  for (size_t i = 0; i < render->formats.n_entries; i++) {
    format = &render->formats.entries[i];
    if (format->drm_format == params->drm_format && format->modifier == params->modifier) break;
  }

  char *format_name = drmGetFormatName(params->drm_format);

  if (!format) {
    c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") 0x%08"PRIx64" pair not found", 
          format_name, params->drm_format, params->modifier);
    goto err;
  }

  if (format->n_planes != params->n_planes) {
    c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") requires %d planes, but %d was specified", 
          format_name, params->drm_format, format->n_planes, params->n_planes);
    goto err;

  }

  if (params->width > format->max_width || params->height > format->max_height) {
    c_log(C_LOG_ERROR, "buffer is too large. %s (0x%08"PRIx32") max resolution: %ux%u", 
          format_name, format->drm_format, format->max_width, format->max_height);
    goto err;
  };

  free(format_name);

  c_egl_import_dmabuf(render->egl, params, buf);
  return 0;

err:
  free(format_name);
  return -1;
}

static inline float value_transform_x(int value, int max_value) {
  return -1 + (float)value/max_value * 2;
}

static inline float value_transform_y(int value, int max_value) {
  return 1 + (float)value/max_value * -2;
}

static void window_transform(struct c_render_window *window, struct vert_pos *vp, 
                              uint32_t max_width, uint32_t max_height) {

  vp->tl_x = value_transform_x(window->x, max_width);
  vp->tl_y = value_transform_y(window->y, max_height);

  vp->bl_x = value_transform_x(window->x, max_width);
  vp->bl_y = value_transform_y(window->y + window->height, max_height);

  vp->br_x = value_transform_x(window->x + window->width, max_width);
  vp->br_y = value_transform_y(window->y + window->height, max_height);

  vp->tr_x = value_transform_x(window->x + window->width, max_width);
  vp->tr_y = value_transform_y(window->y, max_height);

}

int draw(struct c_render *render, struct c_render_window *window) {
  struct c_gles *gl = render->egl->gl;
  GLuint texture = window->surface->active->dma->dma->texture;

  glUseProgram(render->egl->gl->program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  uint32_t width = render->drm->connector.mode.width;
  uint32_t height = render->drm->connector.mode.height;

  struct vert_pos vp = {0};
  window_transform(window, &vp, width, height);

  float vertices[] = {
  // positions         uv
    vp.tl_x, vp.tl_y,  0.0f, 0.0f, // top left
    vp.bl_x, vp.bl_y,  0.0f, 1.0f, // bottom left
    vp.br_x, vp.br_y,  1.0f, 1.0f, // bottom right
                            
    vp.br_x, vp.br_y,  1.0f, 1.0f, // bottom right
    vp.tr_x, vp.tr_y,  1.0f, 0.0f, // top right
    vp.tl_x, vp.tl_y,  0.0f, 0.0f, // top left
  };

  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

  glUniform1i(glGetUniformLocation(gl->program, "tex"), 0);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  return 0;

}

static void draw_windows(struct c_render *render) {
  struct c_render_window *window;

  c_list_for_each(__windows, window) {
    if (window->surface->active) {
      window->texture = 
      draw(render, window);

      c_wl_connection_callback_done(window->surface->conn, window->surface->id);
      wl_buffer_release(window->surface->conn, window->surface->active->id);

      window->surface->pending = window->surface->active;
      window->surface->active = NULL;
    }
  }

}


static void render_frame(struct c_render *render) {
  if (render->drm->connector.waiting_for_flip) {
    __needs_redraw = 1;
    return;
  }

  clear_color();
  draw_windows(render);

  if (!c_render_new_page_flip(render)) 
    __needs_redraw = 0;
}


static int _on_surface_new_cb(struct c_wl_surface *surface, void *userdata) {
  struct c_render_window window = {.surface = surface};
  c_list_push(__windows, &window, sizeof(window));
  return 0;
}

static int _on_surface_update_cb(struct c_wl_surface *surface, void *userdata) {
  render_frame((struct c_render *)userdata);
  return 0;
}

static int _on_window_new_cb(struct c_wl_surface *surface, void *userdata) {
  struct c_render *render = userdata;
  struct c_render_window *window = NULL;
  c_list_for_each(__windows, window) {
    if (window->surface == surface) {
      uint32_t mon_width = render->drm->connector.mode.width;
      uint32_t mon_height = render->drm->connector.mode.height;
      uint32_t gap = 15;

      window->x = gap;
      window->y = gap;

      window->width = mon_width - gap * 2;
      window->height = mon_height - gap * 2;

      c_wl_array arr = {0, NULL};
      xdg_toplevel_configure(surface->conn, surface->xdg_state.toplevel_id, window->width, window->height, &arr);
      return 0;
    }
  }
  return -1;
}

C_EVENT_CALLBACK render_callback(struct c_event_loop *loop, int fd, void *userdata) {
  if (c_render_handle_event((struct c_render *)userdata) == -1) 
    return -C_EVENT_ERROR_FATAL;

  if (__needs_redraw)
    render_frame((struct c_render *)userdata);

  return C_EVENT_OK;
}

struct c_render *c_render_init(struct c_wl_display *display, struct c_drm *drm) {
  int ret;
  struct c_render *render = calloc(1, sizeof(struct c_render));
  if (!render) 
    return NULL;

  __windows = c_list_new();
  render->drm = drm;
  ret = gbm_init(render); 
  if (ret != 0) goto error;

  render->egl = c_egl_init(render->gbm_device, render->gbm_surface);
  if (!render->egl) goto error;

  render->formats.entries = c_egl_query_formats(render->egl, &render->formats.n_entries);

  struct c_wl_interface *zwp_linux_dmabuf_v1 = c_wl_interface_get("zwp_linux_dmabuf_v1");
  assert(zwp_linux_dmabuf_v1);
  zwp_linux_dmabuf_v1->requests[2].handler_data = render; // zwp_linux_dmabuf_v1_get_default_feedback
  zwp_linux_dmabuf_v1->requests[3].handler_data = render; // zwp_linux_dmabuf_v1_get_surface_feedback
    
  struct c_wl_interface *zwp_linux_buffer_params_v1 = c_wl_interface_get("zwp_linux_buffer_params_v1");
  assert(zwp_linux_buffer_params_v1);
  zwp_linux_buffer_params_v1->requests[3].handler_data = render; // zwp_linux_buffer_params_v1_create_immed    
    
  struct c_wl_interface *wl_buffer = c_wl_interface_get("wl_buffer");
  assert(wl_buffer);
  wl_buffer->requests[0].handler_data = render;

  c_egl_swap_buffers(render->egl);
  if (add_fb(render) != 0) goto error;
  if (drmModeSetCrtc(drm->fd, drm->connector.crtc_id, drm->buf_id,
                  0, 0, &drm->connector.id, 1, drm->connector.mode.info) != 0) { 
    c_log(C_LOG_ERROR, "drmModeSetCrtc failed: %s", strerror(errno));
    goto error;
  }

  glViewport(0, 0, drm->connector.mode.width, drm->connector.mode.height);
  clear_color();

  c_render_new_page_flip(render);

  c_event_loop_add(display->loop, drm->fd, render_callback, render);

  struct c_wl_display_listener dpy_listeners = {
    .on_surface_new = _on_surface_new_cb,
    .on_surface_update = _on_surface_update_cb,
    .on_window_new = _on_window_new_cb,
  };
  c_wl_display_add_listener(display, &dpy_listeners, render);
    
  return render;

error:
  c_render_free(render);
  return NULL;
}

void c_render_free(struct c_render *render) {
  if (render->egl) c_egl_free(render->egl);

  if (render->gbm_bo)      gbm_bo_destroy(render->gbm_bo);
  if (render->gbm_bo_next) gbm_bo_destroy(render->gbm_bo_next);
  if (render->gbm_surface) gbm_surface_destroy(render->gbm_surface);
  if (render->gbm_device)  gbm_device_destroy(render->gbm_device);

  if (render->formats.entries)
    free(render->formats.entries);

  free(render);
}
