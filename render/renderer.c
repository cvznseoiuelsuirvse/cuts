#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <gbm.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "output/drm/util.h"
#include "render/renderer.h"
#include "render/gl/egl.h"
#include "render/gl/gles.h"
#include "render/framebuffer.h"
#include "compositor/scene.h"

#include "util/log.h"
#include "util/shm.h"
#include "util/malloc.h"


#define VERT_POS_TOP_LEFT(vp)     vp.tl_x, -(vp.tl_y)
#define VERT_POS_BOTTOM_LEFT(vp)  vp.bl_x, -(vp.bl_y)
#define VERT_POS_TOP_RIGHT(vp)    vp.tr_x, -(vp.tr_y)
#define VERT_POS_BOTTOM_RIGHT(vp) vp.br_x, -(vp.br_y)

#define VERT_TOP_LEFT(vp)     VERT_POS_TOP_LEFT(vp),     0.0f, 0.0f
#define VERT_BOTTOM_LEFT(vp)  VERT_POS_BOTTOM_LEFT(vp),  0.0f, 1.0f
#define VERT_BOTTOM_RIGHT(vp) VERT_POS_BOTTOM_RIGHT(vp), 1.0f, 1.0f
#define VERT_TOP_RIGHT(vp)    VERT_POS_TOP_RIGHT(vp),    1.0f, 0.0f

#define VERTS(vp)                                                          \
  {                                                                            \
      VERT_TOP_LEFT(vp),     VERT_BOTTOM_LEFT(vp),                     \
      VERT_BOTTOM_RIGHT(vp), VERT_TOP_LEFT(vp),                        \
      VERT_BOTTOM_RIGHT(vp), VERT_TOP_RIGHT(vp),                       \
  }

struct vert_pos {
	float tl_x, tl_y;
	float bl_x, bl_y;
	float br_x, br_y;
	float tr_x, tr_y;
};

#define GL_COLOR_VEC4(v) v[0], v[1], v[2], v[3]

static void clear_color(float color[4]) {
  glClearColor(GL_COLOR_VEC4(color));
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

static void render_quad(struct c_renderer *render, struct c_output *output, struct c_scene_quad *quad, GLuint gl_texture) {
	struct c_gles *gl = render->gl;
	struct c_output_mode *mode = output->current_mode;

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
      mode->width,
      mode->height
    );

  float verts[] = VERTS(vp);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

  GLint tex_loc = glGetUniformLocation(gl->program, "tex");
  GLint border_size_loc = glGetUniformLocation(gl->program, "border_size");
  GLint border_color_loc = glGetUniformLocation(gl->program, "border_color");
  GLint draw_border_loc = glGetUniformLocation(gl->program, "draw_border");
  GLint uv_offset_loc = glGetUniformLocation(gl->program, "uv_offset");
  GLint uv_scale_loc = glGetUniformLocation(gl->program, "uv_scale");

#define border_color_args quad->border_color[0], quad->border_color[1], quad->border_color[2], quad->border_color[3]

	glUniform1i(tex_loc, 0);
  glUniform1i(draw_border_loc, quad->border_width > 0);
  glUniform2f(border_size_loc, (float)quad->border_width / quad->width, (float)quad->border_width / quad->height);
  glUniform4f(border_color_loc, border_color_args);

  glUniform2f(uv_offset_loc, quad->uv_offset[0], quad->uv_offset[1]);
  glUniform2f(uv_scale_loc, quad->uv_scale[0], quad->uv_scale[1]);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

static int import_shm(struct c_renderer *render, struct c_wl_buffer *buf) {
	struct c_shm *shm = buf->shm;
	c_gles_texture_from_shm(shm, buf->width / buf->scale, buf->height / buf->scale);
	return 0;
}

static int import_dmabuf(struct c_renderer *render, struct c_wl_buffer *buf) {
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

static GLuint ensure_imported(struct c_renderer *render, struct c_wl_buffer *buf) {
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

int c_renderer_create_format_table(struct c_renderer *render) {
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

static void *on_wl_shm_bind(struct c_wl_connection *conn, c_wl_object_id new_id, c_wl_uint version, void *userdata) {
  struct c_renderer *render = userdata;

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

static int get_dev_id(int drm_fd, dev_t *dev_id) {
	struct stat stat;
	if (fstat(drm_fd, &stat) != 0) {
    c_log_errno(C_LOG_ERROR, "failed to call fstat on DRM fd");
		return -1;
  }

	*dev_id = stat.st_rdev;
  return 0;
}

static void *on_linux_dmabuf_bind(struct c_wl_connection *conn, c_wl_object_id new_id, c_wl_uint version, void *userdata) {
  struct c_output_manager *mgr = userdata;
  struct c_wl_linux_dmabuf_ctx *ctx = c_malloc(sizeof(*ctx));

  if (!ctx) {
    c_log(C_LOG_ERROR, "failed to allocate c_wl_linux_dmabuf_ctx");
    return NULL;
  }

  if (get_dev_id(mgr->drm_fd, &ctx->drm_dev_id) == -1)  {
    c_log_errno(C_LOG_ERROR, "failed to get drm rdev"); 
    goto error;
  }

  ctx->ft_fd = c_renderer_create_format_table(mgr->renderer);
  if (ctx->ft_fd == -1) {
    c_log(C_LOG_ERROR, "failed to create format table");
    goto error;
  }

  ctx->n_ft_entries = mgr->renderer->n_formats;

  return ctx;

error:
  c_free(ctx);
  return NULL;
}

void c_renderer_draw(struct c_renderer *render, struct c_output *output,
                     struct c_scene_quad quads[C_SCENE_MAX_WINDOWS],
                     size_t quad_n, float backgroup[4]) {
  struct c_framebuffer *back_buffer = output->swapchain.buffers[output->swapchain.front ^ 1];

  glBindFramebuffer(GL_FRAMEBUFFER, back_buffer->fbo);
  clear_color(backgroup);

	for (size_t i = 0; i < quad_n; i++) {
    struct c_wl_buffer *buf = quads[i].buffer;
		GLuint tex = ensure_imported(render, buf);
		if (tex == 0) {
      c_log(C_LOG_WARNING, "failed to import %s buffer#%d", buf->type == C_WL_BUFFER_DMA ? "DMA" : "SHM", buf->id);
      continue;
    }
		render_quad(render, output, &quads[i], tex);
	}

  glFlush();
  glFinish();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  output->need_redraw = 0;
}

static int destroy_buffer(struct c_renderer *render, struct c_wl_buffer *buf) {
	if (buf->type == C_WL_BUFFER_DMA) {
		eglDestroyImage(render->egl->display, buf->dma->image);
		glDeleteTextures(1, &buf->dma->texture);
	} else if (buf->type == C_WL_BUFFER_SHM) {
		glDeleteTextures(1, &buf->shm->texture);
	}

	c_free(buf->dma);
	buf->dma = NULL;

	return 0;
}


static void on_buffer_destroy(struct c_wl_buffer *buffer, void *userdata) {
  struct c_renderer *render = userdata;
  destroy_buffer(render, buffer);
}

struct c_renderer *c_renderer_init(struct c_output_manager *mgr, struct c_wl_display *display) {
  struct c_renderer *render = calloc(1, sizeof(struct c_renderer));
  if (!render) 
    return NULL;

  render->egl = c_egl_init(mgr->gbm_device);
  if (!render->egl) goto error;

  render->gl = c_gles_init();
  if (!render->gl) goto error;

  render->formats = c_egl_query_formats(render->egl, &render->n_formats);
  render->wl_formats = malloc(sizeof(uint32_t) * render->n_formats);

  for (size_t i = 0; i < render->n_formats; i++) {
    uint32_t wl_format = drm_to_wl_shm_format(render->formats[i].drm_format);
    render->wl_formats[i] = wl_format;
  }

  struct c_wl_display_listener dpy_listeners = {
    .on_buffer_destroy = on_buffer_destroy,
  };

  c_wl_display_add_listener(display, &dpy_listeners, render);
  c_wl_display_add_supported_interface(display, "wl_shm", on_wl_shm_bind, render);
  c_wl_display_add_supported_interface(display, "zwp_linux_dmabuf_v1", on_linux_dmabuf_bind, mgr);

  return render;

error:
  c_renderer_free(render);
  return NULL;
}

void c_renderer_free(struct c_renderer *render) {
  if (render->egl) c_egl_free(render->egl);
  if (render->gl)  c_gles_free(render->gl);

  if (render->formats)    free(render->formats);
  if (render->wl_formats) free(render->wl_formats);

  free(render);
}
