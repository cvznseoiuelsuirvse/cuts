#ifndef CUTS_RENDER_TYPES_H
#define CUTS_RENDER_TYPES_H

#include <stdint.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

enum c_render_buffer_type {
  C_BUFFER_RAW,
  C_BUFFER_DMA,
};

struct c_format {
	uint32_t drm_format;
	uint64_t modifier;
	uint32_t n_planes;
	uint32_t max_width;
	uint32_t max_height;
};

struct c_dmabuf_plane {
	int fd;
	int stride;
	int offset;
};

struct c_dmabuf {
	uint32_t width, height;
	uint32_t drm_format;
	uint64_t modifier;
	uint32_t n_planes;
	struct c_dmabuf_plane planes[4];
	EGLImageKHR image;
	struct c_gles_texture *texture;
};

struct c_rawbuf {
	uint32_t width, height;
	uint8_t **base_ptr;
	uint32_t format;
	int stride;
	int offset;
	struct c_gles_texture *texture;
};

#endif
