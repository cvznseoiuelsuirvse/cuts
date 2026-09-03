#ifndef CUTS_WAYLAND_TYPES_H
#define CUTS_WAYLAND_TYPES_H

#include <stdint.h>
#include <sys/types.h>

#define C_WL_REQUEST __attribute__((weak)) int
#define C_WL_EVENT   int
#define C_WL_BASE                                                              \
  struct c_wl_object *obj;                                                     \
  struct c_callback *cb;

#define BUFFER_SIZE 2048
#define HEADER_SIZE 8
#define STRING_SIZE (BUFFER_SIZE - HEADER_SIZE - 4) // 4 (string prefix)

#define c_wl_fixed_to_double(v) (double)((v) / 256.0f)
#define c_wl_fixed_from_double(v) (c_wl_fixed)((v) * 256.0f)

#define c_wl_objects_for_each(conn, obj) \
	__attribute__((unused)) uint64_t __key; \
	c_map_for_each(c_wl_connection_get_objects(conn), __key, obj) \

typedef int32_t 		c_wl_int;
typedef uint32_t		c_wl_uint;
typedef int32_t	    c_wl_fixed;
typedef uint32_t		c_wl_object_id;
typedef uint32_t		c_wl_new_id;
typedef const char  *c_wl_string;
typedef int     	 	c_wl_fd;
typedef uint32_t 		c_wl_enum;

typedef struct c_wl_array {
	c_wl_uint  size;
	void      *data;
} c_wl_array;

struct c_wl_object {
	c_wl_object_id id;
  uint32_t version;
  struct c_wl_connection *conn;
	const struct c_wl_interface *iface;
  struct c_callback *cb;
  void *data;
};

typedef union c_wl_arg {
	c_wl_int 	  i;
	c_wl_uint   u;
	c_wl_fixed  f;
	c_wl_new_id n;
	c_wl_array  *a;
	c_wl_fd     F;
	c_wl_enum   e;
	c_wl_object_id o;
	char      s[STRING_SIZE];
} c_wl_arg, *c_wl_args;

#endif
