#ifndef CUTS_RENDER_RENDER_H
#define CUTS_RENDER_RENDER_H

#include <stdint.h>
#include <stdio.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#include "wayland/display.h"
#include "wayland/server.h"

#include "backend/drm/drm.h"

#include "render/window.h"

#define C_DMABUF_MAX_PLANES 4

struct c_format {
	uint32_t drm_format;
	uint64_t modifier;
	uint32_t n_planes;
	uint32_t max_width;
	uint32_t max_height;
	int 	 supports_disjoint;
};

struct c_dmabuf_plane {
    int fd;
    int stride;
    int offset;
};

struct c_dmabuf {
	int flags;

    uint32_t drm_format;
    uint64_t modifier;

	uint32_t 			  n_planes;
	struct c_dmabuf_plane planes[C_DMABUF_MAX_PLANES];

	EGLImageKHR image;
	GLuint texture;
};

struct c_shm {
	uint8_t    *ptr;
    uint32_t   format; // WL_SHM_FORMAT
    int        stride;
    int        offset;
	GLuint texture;
};

struct c_dmabuf_params {
	uint32_t width, height;
	uint32_t drm_format;
	uint64_t modifier;
	uint32_t n_planes;
	struct c_dmabuf_plane *planes;
};

struct c_shm_params {
    int fd;
	  uint32_t width, height;
    uint32_t format;
    int      stride;
    int      offset;
};

struct c_render_listener {
	int (*on_window_new)	 (struct c_window *, void *);
	int (*on_window_close)	 (struct c_window *, void *);
};

struct c_render {
	struct c_drm  *drm;
	struct gbm_surface *gbm_surface;
	struct gbm_bo 	   *gbm_bo;
	struct gbm_bo 	   *gbm_bo_next;

	struct c_egl *egl;

	struct {
		size_t n_entries;
		struct c_format *entries;
		c_list *wl_shm_formats;
	} formats;

	c_list *listeners;
	c_map  *surfaces;

};

struct c_render *c_render_init(struct c_wl_display *dpy, struct c_drm *drm);
void c_render_free(struct c_render *render);
void c_render_add_listener(struct c_render *render, struct c_render_listener *listener, void *userdata);

// int c_render_import_shm(struct c_render *render, struct c_wl_buffer *buf);
// int c_render_destroy_shm(struct c_render *render, struct c_shm *buf);

// int c_render_import_dmabuf(struct c_render *render, struct c_wl_buffer *buf);
// int c_render_destroy_dmabuf(struct c_render *render, struct c_dmabuf *buf);

int c_render_get_ft_fd(struct c_render *render);

void c_render_redraw(struct c_render *render);
#endif
