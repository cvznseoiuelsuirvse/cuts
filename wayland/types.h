#ifndef CUTS_WAYLAND_TYPES_H
#define CUTS_WAYLAND_TYPES_H

#include <stdint.h>

#define C_WL_REQUEST __attribute__((weak)) int
#define C_WL_EVENT int

typedef long 		  		c_wl_int;
typedef unsigned long 		c_wl_uint;
typedef float 				c_wl_fixed;
typedef unsigned long		c_wl_object_id;
typedef unsigned long		c_wl_new_id;
typedef const char	   	   *c_wl_string;
typedef int     	    	c_wl_fd;
typedef int 				c_wl_enum;
typedef unsigned short      c_wl_u16;
typedef unsigned long       c_wl_u32;
typedef unsigned long long  c_wl_u64;

typedef struct c_wl_array {
	c_wl_uint  size;
	void      *data;
} c_wl_array;


struct c_wl_shm {
	c_wl_object_id id;
	uint8_t		  *buffer;
	c_wl_int	   size;
	struct c_shm  *shm;
};

struct c_wl_dmabuf {
	c_wl_object_id id;
	int            used;
	c_wl_int       flags;
	struct c_dmabuf *dma;
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
		struct c_wl_dmabuf *dma;
		struct c_wl_shm *shm;
	};
};

struct c_wl_region {
	c_wl_int width,  x;
	c_wl_int height, y;
};

enum c_wl_surface_roles {
	C_WL_SURFACE_TOPLEVEL = 1,
	C_WL_SURFACE_POPUP,
};

struct c_wl_surface {
	c_wl_object_id  id;
	c_wl_uint 		serial;
	enum c_wl_surface_roles role;

	struct {
		c_wl_int width,  x;
		c_wl_int height, y;
		int		 damaged;
	} damage;

	struct {
		c_wl_object_id  surface_id;
		c_wl_object_id  toplevel_id;
		c_wl_int width,  max_width,  min_width,  x;
		c_wl_int height, max_height, min_height, y;
		char title[256];
		char app_id[256];
	} xdg_state;

	struct c_wl_region opaque;
	struct c_wl_region input;

	struct c_wl_buffer 	*pending;
	struct c_wl_buffer 	*active;

	struct c_wl_connection *conn;
};

#endif
