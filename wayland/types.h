#ifndef CUTS_WAYLAND_TYPES_H
#define CUTS_WAYLAND_TYPES_H

#include <stdint.h>
#include <sys/types.h>

#define C_WL_REQUEST __attribute__((weak)) int
#define C_WL_EVENT int

#define C_WL_FIXED_TO_DOUBLE(v) (double)((v) / 256.0f)
#define C_WL_FIXED_FROM_DOUBLE(v) (c_wl_fixed)((v) * 256.0f)

typedef int32_t 	  		c_wl_int;
typedef uint32_t	 		c_wl_uint;
typedef int32_t			c_wl_fixed;
typedef uint32_t			c_wl_object_id;
typedef uint32_t			c_wl_new_id;
typedef const char	   	   *c_wl_string;
typedef int     	    	c_wl_fd;
typedef uint32_t 			c_wl_enum;

typedef struct c_wl_array {
	c_wl_uint  size;
	void      *data;
} c_wl_array;


#endif
