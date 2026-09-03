#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <gbm.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "output/drm/util.h"
#include "output/drm/syncobj.h"

#include "render/renderer.h"
#include "render/gl/egl.h"
#include "render/gl/gles.h"
#include "render/framebuffer.h"

#include "wayland/display.h"
#include "wayland/proto/wayland.h"
#include "wayland/impl/wayland.h"
#include "wayland/proto/linux-dmabuf-v1.h"

#include "util/log.h"
#include "util/shm.h"
#include "util/mem.h"

static struct wl_shm_pool shm_pool;
struct zwp_linux_dmabuf_v1 linux_dmabuf;

static int import_raw(struct c_renderer *render, struct c_rawbuf *shm) {
	if (c_gles_texture_from_raw(render->gl, shm) == -1) {
    c_log(C_LOG_ERROR, "failed to create a texture from shm");
    return -1;
  }

	char *format_name = drmGetFormatName(shm->format);
  c_log(C_LOG_DEBUG, "imported SHM %p: %dx%d %s (0x%08"PRIx32")",
        shm, shm->width, shm->height, format_name, shm->format);

  free(format_name);
	return 0;
}

static int import_dmabuf(struct c_renderer *render, struct c_dmabuf *dmabuf) {
	int ret = 0;

	struct c_format *format = NULL;
	for (size_t i = 0; i < render->format_table_entries; i++) {
		format = &render->formats[i];
		if (format->drm_format == dmabuf->drm_format && format->modifier == dmabuf->modifier) break;
	}

	char *format_name = drmGetFormatName(dmabuf->drm_format);

	if (!format) {
		c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") 0x%08"PRIx64" pair not found",
		      format_name, dmabuf->drm_format, dmabuf->modifier);
		ret = -1;
		goto out_pre_image;
	}

	if (format->n_planes != dmabuf->n_planes) {
		c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") requires %d planes, but %d was specified",
		      format_name, dmabuf->drm_format, format->n_planes, dmabuf->n_planes);
		ret = -1;
		goto out_pre_image;
	}

	if (dmabuf->width > format->max_width || dmabuf->height > format->max_height) {
		c_log(C_LOG_ERROR, "buffer is too large. %s (0x%08"PRIx32") max resolution: %ux%u",
		      format_name, format->drm_format, format->max_width, format->max_height);
		ret = -1;
		goto out_pre_image;
	}

	dmabuf->image = c_egl_create_image_from_dmabuf(render->egl, dmabuf);
	if (!dmabuf->image) {
		c_log(C_LOG_ERROR, "failed to import dmabuf");
		ret = -1;
		goto out;
	}

	if (c_gles_texture_from_dma(render->gl, dmabuf) == -1) {
    c_log(C_LOG_ERROR, "failed to create a texture from dmabuf");
    ret = -1;
    goto out;
  }

  c_log(C_LOG_DEBUG, "imported DMA %p: %dx%d %s (0x%08" PRIx32 ") 0x%08" PRIx64,
        dmabuf, dmabuf->width, dmabuf->height, format_name, dmabuf->drm_format, dmabuf->modifier);
out:
	for (uint32_t i = 0; i < dmabuf->n_planes; i++) close(dmabuf->planes[i].fd);

out_pre_image:

	free(format_name);
	return ret;
}

static void defer_dma(void *data) {
  c_log(C_LOG_DEBUG, __PRETTY_FUNCTION__);
  struct c_dmabuf *buf = data;
  struct c_renderer *renderer = buf->renderer;
  if (buf->image && renderer->egl->proc.eglDestroyImageKHR)
    renderer->egl->proc.eglDestroyImageKHR(renderer->egl->display, buf->image);

  if (buf->texture) {
    glDeleteTextures(1, &buf->texture->texture);
    free(buf->texture);
  }
  c_log(C_LOG_DEBUG, "DMA %p destroyed", buf);
}

static void defer_shm(void *data) {
  struct c_rawbuf *buf = data;
  if (buf->texture) {
    glDeleteTextures(1, &buf->texture->texture);
    free(buf->texture);
  }
  c_log(C_LOG_DEBUG, "SHM %p destroyed", buf);
}

static struct c_gles_texture *ensure_imported(struct c_renderer *render, void *buffer,
                enum c_render_buffer_type buf_type) {

  if (buf_type == C_BUFFER_DMA) {
    struct c_dmabuf *buf = buffer;
		if (!buf->texture) {
			if (import_dmabuf(render, buf) < 0) return NULL;

      buf->renderer = render;
      c_defer(buf, defer_dma);
    }

		return buf->texture;

	} else if (buf_type == C_BUFFER_RAW) {
    struct c_rawbuf *buf = buffer;
    int no_texture = !buf->texture;
		if (!buf->texture || buf->dirty) {
			if (import_raw(render, buf) < 0) return NULL;
      buf->dirty = 0;
      if (no_texture) {
        c_defer(buf, defer_shm);
      }
    }

		return buf->texture;
	}

	assert(0);
}

static int create_format_table(struct c_renderer *render) {
  int rfd, rwfd;
  size_t table_size = render->format_table_entries * sizeof(*render->formats); 

  if (new_shm(table_size, &rfd, &rwfd) < 0) {
    c_log(C_LOG_ERROR, "failed to create new shm file");
    return -1;
  }

  for (size_t i = 0; i < render->format_table_entries; i++) {
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

static void *on_wl_shm_bind(struct c_wl_connection *conn, struct c_wl_object *wl_shm, void *userdata) {
  struct c_renderer *render = userdata;

  for (size_t i = 0; i < render->format_table_entries; i++) {
    enum wl_shm_format_enum wl_format = drm_fmt_to_wl_shm_fmt(render->formats[i].drm_format);
    wl_shm_format(conn, wl_shm->id, wl_format);
  }

  return NULL;
}

static int get_dev_id(int drm_fd, dev_t *dev_id) {
	struct stat stat;
	if (fstat(drm_fd, &stat) != 0) {
    c_log_errno(C_LOG_ERROR, "failed to call fstat on DRM fd");
		return -1;
  }

	*dev_id = stat.st_rdev;
  return 0;
}

static int send_feedback(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_output_manager *mgr = userdata;
  struct c_wl_object *feedback = c_wl_object_get(conn, args[1].n);

  dev_t drm_dev_id;
  if (get_dev_id(mgr->drm_fd, &drm_dev_id) == -1)  {
    c_log_errno(C_LOG_ERROR, "failed to get drm rdev"); 
    return 1;
  }

  c_wl_array device = {
    sizeof(dev_t), 
    &drm_dev_id,
  };

  int ft_fd = create_format_table(mgr->renderer);
  if (ft_fd == -1) {
    c_log(C_LOG_ERROR, "failed to create format table");
    return 1;
  }

  size_t format_table_entries = mgr->renderer->format_table_entries;

  zwp_linux_dmabuf_feedback_v1_main_device(conn, feedback->id, &device);

  zwp_linux_dmabuf_feedback_v1_format_table(conn, feedback->id, ft_fd, format_table_entries * 16);
  zwp_linux_dmabuf_feedback_v1_tranche_target_device(conn, feedback->id, &device);
  zwp_linux_dmabuf_feedback_v1_tranche_flags(conn, feedback->id, ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT);

  uint16_t data[format_table_entries];
  c_wl_array arr = {
    .size = format_table_entries * 2,
  };

  for (size_t i = 0; i < format_table_entries; i++) {
    data[i] = i;
  }

  arr.data = data;

  zwp_linux_dmabuf_feedback_v1_tranche_formats(conn, feedback->id, &arr);
  zwp_linux_dmabuf_feedback_v1_done(conn, feedback->id);

  return 0;
}

int c_renderer_draw(struct c_renderer *renderer, struct c_output *output,
                    struct c_renderer_quad *quad) {
  if (quad->type == C_RENDERER_BUFFER) {
    struct c_gles_texture *texture = ensure_imported(renderer, quad->buffer, quad->buffer_type);
    if (!texture) {
      c_log(C_LOG_ERROR, "failed to import %s buffer", quad->buffer_type == C_BUFFER_DMA ? "DMA" : "SHM");
      return -1;
    }
    c_gles_add_texture(renderer->gl, output, quad, texture);
  } else {
    c_gles_add_solid(renderer->gl, output, quad);
  }
  return 0;
}

void c_renderer_begin(struct c_renderer *renderer, struct c_output *output) {
  struct c_framebuffer *back_buffer = output->swapchain.buffers[output->swapchain.front ^ 1];
  glBindFramebuffer(GL_FRAMEBUFFER, back_buffer->fbo);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
}

int c_renderer_commit(struct c_renderer *render, struct c_output *output) {
  if (render->egl->ext_support.KHR_fence_sync) {
    EGLSyncKHR sync = c_egl_create_sync(render->egl);
    if (!sync) {
      c_log(C_LOG_ERROR, "failed to create EGLSyncKHR");
      return -1;
    }

    EGLint fence_fd = c_egl_dup_fence_fd(render->egl, sync);
    if (fence_fd == -1) {
      c_log(C_LOG_ERROR, "failed to dup fence fd");
      return -1;
    }
    c_egl_destroy_sync(render->egl, sync);

    if (c_drm_import_sync_file(output->timeline, fence_fd)) {
      c_log(C_LOG_ERROR, "failed to import sync file into timeline");
      return -1;
    }
    close(fence_fd);

  } else {
    c_log(C_LOG_DEBUG, "glFlush");
    glFlush();
    glFinish();
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  output->need_redraw = 0;
  return 0;
}

static int on_shm_buffer_create(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_renderer *renderer = userdata;

  struct c_wl_buffer *buffer = c_wl_object_get(conn, args[1].n)->data;
  struct c_rawbuf *rawbuf = buffer->shm;

  for (size_t i = 0; i < renderer->format_table_entries; i++) {
    uint32_t supported_format = renderer->formats[i].drm_format;
    if (supported_format == rawbuf->format) return 0;
  }

  c_wl_error_set_and_return(args[0].o, WL_SHM_ERROR_INVALID_FORMAT, "format not supported");
}

struct c_renderer *c_renderer_init(struct c_output_manager *mgr) {
  struct c_renderer *render = calloc(1, sizeof(struct c_renderer));
  if (!render) 
    return NULL;

  render->egl = c_egl_init(mgr->gbm_device);
  if (!render->egl) goto error;

  render->gl = c_gles_init();
  if (!render->gl) goto error;

  render->formats = c_egl_query_formats(render->egl, &render->format_table_entries);
  render->format_table_fd = create_format_table(render);

  shm_pool.create_buffer = on_shm_buffer_create;
  wl_shm_pool_listen(&shm_pool, render);

  linux_dmabuf.get_default_feedback = send_feedback;
  linux_dmabuf.get_surface_feedback = send_feedback;
  zwp_linux_dmabuf_v1_listen(&linux_dmabuf, mgr);

  c_wl_interface_support("wl_shm", on_wl_shm_bind, render);
  
  if (render->egl->ext_support.EXT_image_dma_buf_import)
    c_wl_interface_support("zwp_linux_dmabuf_v1", NULL, NULL);

  return render;

error:
  c_renderer_free(render);
  return NULL;
}

void c_renderer_free(struct c_renderer *render) {
  if (render->egl) c_egl_free(render->egl);
  if (render->gl)  c_gles_free(render->gl);

  if (render->formats)    free(render->formats);

  free(render);
}
