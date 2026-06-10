#ifndef CUTS_RENDER_TYPES_H
#define CUTS_RENDER_TYPES_H

#include <stdint.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#define C_DMABUF_MAX_PLANES 4

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
	uint32_t drm_format;
	uint64_t modifier;

	uint32_t              n_planes;
	struct c_dmabuf_plane planes[C_DMABUF_MAX_PLANES];

	EGLImageKHR image;
	GLuint      texture;
};

struct c_shm {
	uint8_t  **base_ptr;
	uint32_t   format;
	int        stride;
	int        offset;
	GLuint     texture;
};

struct c_dmabuf_params {
	uint32_t width, height;
	uint32_t drm_format;
	uint64_t modifier;
	uint32_t n_planes;
	struct c_dmabuf_plane *planes;
};

struct c_shm_params {
	int      fd;
	uint32_t width, height;
	uint32_t format;
	int      stride;
	int      offset;
};

#endif
