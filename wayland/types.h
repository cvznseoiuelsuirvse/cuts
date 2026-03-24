#ifndef CUTS_WAYLAND_TYPES_H
#define CUTS_WAYLAND_TYPES_H

#include <stdint.h>
#include <sys/types.h>

#include "util/list.h"

#define C_WL_REQUEST __attribute__((weak)) int
#define C_WL_EVENT int

typedef int32_t 	  		c_wl_int;
typedef uint32_t	 		c_wl_uint;
typedef float 				c_wl_fixed;
typedef uint32_t			c_wl_object_id;
typedef uint32_t			c_wl_new_id;
typedef const char	   	   *c_wl_string;
typedef int     	    	c_wl_fd;
typedef uint32_t 			c_wl_enum;

typedef struct c_wl_array {
	c_wl_uint  size;
	void      *data;
} c_wl_array;


struct c_wl_shm_pool {
	int 	   fd;
	uint8_t	  *ptr;
	uint32_t   size;
	c_list 	  *supported_formats;
};

enum c_wl_buffer_type {
	C_WL_BUFFER_DMA = 1,
	C_WL_BUFFER_SHM,
};

struct c_wl_buffer {
	c_wl_object_id id;

	c_wl_uint width;
	c_wl_uint height;

	enum c_wl_buffer_type type;	
	union {
		struct c_dmabuf *dma;
		struct c_shm *shm;
	};
};

struct c_wl_region {
	c_wl_int width,  x;
	c_wl_int height, y;
};

enum c_wl_surface_roles {
	C_WL_SURFACE_ROLE_XDG_TOPLEVEL = 1,
	C_WL_SURFACE_ROLE_XDG_POPUP,
	C_WL_SURFACE_ROLE_SUBSURFACE,
};


struct c_wl_surface {
	c_wl_object_id  id;
	enum c_wl_surface_roles role;

	struct {
		c_wl_int width,  x;
		c_wl_int height, y;
		int		 damaged;
	} damage;

	struct {
		c_wl_object_id  surface_id;
		c_wl_object_id  toplevel_id;
		char title[256];
		char app_id[256];
		c_wl_uint 		serial;

		struct c_wl_surface *parent;
		c_list *children;
	} xdg;

	struct {
		c_wl_object_id  surface_id;
		c_wl_int x, y;
		int sync;

		struct c_wl_surface *parent;
		c_list *children;
	} sub;

	struct c_wl_region opaque;
	struct c_wl_region input;

	struct c_wl_buffer 	*pending;
	struct c_wl_buffer 	*active;

	struct c_wl_connection *conn;
};

struct c_wl_linux_dmabuf_ctx {
	dev_t  drm_dev_id;	
	int    ft_fd;
	size_t n_ft_entries;
};

#endif
