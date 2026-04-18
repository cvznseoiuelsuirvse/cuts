#include <assert.h>
#include <unistd.h>
#include <inttypes.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "render/gl/egl.h"
#include "render/gl/gles.h"

#include "util/log.h"
#include "util/shm.h"

#define CUTS_GL_COLOR 0.8f, 0.1f, 0.2f, 1.0f
#define clear_color()   \
  glClearColor(CUTS_GL_COLOR); \
  glClear(GL_COLOR_BUFFER_BIT)

#define DMA_VERT_POS_TOP_LEFT(vp)     vp.tl_x, vp.tl_y
#define DMA_VERT_POS_BOTTOM_LEFT(vp)  vp.bl_x, vp.bl_y
#define DMA_VERT_POS_TOP_RIGHT(vp)    vp.tr_x, vp.tr_y
#define DMA_VERT_POS_BOTTOM_RIGHT(vp) vp.br_x, vp.br_y

#define SHM_VERT_POS_TOP_LEFT(vp)     vp.tl_x, -(vp.tl_y)
#define SHM_VERT_POS_BOTTOM_LEFT(vp)  vp.bl_x, -(vp.bl_y)
#define SHM_VERT_POS_TOP_RIGHT(vp)    vp.tr_x, -(vp.tr_y)
#define SHM_VERT_POS_BOTTOM_RIGHT(vp) vp.br_x, -(vp.br_y)

#define DMA_VERT_TOP_LEFT(vp)     DMA_VERT_POS_TOP_LEFT(vp),     0.0f, 1.0f
#define DMA_VERT_BOTTOM_LEFT(vp)  DMA_VERT_POS_BOTTOM_LEFT(vp),  0.0f, 0.0f
#define DMA_VERT_BOTTOM_RIGHT(vp) DMA_VERT_POS_BOTTOM_RIGHT(vp), 1.0f, 0.0f
#define DMA_VERT_TOP_RIGHT(vp)    DMA_VERT_POS_TOP_RIGHT(vp),    1.0f, 1.0f

#define SHM_VERT_TOP_LEFT(vp)     SHM_VERT_POS_TOP_LEFT(vp),     0.0f, 0.0f
#define SHM_VERT_BOTTOM_LEFT(vp)  SHM_VERT_POS_BOTTOM_LEFT(vp),  0.0f, 1.0f
#define SHM_VERT_BOTTOM_RIGHT(vp) SHM_VERT_POS_BOTTOM_RIGHT(vp), 1.0f, 1.0f
#define SHM_VERT_TOP_RIGHT(vp)    SHM_VERT_POS_TOP_RIGHT(vp),    1.0f, 0.0f

#define DMA_VERTS(vp) {DMA_VERT_TOP_LEFT(vp), DMA_VERT_BOTTOM_LEFT(vp), DMA_VERT_BOTTOM_RIGHT(vp), DMA_VERT_TOP_LEFT(vp), DMA_VERT_BOTTOM_RIGHT(vp), DMA_VERT_TOP_RIGHT(vp)}

#define SHM_VERTS(vp) {SHM_VERT_TOP_LEFT(vp), SHM_VERT_BOTTOM_LEFT(vp), SHM_VERT_BOTTOM_RIGHT(vp), SHM_VERT_TOP_LEFT(vp), SHM_VERT_BOTTOM_RIGHT(vp), SHM_VERT_TOP_RIGHT(vp)}

struct vert_pos {
  float tl_x, tl_y;
  float bl_x, bl_y;
  float br_x, br_y;
  float tr_x, tr_y;
};

struct texture {
  GLuint gl_texture;
  uint32_t width, height;
  int32_t x, y;
  enum c_wl_buffer_type buf_type;
};

enum c_render_notifier {
	C_RENDER_ON_WINDOW_NEW,
	C_RENDER_ON_WINDOW_UPDATE,
	C_RENDER_ON_WINDOW_CLOSE,
};

struct __render_event_listener {
  void *userdata;
  struct c_render_listener *listener;
};

static int __needs_redraw = 0;

static void notify(struct c_render *render, struct c_window *window, enum c_render_notifier notifier) {
  struct __render_event_listener *l;

  #define __notify(callback) \
    c_list_for_each(render->listeners, l) { \
      if (l->listener->callback) {\
        c_log(C_LOG_DEBUG, #callback " %p", window); \
        l->listener->callback(window, l->userdata); \
      } \
    }

  switch (notifier) {
    case C_RENDER_ON_WINDOW_NEW:       __notify(on_window_new); break;
    case C_RENDER_ON_WINDOW_UPDATE:    __notify(on_window_update); break;
    case C_RENDER_ON_WINDOW_CLOSE:     __notify(on_window_close); break;
    default: break;
  }

}

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *user_data) {
  struct c_render *render = user_data;

  render->swapchain.front ^= 1;
  render->drm->connector.waiting_for_flip = 0;

  struct c_window *window;
  c_map_for_each_values(render->windows, window) {
    struct c_wl_surface *surface;
    c_list_for_each(window->surfaces, surface)
      if (surface->frame_id) {
        c_wl_connection_callback_done(surface->conn, surface->frame_id);
        surface->frame_id = 0;
      }
  }

}

static int get_ft_fd(struct c_render *render) {
  int rfd, rwfd;
  size_t table_size = render->n_formats * sizeof(*render->formats); 

  if (new_shm(table_size, &rfd, &rwfd) < 0) {
    c_log(C_LOG_ERROR, "failed to create new shm file");
    return -1;
  }

  for (size_t i = 0; i < render->n_formats; i++) {
    struct c_format format = render->formats[i];
    struct {
      uint32_t format;
      uint32_t pad;
      uint64_t modifier;
    } entry = {
      .format = format.drm_format,
      .modifier = format.modifier
    };
    write(rwfd, &entry, sizeof(entry));
  }

  close(rwfd);
  return rfd;
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

  ctx->ft_fd = get_ft_fd(render);
  if (ctx->ft_fd == -1) {
    c_log(C_LOG_ERROR, "failed to get format table fd");
    goto error;
  }

  ctx->n_ft_entries = render->n_formats;

  return ctx;

error:
  free(ctx);
  return NULL;

}

static void *on_wl_shm_bind(struct c_wl_connection *conn, c_wl_object_id new_id, c_wl_uint version, void *userdata) {
  struct c_render *render = userdata;

  struct c_wl_formats *wl_formats = calloc(1, sizeof(*wl_formats));
  if (!wl_formats) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  wl_formats->formats = render->wl_formats;
  wl_formats->n_formats = render->n_formats;

  for (size_t i = 0; i < wl_formats->n_formats; i++) {
    if (i > 0 && wl_formats->formats[i-1] != wl_formats->formats[i])
      wl_shm_format(conn, new_id, wl_formats->formats[i]);
  }

  return wl_formats;
}

int c_render_handle_event(struct c_render *render) {
  drmEventContext ctx = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler,
  };

  if (drmHandleEvent(render->drm->fd, &ctx) < 0) {
    c_log_errno(C_LOG_ERROR, "drmHandleEvent failed");
    return -1;
  }

  return 0;
}

static inline float value_transform_x(int value, int max_value) {
  return -1 + (float)value/max_value * 2;
}

static inline float value_transform_y(int value, int max_value) {
  return 1 + (float)value/max_value * -2;
}

static void create_verts(uint32_t width, uint32_t height, int32_t x, int32_t y, 
    struct vert_pos *vp, uint32_t max_width, uint32_t max_height) {

  vp->tl_x = value_transform_x(x, max_width);
  vp->tl_y = value_transform_y(y, max_height);

  vp->bl_x = value_transform_x(x, max_width);
  vp->bl_y = value_transform_y(y + height, max_height);

  vp->br_x = value_transform_x(x + width, max_width);
  vp->br_y = value_transform_y(y + height, max_height);

  vp->tr_x = value_transform_x(x + width, max_width);
  vp->tr_y = value_transform_y(y, max_height);

}

static int render_texture(struct c_render *render, struct texture *texture) {
  struct c_gles *gl = render->gl;
  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(render->drm);

  glUseProgram(gl->program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture->gl_texture);

  uint32_t mon_width =  preferred_mode->width;
  uint32_t mon_height = preferred_mode->height;

  struct vert_pos vp = {0};
  create_verts(texture->width, texture->height, texture->x, texture->y, &vp, mon_width, mon_height);


  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  if (texture->buf_type == C_WL_BUFFER_DMA) {
    float dma_vertices[] = DMA_VERTS(vp);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(dma_vertices), dma_vertices);
  } else {
    float shm_vertices[] = SHM_VERTS(vp);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(shm_vertices), shm_vertices);
  }

  glUniform1i(glGetUniformLocation(gl->program, "tex"), 0);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  return 0;

}

static int c_render_import_shm(struct c_render *render, struct c_wl_buffer *buf) {
  struct c_shm *shm = buf->shm;
  c_gles_texture_from_shm(shm, buf->width / buf->surface->scale, buf->height / buf->surface->scale);
  return 0;
}

static int c_render_import_dmabuf(struct c_render *render, struct c_wl_buffer *buf) {
  int ret = 0;
  struct c_dmabuf *dmabuf = buf->dma;

  struct c_format *format = NULL;
  for (size_t i = 0; i < render->n_formats; i++) {
    format = &render->formats[i];
    if (format->drm_format == dmabuf->drm_format && format->modifier == dmabuf->modifier) break;
  }

  char *format_name = drmGetFormatName(dmabuf->drm_format);

  if (!format) {
    c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") 0x%08"PRIx64" pair not found", 
          format_name, dmabuf->drm_format, dmabuf->modifier);
    ret = -1;
    goto out;
  }

  if (format->n_planes != dmabuf->n_planes) {
    c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") requires %d planes, but %d was specified", 
          format_name, dmabuf->drm_format, format->n_planes, dmabuf->n_planes);
    ret = -1;
    goto out;

  }

  if (buf->width > format->max_width || buf->height > format->max_height) {
    c_log(C_LOG_ERROR, "buffer is too large. %s (0x%08"PRIx32") max resolution: %ux%u", 
          format_name, format->drm_format, format->max_width, format->max_height);
    ret = -1;
    goto out;
  };

  struct c_dmabuf_params params = {
    .width = buf->width / buf->surface->scale,
    .height = buf->height / buf->surface->scale,
    .modifier = dmabuf->modifier,
    .drm_format = dmabuf->drm_format,
    .n_planes = dmabuf->n_planes,
    .planes = dmabuf->planes,
  };

  dmabuf->image = c_egl_create_image_from_dmabuf(render->egl, &params);
  if (!dmabuf->image) {
    c_log(C_LOG_ERROR, "failed to import dmabuf"); 
    ret = -1;
    goto out;
  }

  c_gles_texture_from_dmabuf_image(render->gl, dmabuf);

  for (uint32_t i = 0; i < dmabuf->n_planes; i++) {
    close(dmabuf->planes[i].fd);
  } 

out:
  free(format_name);
  return ret;

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

static int draw_windows(struct c_render *render) {
  struct c_window *window;
  uint64_t key;
  c_map_for_each(render->windows, key, window) {
    struct c_wl_surface *surface = (struct c_wl_surface *)key;

    c_log(C_LOG_DEBUG, "window->surfaces->size=%d", window->surfaces->size);

    if (window->surfaces->size == 1 && surface->active) {
      struct texture texture  = {
        .gl_texture = ensure_buf_imported(render, surface->active),
        .width =  window->state & C_WINDOW_FLOAT ? surface->active->width : window->width,
        .height = window->state & C_WINDOW_FLOAT ? surface->active->height : window->height,
        .x = window->x,
        .y = window->y,
      };

      c_log(C_LOG_DEBUG, "%p: surface(0) %p width=%d height=%d x=%d y=%d", 
          surface->conn, surface, texture.width, texture.height, texture.x, texture.y);
      render_texture(render, &texture);

    } else {
      int i = -1;
      c_list_for_each(window->surfaces, surface) {
        i++;
        /* most likely the first surface created is a surface for decor */
        if (i == 0 || !surface->active) continue; 
  
        struct texture texture  = {0};
        texture.gl_texture = ensure_buf_imported(render, surface->active);
        assert(texture.gl_texture > 0);
        if (i == 1) {
          texture.width =  window->state & C_WINDOW_FLOAT ? surface->active->width : window->width;
          texture.height = window->state & C_WINDOW_FLOAT ? surface->active->height : window->height;
          texture.x = window->x;
          texture.y = window->y;

        } else {
          texture.width = surface->active->width;
          texture.height = surface->active->height;
          texture.x = window->x + surface->sub.x;
          texture.y = window->y + surface->sub.y;
        }

        c_log(C_LOG_DEBUG, "%p: surface(%d) %p width=%d height=%d x=%d y=%d", 
            surface->conn, i, surface, texture.width, texture.height, texture.x, texture.y);
        render_texture(render, &texture);

      }
    }
  }

  return 0;
}

static int destroy_wl_buffer(struct c_render *render, struct c_wl_buffer *buf) {
  if (buf->type == C_WL_BUFFER_DMA) {
    eglDestroyImage(render->egl->display, buf->dma->image);
    glDeleteTextures(1, &buf->dma->texture);
  } else if (buf->type == C_WL_BUFFER_SHM) {
    glDeleteTextures(1, &buf->shm->texture);
  } else return 0;

  /* buf->dma and buf->shm are union, so if buf if C_WL_BUFFER_SHM its still
   * gonna be destroyed */
  free(buf->dma);
  return 0;
}


static int redraw_scene(struct c_render *render) {
  if (render->drm->connector.waiting_for_flip) {
    __needs_redraw = 1;
    return 0;
  }

  struct c_render_buffer *back_buffer = render->swapchain.buffers[render->swapchain.front ^ 1];

  glBindFramebuffer(GL_FRAMEBUFFER, back_buffer->fbo);
  clear_color();
  if (draw_windows(render) == -1) return -1;
  glFlush();
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  drmModePageFlip(render->drm->fd, render->drm->connector.crtc_id,
                  back_buffer->drm_fb_id, DRM_MODE_PAGE_FLIP_EVENT, render);

  render->drm->connector.waiting_for_flip = 1;
  __needs_redraw = 0;
  return 0;
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

  if (!(render->swapchain.buffers[0] = c_render_buffer_create(render, width, height))) {
    c_log(C_LOG_ERROR, "failed to create swapchain buffer(0)");
    return -1;
  }

  if (!(render->swapchain.buffers[1] = c_render_buffer_create(render, width, height))) {
    c_log(C_LOG_ERROR, "failed to create swapchain buffer(1)");
    return -1;
  }

  render->swapchain.front = 0;

  return 0;
}

static void destroy_surface(struct c_render *render, struct c_wl_surface *surface) {
  if (surface->active) {
    destroy_wl_buffer(render, surface->active);
  }

  if (surface->pending) {
    destroy_wl_buffer(render, surface->pending);
  }

}

static int on_surface_destroy_cb(struct c_wl_surface *surface, void *userdata) {
  struct c_render *render = userdata;

  uint64_t window_key = (uint64_t)(surface->sub.parent ? surface->sub.parent : surface);
  struct c_window *window = c_map_get(render->windows, window_key);

  if (!window) return 0;

  struct c_wl_surface *s;
  c_list_for_each(window->surfaces, s) {
    if (s == surface) {
      c_list_remove_ptr(&window->surfaces, s);
      break;
    }
  }
  return redraw_scene(render);
}

static int on_client_gone(struct c_wl_connection *connection, void *userdata) {
  struct c_render *render = userdata;

  size_t key;
  struct c_window *window;
  c_map_for_each(render->windows, key, window) {
    if (window->conn == connection) {
      notify(render, window, C_RENDER_ON_WINDOW_CLOSE);

      struct c_wl_surface *surface;
      c_list_for_each(window->surfaces, surface)
        destroy_surface(render, surface);
      

      c_list_destroy(window->surfaces);
      c_map_remove(render->windows, key);
      break;
    }
  }
  return 0;
}

static struct c_window *add_new_window(struct c_render *render, struct c_wl_surface *surface) {
    struct c_window new_window = {0}; 

    new_window.surfaces = c_list_new();
    new_window.state |= C_WINDOW;
    new_window.conn = surface->conn;

    c_list_push(new_window.surfaces, surface, 0);
    return c_map_set(render->windows, (uint64_t)surface, &new_window, sizeof(new_window));
}

static int on_surface_update_cb(struct c_wl_surface *surface, void *userdata) {
  struct c_render *render = userdata;
  struct c_wl_surface *parent = surface->sub.parent;
  struct c_window *window;

  uint64_t window_key;

  if (surface->role == 0) return 0;

  if (surface->role == C_WL_SURFACE_ROLE_SUBSURFACE) {
    window_key = (uint64_t)parent;
    window = c_map_get(render->windows, window_key);

    if (!window)
      window = add_new_window(render, parent);

    if (c_list_idx(window->surfaces, surface) == -1)
      c_list_push(window->surfaces, surface, 0);

  } else {
    window_key = (uint64_t)surface;
    window = c_map_get(render->windows, window_key);

    if (!window) {
      window = add_new_window(render, surface);
      // struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(render->drm);
      // if (surface->xdg.max_height < preferred_mode->height 
      //     || surface->xdg.max_width < preferred_mode->width) {
      //   window->x = (uint32_t)preferred_mode->width / 2;
      //   window->y = (uint32_t)preferred_mode->height / 2;
      //   window->state |= C_WINDOW_FLOAT;
      // }
      notify(render, window, C_RENDER_ON_WINDOW_NEW);
    }
    
  }

  return redraw_scene(render);
}

static int on_buffer_destroy(struct c_wl_buffer *buffer, void *userdata) {
  struct c_render *render = userdata;
  destroy_wl_buffer(render, buffer);
  return 0;
}

C_EVENT_CALLBACK render_callback(struct c_event_loop *loop, int fd, void *userdata) {
  if (c_render_handle_event((struct c_render *)userdata) == -1) 
    return C_EVENT_ERROR_FATAL;

  if (__needs_redraw)
    if (redraw_scene((struct c_render *)userdata) == -1) return C_EVENT_ERROR_FATAL;

  return C_EVENT_OK;
}

struct c_render *c_render_init(struct c_wl_display *display, struct c_drm *drm) {
  struct c_render *render = calloc(1, sizeof(struct c_render));
  if (!render) 
    return NULL;

  render->windows = c_map_new(1024);
  render->drm = drm;


  render->egl = c_egl_init(render->drm->gbm_device);
  if (!render->egl) goto error;

  render->gl = c_gles_init();
  if (!render->gl) goto error;

  render->formats = c_egl_query_formats(render->egl, &render->n_formats);
  render->wl_formats = malloc(sizeof(uint32_t) * render->n_formats);

  if (create_swapchain(render) < 0) goto error;

  for (size_t i = 0; i < render->n_formats; i++) {
    uint32_t wl_format = drm_to_wl_shm_format(render->formats[i].drm_format);
    render->wl_formats[i] = wl_format;
  }

  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(drm);
  struct c_render_buffer *front_buffer = render->swapchain.buffers[0];
  if (drmModeSetCrtc(drm->fd, drm->connector.crtc_id, front_buffer->drm_fb_id,
                  0, 0, &drm->connector.id, 1, preferred_mode->info) != 0) {
    c_log_errno(C_LOG_ERROR, "drmModeSetCrtc failed");
    goto error;
  }

  glViewport(0, 0, preferred_mode->width, preferred_mode->height);
  if (redraw_scene(render) == -1) return NULL;
  c_event_loop_add(display->loop, drm->fd, render_callback, render);

  struct c_wl_display_listener dpy_listeners = {
    .on_client_gone = on_client_gone,
    .on_surface_update = on_surface_update_cb,
    .on_surface_destroy = on_surface_destroy_cb,
    .on_buffer_destroy = on_buffer_destroy,
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
  if (render->windows) {
    struct c_window *window;
    c_map_for_each_values(render->windows, window) {
      c_wl_connection_free(window->conn);
      free(window->surfaces);
    }
    
    c_map_destroy(render->windows);
  }

  if (render->swapchain.buffers[0])
    c_render_buffer_destroy(render, render->swapchain.buffers[0]);

  if (render->swapchain.buffers[1])
    c_render_buffer_destroy(render, render->swapchain.buffers[1]);

  if (render->egl)             c_egl_free(render->egl);
  if (render->gl)              c_gles_free(render->gl);

  if (render->formats)        free(render->formats);
  if (render->wl_formats)     free(render->wl_formats);

  if (render->listeners) {
    struct __render_event_listener *l;
    c_list_for_each(render->listeners, l)
      free(l->listener);
    c_list_destroy(render->listeners);

  }

  free(render);
}
