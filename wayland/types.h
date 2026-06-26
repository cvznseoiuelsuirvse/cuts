#ifndef CUTS_WAYLAND_TYPES_H
#define CUTS_WAYLAND_TYPES_H

#include <stdint.h>
#include <sys/types.h>

#include "util/list.h"

#define C_WL_REQUEST __attribute__((weak)) int
#define C_WL_EVENT int

#define C_WL_MAX_INTERFACES 2048
#define C_WL_HEADER_SIZE 8
#define C_WL_BUFFER_SIZE 4096
#define C_WL_STRING_SIZE (C_WL_BUFFER_SIZE - C_WL_HEADER_SIZE - 4) // 4 -> string prefix size

typedef int32_t 	  		c_wl_int;
typedef uint32_t	 		c_wl_uint;
typedef uint32_t			c_wl_fixed;
typedef uint32_t			c_wl_object_id;
typedef uint32_t			c_wl_new_id;
typedef const char	   	   *c_wl_string;
typedef int     	    	c_wl_fd;
typedef uint32_t 			c_wl_enum;

typedef struct c_wl_array {
	c_wl_uint  size;
	void      *data;
} c_wl_array;


struct c_wl_formats {
	size_t    n_formats;
	uint32_t *formats;
};

struct c_wl_shm_pool {
	int 	     fd;
	uint8_t	  *ptr;
	uint32_t   size;

	size_t    n_supported_formats;
	uint32_t *supported_formats;
};

enum c_wl_buffer_type {
	C_WL_BUFFER_DMA = 1,
	C_WL_BUFFER_SHM,
};

struct c_wl_buffer {
	c_wl_object_id id;
  struct c_wl_connection *conn;
  c_wl_int       scale;

	c_wl_uint width;
	c_wl_uint height;

	enum c_wl_buffer_type type;	
	union {
		struct c_dmabuf *dma;
		struct c_shm    *shm;
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
	c_wl_object_id          id;
	struct c_wl_connection *conn;


  c_wl_object_id frame_id;
  c_wl_object_id frames[4];
  size_t         n_frames;

	enum c_wl_surface_roles role;
	struct {
		c_wl_int width,  x;
		c_wl_int height, y;
		int		 damaged;
	} damage;

  struct c_xdg_surface *xdg_surface;

  struct {
    struct c_wl_subsurface *surface;
    c_list *children;
  } sub;

	struct c_wl_region opaque;
	struct c_wl_region input;

	struct c_wl_buffer 	*pending;
	struct c_wl_buffer 	*active;
};

struct c_wl_subsurface {
  c_wl_object_id id;

  c_wl_int x;
  c_wl_int y;
  int      sync;

  struct c_wl_surface *surface;
  struct c_wl_surface *parent;
};

struct c_xdg_positioner {
  c_wl_object_id id;

  c_wl_int x; 
  c_wl_int y; 

  c_wl_int width;
  c_wl_int height;

  c_wl_uint gravity;
  c_wl_uint anchor;
  c_wl_uint constraint_adjustment;

  struct {
    c_wl_int x; 
    c_wl_int y; 

    c_wl_int width;
    c_wl_int height;
  } anchor_rect;
};

struct c_xdg_surface {
  c_wl_object_id  id;

  c_wl_int x;
  c_wl_int y;
  c_wl_int width;
  c_wl_int height;
  
  struct {
    c_wl_object_id id;

    c_wl_int max_width;
    c_wl_int max_height;
    c_wl_int min_width;
    c_wl_int min_height;

    char *title;
    char *app_id;

  } toplevel;

  struct c_wl_surface *surface;

  struct {
    c_wl_object_id id;
    struct c_xdg_positioner positioner;
  } popup;

  c_list *children;
  struct c_xdg_surface *parent;
};

struct c_wl_linux_dmabuf_ctx {
	dev_t    drm_dev_id;	
	int      ft_fd;
	size_t   n_ft_entries;
};

double c_wl_fixed_to_double(c_wl_fixed f);
c_wl_fixed c_wl_fixed_from_double(double d);

#endif
