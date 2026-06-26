#include <assert.h>
#include <unistd.h>
#include <inttypes.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "render/render.h"
#include "render/gl/egl.h"
#include "render/gl/gles.h"
#include "compositor/scene.h"

#include "util/log.h"
#include "util/shm.h"
#include "util/malloc.h"

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

#define DMA_VERTS(vp)                                                          \
  {                                                                            \
      DMA_VERT_TOP_LEFT(vp),     DMA_VERT_BOTTOM_LEFT(vp),                     \
      DMA_VERT_BOTTOM_RIGHT(vp), DMA_VERT_TOP_LEFT(vp),                        \
      DMA_VERT_BOTTOM_RIGHT(vp), DMA_VERT_TOP_RIGHT(vp)                        \
  }

#define SHM_VERTS(vp)                                                          \
  {                                                                            \
      SHM_VERT_TOP_LEFT(vp),     SHM_VERT_BOTTOM_LEFT(vp),                     \
      SHM_VERT_BOTTOM_RIGHT(vp), SHM_VERT_TOP_LEFT(vp),                        \
      SHM_VERT_BOTTOM_RIGHT(vp), SHM_VERT_TOP_RIGHT(vp),                       \
  }

struct vert_pos {
	float tl_x, tl_y;
	float bl_x, bl_y;
	float br_x, br_y;
	float tr_x, tr_y;
};

static int __needs_redraw = 0;
float __gl_bg_color[4] = {0.4f, 0.4f, 0.4f, 1.0f};

#define GL_COLOR_VEC4(v) v[0], v[1], v[2], v[3]

static void clear_color() {
  glClearColor(GL_COLOR_VEC4(__gl_bg_color));
  glClear(GL_COLOR_BUFFER_BIT);
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

static void render_quad(struct c_render *render, struct c_scene_quad *quad, GLuint gl_texture) {
	struct c_gles *gl = render->gl;
	struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(render->drm);

	glUseProgram(gl->program);
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  
  glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl_texture);

	struct vert_pos vp = {0};
	create_verts(
      quad->width,
      quad->height,
      quad->x,
      quad->y,
      &vp, 
      preferred_mode->width,
      preferred_mode->height
    );


	if (quad->buffer->type == C_WL_BUFFER_DMA) {
		float dma_vertices[] = DMA_VERTS(vp);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(dma_vertices), dma_vertices);
	} else {
		float shm_vertices[] = SHM_VERTS(vp);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(shm_vertices), shm_vertices);
	}

  GLint tex_loc = glGetUniformLocation(gl->program, "tex");
  GLint border_size_loc = glGetUniformLocation(gl->program, "border_size");
  GLint border_color_loc = glGetUniformLocation(gl->program, "border_color");

#define border_color_args quad->border_color[0], quad->border_color[1], quad->border_color[2], quad->border_color[3]

  c_log(C_LOG_DEBUG, "%f %f %f %f", border_color_args);
	glUniform1i(tex_loc, 0);
  glUniform2f(border_size_loc, (float)quad->border_width / quad->width, (float)quad->border_width / quad->height);
  glUniform4f(border_color_loc, border_color_args);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

static int import_shm(struct c_render *render, struct c_wl_buffer *buf) {
	struct c_shm *shm = buf->shm;
	c_gles_texture_from_shm(shm, buf->width / buf->scale, buf->height / buf->scale);
	return 0;
}

static int import_dmabuf(struct c_render *render, struct c_wl_buffer *buf) {
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
	}

	struct c_dmabuf_params params = {
		.width      = buf->width / buf->scale,
		.height     = buf->height / buf->scale,
		.modifier   = dmabuf->modifier,
		.drm_format = dmabuf->drm_format,
		.n_planes   = dmabuf->n_planes,
		.planes     = dmabuf->planes,
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

static GLuint ensure_imported(struct c_render *render, struct c_wl_buffer *buf) {
	if (buf->type == C_WL_BUFFER_DMA) {
		if (buf->dma->texture == 0)
			if (import_dmabuf(render, buf) < 0) return 0;
		return buf->dma->texture;
	} else if (buf->type == C_WL_BUFFER_SHM) {
		if (buf->shm->texture == 0)
			if (import_shm(render, buf) < 0) return 0;
		return buf->shm->texture;
	}

	assert(0);
}

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *userdata) {
  struct c_render *render = userdata;

  render->swapchain.front ^= 1;
  render->drm->connector.waiting_for_flip = 0;
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
  struct c_wl_linux_dmabuf_ctx *ctx = c_malloc(sizeof(*ctx));

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
  c_free(ctx);
  return NULL;

}

static void *on_wl_shm_bind(struct c_wl_connection *conn, c_wl_object_id new_id, c_wl_uint version, void *userdata) {
  struct c_render *render = userdata;

  struct c_wl_formats *wl_formats = c_malloc(sizeof(*wl_formats));
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

static int c_render_handle_event(struct c_render *render) {
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


void c_render_draw_scene(struct c_render *render) {
  if (render->drm->connector.waiting_for_flip) {
    __needs_redraw = 1;
    return;
  }

  struct c_render_buffer *back_buffer = render->swapchain.buffers[render->swapchain.front ^ 1];

  glBindFramebuffer(GL_FRAMEBUFFER, back_buffer->fbo);
  clear_color();

	struct c_scene_quad quads[C_SCENE_MAX_WINDOWS];
	int n = c_scene_collect(quads, C_SCENE_MAX_WINDOWS);
	for (int i = 0; i < n; i++) {
    struct c_wl_buffer *buf = quads[i].buffer;
		GLuint tex = ensure_imported(render, buf);
		if (tex == 0) {
      c_log(C_LOG_WARNING, "failed to import %s buffer#%d", buf->type == C_WL_BUFFER_DMA ? "DMA" : "SHM", buf->id);
      continue;
    }
		render_quad(render, &quads[i], tex);
	}

  glFlush();
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  drmModePageFlip(render->drm->fd, render->drm->connector.crtc_id,
                  back_buffer->drm_fb_id, DRM_MODE_PAGE_FLIP_EVENT, render);

  render->drm->connector.waiting_for_flip = 1;
  __needs_redraw = 0;
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

static void on_surface_commit_cb(struct c_wl_surface *surface, void *userdata) {
  c_render_draw_scene(userdata);
}

static void on_surface_destroy_cb(struct c_wl_surface *surface, void *userdata) {
  c_render_draw_scene(userdata);
}

static int destroy_buffer(struct c_render *render, struct c_wl_buffer *buf) {
	if (buf->type == C_WL_BUFFER_DMA) {
		eglDestroyImage(render->egl->display, buf->dma->image);
		glDeleteTextures(1, &buf->dma->texture);
	} else if (buf->type == C_WL_BUFFER_SHM) {
		glDeleteTextures(1, &buf->shm->texture);
	} else return 0;

	c_free(buf->dma);
	buf->dma = NULL;
	return 0;
}


static void on_buffer_destroy(struct c_wl_buffer *buffer, void *userdata) {
  struct c_render *render = userdata;
  destroy_buffer(render, buffer);
}

C_EVENT_CALLBACK render_callback(struct c_event_loop *loop, int fd, void *userdata) {
  if (c_render_handle_event((struct c_render *)userdata) == -1) 
    return C_EVENT_ERROR_FATAL;

  if (__needs_redraw)
    c_render_draw_scene((struct c_render *)userdata);

  return C_EVENT_OK;
}

struct c_render *c_render_init(struct c_event_loop *loop, struct c_wl_display *display, struct c_drm *drm) {
  struct c_render *render = calloc(1, sizeof(struct c_render));
  if (!render) 
    return NULL;

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

  c_scene_init(preferred_mode->width, preferred_mode->height);

  glViewport(0, 0, preferred_mode->width, preferred_mode->height);
  c_render_draw_scene(render);
  c_event_loop_add(loop, drm->fd, render_callback, render);

  struct c_wl_display_listener dpy_listeners = {
    .on_surface_commit = on_surface_commit_cb,
    .on_surface_destroy = on_surface_destroy_cb,

    .on_subsurface_destroy = on_surface_destroy_cb,
    .on_buffer_destroy = on_buffer_destroy,
  };

  c_wl_display_add_listener(display, &dpy_listeners, render);

  c_wl_display_add_supported_interface(display, "wl_shm", on_wl_shm_bind, render);
  c_wl_display_add_supported_interface(display, "zwp_linux_dmabuf_v1", on_linux_dmabuf_bind, render);

  return render;

error:
  c_render_free(render);
  return NULL;
}

void c_render_free(struct c_render *render) {
  if (render->swapchain.buffers[0])
    c_render_buffer_destroy(render, render->swapchain.buffers[0]);

  if (render->swapchain.buffers[1])
    c_render_buffer_destroy(render, render->swapchain.buffers[1]);


  if (render->egl)             c_egl_free(render->egl);
  if (render->gl)              c_gles_free(render->gl);

  if (render->formats)        free(render->formats);
  if (render->wl_formats)     free(render->wl_formats);

  c_scene_destroy();
  free(render);
}
