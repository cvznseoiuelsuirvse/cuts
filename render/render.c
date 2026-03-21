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

#include "backend/drm/drm.h"

#include "util/log.h"
#include "util/event_loop.h"

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

enum c_render_notifer {
	C_RENDER_ON_WINDOW_NEW,
	C_RENDER_ON_WINDOW_CLOSE,
};

struct __render_event_listener {
  void *userdata;
  struct c_render_listener *listener;
};

static int __needs_redraw = 0;

static void notify(struct c_render *render, struct c_window *window, enum c_render_notifer notifier) {
  struct __render_event_listener *l;

  #define __notify(callback) \
    c_list_for_each(render->listeners, l) { \
      if (l->listener->callback) {\
        l->listener->callback(window, l->userdata); \
      } \
    }

  switch (notifier) {
    case C_RENDER_ON_WINDOW_NEW:       __notify(on_window_new); break;
    case C_RENDER_ON_WINDOW_CLOSE:     __notify(on_window_close); break;
    default: break;
  }

}


static int add_fb(struct c_render *render) {
  struct gbm_bo *bo = gbm_surface_lock_front_buffer(render->gbm_surface);
  if (!bo) { 
    c_log_errno(C_LOG_ERROR, "gbm_surface_lock_front_buffer failed");
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
    c_log_errno(C_LOG_ERROR, "drmModeAddFB2 failed");
    return -1;
  }

  return 0;

}

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *user_data) {
  struct c_render *render = user_data;

  if (render->gbm_bo) {
    gbm_surface_release_buffer(render->gbm_surface, render->gbm_bo);
  }

  render->gbm_bo = render->gbm_bo_next;
  render->gbm_bo_next = NULL;
      
  render->drm->connector.waiting_for_flip = 0;

  struct c_window *window;
  size_t key;

  c_map_for_each(render->surfaces, key, window) {
    struct c_wl_surface *surface = (struct c_wl_surface *)key;
      if (surface->active)
        c_wl_connection_callback_done(surface->conn, surface->id);
  }

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
  if (!render->drm->connector.waiting_for_flip) {
    c_egl_swap_buffers(render->egl);

    if (add_fb(render) != 0) return -1;
    if (drmModePageFlip(render->drm->fd, render->drm->connector.crtc_id, 
                        render->drm->buf_id, DRM_MODE_PAGE_FLIP_EVENT, render) != 0) {
      c_log_errno(C_LOG_ERROR, "drmModePageFlip failed");
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

  uint32_t width =  drm->output->width;
  uint32_t height = drm->output->height;
  render->gbm_surface = gbm_surface_create(render->drm->gbm_device, 
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
  free(buf);
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

static void window_transform(struct c_window *window, struct vert_pos *vp, 
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

static int draw_window(struct c_render *render, struct c_window *window, GLuint texture) {
  struct c_gles *gl = render->egl->gl;

  glUseProgram(render->egl->gl->program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  uint32_t width = render->drm->output->width;
  uint32_t height = render->drm->output->height;

  struct vert_pos vp = {0};
  window_transform(window, &vp, width, height);

  float vertices[] = {
  //positions          uv
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
  struct c_window *window;
  size_t key;

  c_map_for_each(render->surfaces, key, window) {
    struct c_wl_surface *surface = (struct c_wl_surface *)key;
      if (surface->active)
        draw_window(render, window, surface->active->dma->texture);
  }
}


void c_render_redraw(struct c_render *render) {
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
  struct c_render *render = userdata;
  struct c_window window = {0};
  window.wl_surface = surface;
  c_map_set(render->surfaces, (uint64_t)surface, &window, sizeof(window));
  return 0;
}

static int _on_surface_destroy_cb(struct c_wl_surface *surface, void *userdata) {
  struct c_render *render = userdata;
  c_map_remove(render->surfaces, (uint64_t)surface);
  c_render_redraw(render);
  return 0;
}

static int _on_surface_update_cb(struct c_wl_surface *surface, void *userdata) {
  c_render_redraw((struct c_render *)userdata);
  return 0;
}

static int _on_toplevel_new(struct c_wl_surface *surface, void *userdata) {
  struct c_render *render = userdata;
  struct c_window *window = c_map_get(render->surfaces, (uint64_t)surface);
  notify(render, window, C_RENDER_ON_WINDOW_NEW);
  return 0;
  
}

C_EVENT_CALLBACK render_callback(struct c_event_loop *loop, int fd, void *userdata) {
  if (c_render_handle_event((struct c_render *)userdata) == -1) 
    return C_EVENT_ERROR_FATAL;

  if (__needs_redraw)
    c_render_redraw((struct c_render *)userdata);

  return C_EVENT_OK;
}

void c_render_add_listener(struct c_render *render, struct c_render_listener *listener, void *userdata) {
  struct __render_event_listener l = {
    .userdata = userdata,
    .listener = calloc(1, sizeof(*listener)),
  };
  memcpy(l.listener, listener, sizeof(*listener));
  c_list_push(render->listeners, &l, sizeof(l)); 
}

struct c_render *c_render_init(struct c_wl_display *display, struct c_drm *drm) {
  int ret;
  struct c_render *render = calloc(1, sizeof(struct c_render));
  if (!render) 
    return NULL;

  render->surfaces = c_map_new(1024);
  render->drm = drm;
  ret = gbm_init(render); 
  if (ret != 0) goto error;

  render->egl = c_egl_init(render->drm->gbm_device, render->gbm_surface);
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
                  0, 0, &drm->connector.id, 1, drm->connector.info) != 0) { 
    c_log_errno(C_LOG_ERROR, "drmModeSetCrtc failed");
    goto error;
  }

  glViewport(0, 0, drm->output->width, drm->output->height);
  clear_color();

  c_render_new_page_flip(render);

  c_event_loop_add(display->loop, drm->fd, render_callback, render);

  struct c_wl_display_listener dpy_listeners = {
    .on_surface_new = _on_surface_new_cb,
    .on_surface_update = _on_surface_update_cb,
    .on_surface_destroy = _on_surface_destroy_cb,
    .on_toplevel_new = _on_toplevel_new,
  };
  c_wl_display_add_listener(display, &dpy_listeners, render);

  render->listeners = c_list_new();
    
  return render;

error:
  c_render_free(render);
  return NULL;
}

void c_render_free(struct c_render *render) {
  if (render->egl)              c_egl_free(render->egl);
  if (render->gbm_bo)           gbm_bo_destroy(render->gbm_bo);
  if (render->gbm_bo_next)      gbm_bo_destroy(render->gbm_bo_next);
  if (render->gbm_surface)      gbm_surface_destroy(render->gbm_surface);
  if (render->formats.entries)  free(render->formats.entries);
  if (render->surfaces)         c_map_destroy(render->surfaces);

  if (render->listeners) {
    struct __render_event_listener *l;
    c_list_for_each(render->listeners, l)
      free(l->listener);
    c_list_destroy(render->listeners);

  }

  free(render);
}
