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
#include "render/gl/egl.h"
#include "render/gl/gles.h"

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

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *user_data) {
  struct c_render *render = user_data;

  if (render->gbm_bo) {
    gbm_surface_release_buffer(render->gbm_surface, render->gbm_bo);
  }

  render->gbm_bo = render->gbm_bo_next;
  render->gbm_bo_next = NULL;
      
  render->drm->connector.waiting_for_flip = 0;

  size_t key;
  struct c_window *window;
  c_map_for_each(render->surfaces, key, window) {
    struct c_wl_surface *surface = (struct c_wl_surface *)key;
      if (surface->active)
        c_wl_connection_callback_done(surface->conn, surface->id);
  }

}

static void *on_linux_dmabuf_bind(struct c_wl_connection *conn, c_wl_object_id new_id, c_wl_uint version, void *userdata) {
  struct c_render *render = userdata;
  struct c_wl_linux_dmabuf_ctx *ctx = malloc(sizeof(*ctx));
  if (!ctx) {
    c_log(C_LOG_ERROR, "malloc(c_wl_linux_dmabuf_ctx) failed");
    return NULL;
  }

  if (c_drm_dev_id(render->drm, &ctx->drm_dev_id) == -1)  {
    c_log_errno(C_LOG_ERROR, "failed to get drm rdev"); 
    goto error;
  }

  ctx->ft_fd = c_render_get_ft_fd(render);
  if (ctx->ft_fd == -1) {
    c_log_errno(C_LOG_ERROR, "failed to get format table fd");
    goto error;
  }
  ctx->n_ft_entries = render->formats.n_entries;

  return ctx;

error:
  free(ctx);
  return NULL;

}

static void *on_wl_shm_bind(struct c_wl_connection *conn, c_wl_object_id new_id, c_wl_uint version, void *userdata) {
  struct c_render *render = userdata;
  int *fmt;

  c_list_for_each(render->formats.wl_shm_formats, fmt)
      wl_shm_format(conn, new_id, (uint64_t)fmt);
  
  return render->formats.wl_shm_formats;
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
  struct c_drm *drm = render->drm;;
  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(drm);

  uint32_t width =  preferred_mode->width;
  uint32_t height = preferred_mode->height;

  // render->gbm_bo = gbm_bo_create(render->gbm_device, width, height, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  //
  // if (!render->gbm_bo) {
  //   gbm_device_destroy(render->gbm_device);
  //   c_log(C_LOG_ERROR, "gbm_bo_create failed");
  //   return -1;
  // }

  return 0;
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
  struct c_gles *gl = render->gl;
  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(render->drm);

  glUseProgram(gl->program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  uint32_t width =  preferred_mode->width;
  uint32_t height = preferred_mode->height;

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

static int c_render_import_shm(struct c_render *render, struct c_wl_buffer *buf) {
  struct c_shm *shm = buf->shm;
  c_gles_texture_from_shm(shm, buf->width, buf->height);
  return 0;
}

static int c_render_import_dmabuf(struct c_render *render, struct c_wl_buffer *buf) {
  struct c_dmabuf *dmabuf = buf->dma;

  struct c_format *format = NULL;
  for (size_t i = 0; i < render->formats.n_entries; i++) {
    format = &render->formats.entries[i];
    if (format->drm_format == dmabuf->drm_format && format->modifier == dmabuf->modifier) break;
  }

  char *format_name = drmGetFormatName(dmabuf->drm_format);

  if (!format) {
    c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") 0x%08"PRIx64" pair not found", 
          format_name, dmabuf->drm_format, dmabuf->modifier);
    goto err;
  }

  if (format->n_planes != dmabuf->n_planes) {
    c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") requires %d planes, but %d was specified", 
          format_name, dmabuf->drm_format, format->n_planes, dmabuf->n_planes);
    goto err;

  }

  if (buf->width > format->max_width || buf->height > format->max_height) {
    c_log(C_LOG_ERROR, "buffer is too large. %s (0x%08"PRIx32") max resolution: %ux%u", 
          format_name, format->drm_format, format->max_width, format->max_height);
    goto err;
  };

  free(format_name);

  struct c_dmabuf_params params = {
    .width = buf->width,
    .height = buf->height,
    .modifier = dmabuf->modifier,
    .drm_format = dmabuf->drm_format,
    .n_planes = dmabuf->n_planes,
    .planes = dmabuf->planes,
  };

  dmabuf->image = c_egl_create_dmabuf_image(render->egl, &params);
  if (!dmabuf->image) {
    c_log(C_LOG_ERROR, "failed to create an EGLImageKHR"); 
    goto err;
  }

  c_gles_texture_from_dmabuf_image(render->gl, dmabuf);

  for (uint32_t i = 0; i < dmabuf->n_planes; i++) {
    close(dmabuf->planes[i].fd);
  } 

  return 0;

err:
  free(format_name);
  return -1;
}


static GLuint ensure_buf_imported(struct c_render *render, struct c_wl_buffer *buf) {
  if (buf->type == C_WL_BUFFER_DMA) {
    if (buf->dma->texture == 0)
      if (c_render_import_dmabuf(render, buf) < 0) return 0;

    return buf->dma->texture;

  } else if (buf->type == C_WL_BUFFER_SHM) {
    if (buf->shm->texture == 0) 
      if (c_render_import_shm(render, buf) < 0) return 0;

    return buf->shm->texture;
  }

  assert(0);
}

static void draw_windows(struct c_render *render) {
  struct c_window *window;
  size_t key;
  c_map_for_each(render->surfaces, key, window) {
    struct c_wl_surface *surface = (struct c_wl_surface *)key;
    if (surface->active && surface->role != C_WL_SURFACE_ROLE_SUBSURFACE) {
      GLuint texture = ensure_buf_imported(render, surface->active);
      if (texture == 0) return;
      c_log(C_LOG_DEBUG, "window %p: width=%d height=%d x=%d y=%d", window, 
            window->width, window->height, window->x, window->y);
      draw_window(render, window, texture);

      struct c_wl_surface *sub_surface;
      c_list_for_each(surface->sub.children, sub_surface) {
        if (!sub_surface->active || !sub_surface->sub.sync) continue;

        texture = ensure_buf_imported(render, sub_surface->active);
        if (texture == 0) return;

        struct c_window *sub_window = c_map_get(render->surfaces, (uint64_t)sub_surface);
        sub_window->x = window->x +sub_surface->sub.x;
        sub_window->y = window->y + sub_surface->sub.y;
        sub_window->width = sub_surface->active->width;
        sub_window->height = sub_surface->active->height;
        c_log(C_LOG_DEBUG, "sub window %p: width=%d height=%d x=%d y=%d", sub_window, 
              sub_window->width, sub_window->height, sub_window->x, sub_window->y);

        draw_window(render, sub_window, texture);
      }
      
    }
  }
}

static int c_render_destroy_buf(struct c_render *render, struct c_wl_buffer *buf) {
  if (buf->type == C_WL_BUFFER_DMA) {
    eglDestroyImage(render->egl->display, buf->dma->image);
    glDeleteTextures(1, &buf->dma->texture);
  } else if (buf->type == C_WL_BUFFER_SHM) {
    glDeleteTextures(1, &buf->shm->texture);
  }
  free(buf);
  return 0;
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
  if (surface->active)
    c_render_destroy_buf(render, surface->active);

  if (surface->pending)
    c_render_destroy_buf(render, surface->pending);

  c_map_remove(render->surfaces, (uint64_t)surface);
  c_render_redraw(render);
  return 0;
}

static int _on_client_gone(struct c_wl_connection *connection, void *userdata) {
  struct c_render *render = userdata;

  size_t key;
  struct c_window *window;
  c_map_for_each(render->surfaces, key, window) {
    struct c_wl_surface *surface = window->wl_surface;
      if (surface->conn == connection) {
        notify(render, window, C_RENDER_ON_WINDOW_CLOSE);
        _on_surface_destroy_cb(surface, userdata);
        break;
    }
  }
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


int c_render_get_ft_fd(struct c_render *render) {
  int fd = memfd_create("format_table", MFD_CLOEXEC);
  if (!fd)
    return -1;

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

void c_render_add_listener(struct c_render *render, struct c_render_listener *listener, void *userdata) {
  struct __render_event_listener l = {
    .userdata = userdata,
    .listener = calloc(1, sizeof(*listener)),
  };
  memcpy(l.listener, listener, sizeof(*listener));
  c_list_push(render->listeners, &l, sizeof(l)); 
}


static int create_swapchain(struct c_render *render) {
  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(render->drm);
  uint32_t width = preferred_mode->width;
  uint32_t height = preferred_mode->height;

  render->swapchain.buffers[0] = c_render_buffer_create(render, width, height, DRM_FORMAT_XRGB8888, NULL, 0);
  render->swapchain.buffers[1] = c_render_buffer_create(render, width, height, DRM_FORMAT_XRGB8888, NULL, 0);
  if (!(uint64_t)(render->swapchain.buffers[0] || render->swapchain.buffers[1])) return -1;

  render->swapchain.front = 0;

  return 0;
}

struct c_render *c_render_init(struct c_wl_display *display, struct c_drm *drm) {
  int ret;
  struct c_render *render = calloc(1, sizeof(struct c_render));
  if (!render) 
    return NULL;

  render->surfaces = c_map_new(1024);
  render->drm = drm;

  render->gbm_device = gbm_create_device(drm->fd);
  if (!render->gbm_device) {
    c_log_errno(C_LOG_ERROR, "gbm_create_device failed");
    goto error;
  }

  render->egl = c_egl_init(render->gbm_device);
  if (!render->egl) goto error;

  render->gl = c_gles_init();
  if (!render->gl) goto error;

  if (create_swapchain(render) < 0) goto error;

  render->formats.entries = c_egl_query_formats(render->egl, &render->formats.n_entries);
  render->formats.wl_shm_formats = c_list_new();

  for (size_t i = 0; i < render->formats.n_entries; i++) {
    struct c_format format = render->formats.entries[i];
    c_list_push(render->formats.wl_shm_formats, (void *)drm_to_wl_shm_format(format.drm_format), 0);
  }

  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(drm);
  if (drmModeSetCrtc(drm->fd, drm->connector.crtc_id, drm->buf_id,
                  0, 0, &drm->connector.id, 1, preferred_mode->info) != 0) { 
    c_log_errno(C_LOG_ERROR, "drmModeSetCrtc failed");
    goto error;
  }

  glViewport(0, 0, preferred_mode->width, preferred_mode->height);
  clear_color();

  c_render_new_page_flip(render);

  c_event_loop_add(display->loop, drm->fd, render_callback, render);

  struct c_wl_display_listener dpy_listeners = {
    .on_client_gone = _on_client_gone,
    .on_surface_new = _on_surface_new_cb,
    .on_surface_update = _on_surface_update_cb,
    .on_surface_destroy = _on_surface_destroy_cb,
    .on_toplevel_new = _on_toplevel_new,
  };

  c_wl_display_add_listener(display, &dpy_listeners, render);

  c_wl_display_add_supported_interface(display, "wl_shm", on_wl_shm_bind, render);
  c_wl_display_add_supported_interface(display, "zwp_linux_dmabuf_v1", on_linux_dmabuf_bind, render);

  render->listeners = c_list_new();
    
  return render;

error:
  c_render_free(render);
  return NULL;
}

void c_render_free(struct c_render *render) {
  if (render->surfaces) {
    size_t key;
    struct c_window *window;
    c_map_for_each(render->surfaces, key, window) {
      struct c_wl_surface *surface = (struct c_wl_surface *)key;
      if (surface->active)
        c_render_destroy_buf(render, surface->active);

      if (surface->pending)
        c_render_destroy_buf(render, surface->pending);
    }
    c_map_destroy(render->surfaces);
  }

  if (render->swapchain.buffers[0])
    c_render_buffer_destroy(render, render->swapchain.buffers[0]);

  if (render->swapchain.buffers[1])
    c_render_buffer_destroy(render, render->swapchain.buffers[1]);

  if (render->egl)              c_egl_free(render->egl);
  if (render->gl)              c_gles_free(render->gl);
  if (render->gbm_device)       gbm_device_destroy(render->gbm_device);

  if (render->formats.entries)        free(render->formats.entries);
  if (render->formats.wl_shm_formats) free(render->formats.wl_shm_formats);

  if (render->listeners) {
    struct __render_event_listener *l;
    c_list_for_each(render->listeners, l)
      free(l->listener);
    c_list_destroy(render->listeners);

  }

  free(render);
}
