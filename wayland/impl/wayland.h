#ifndef CUTS_WAYLAND_IMPL_WAYLAND_H
#define CUTS_WAYLAND_IMPL_WAYLAND_H

#include "wayland/types.h"
#include "wayland/proto/wayland.h"
#include "util/list.h"

struct c_wl_region {
	c_wl_int width,  x;
	c_wl_int height, y;
};

struct c_wl_output {
  struct c_wl_object *obj;
  struct c_output *output;
};

struct c_wl_shm_pool {
  struct c_wl_object *obj;
	int 	     fd;
	uint8_t	  *ptr;
	uint32_t   size;
};

struct c_wl_buffer {
	struct c_wl_object *obj;
  struct c_wl_shm_pool *pool;

  struct c_dmabuf *dma;
  struct c_rawbuf *shm;
};

enum c_wl_surface_roles {
	C_WL_SURFACE_ROLE_XDG_TOPLEVEL = 1,
	C_WL_SURFACE_ROLE_XDG_POPUP,
	C_WL_SURFACE_ROLE_SUBSURFACE,
	C_WL_SURFACE_ROLE_DND_ICON,
};

struct c_wl_surface {
	struct c_wl_object *obj;

  c_wl_int scale;
  c_wl_int fscale;

  enum wl_output_transform_enum transform;

  c_wl_object_id frames[8];
  size_t frames_n;

  c_wl_object_id feedbacks[8];
  size_t feedbacks_n;

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

  struct {
    struct c_wl_region pending;
    struct c_wl_region active;
  } input;

  struct {
    struct c_wl_region pending;
    struct c_wl_region active;
  } opaque;

  struct {
    struct c_wl_buffer 	*pending;
    struct c_wl_buffer 	*active;
  } buffer;

  struct c_wp_viewport *viewport;

  struct c_wl_output *output;
};

struct c_wl_subsurface {
  struct c_wl_object *obj;
  c_wl_int x, y;
  int sync;

  struct c_wl_surface *surface;
  struct c_wl_surface *parent;
};

struct c_wl_data_offer {
  struct c_wl_object *obj;

  char *mimetype;

  enum wl_data_device_manager_dnd_action_enum actions;
  enum wl_data_device_manager_dnd_action_enum preferred;

  struct c_wl_data_device *device;
  struct c_wl_data_source *source;
};

struct c_wl_data_source {
  struct c_wl_object *obj;

  const char *mimetypes[64];
  size_t mimes;

  enum wl_data_device_manager_dnd_action_enum actions;

  struct c_wl_data_device *device;

};

struct c_wl_data_device {
  struct c_wl_object *obj;

  struct c_wl_data_source *selection;
  struct c_wl_data_offer *offer;

  struct {
    struct c_wl_data_source *source;
    struct c_wl_surface *origin;
    struct c_wl_surface *icon;
  } dnd;
};

struct c_wl_pointer {
  struct c_wl_object *obj;
  struct c_wl_object *seat;
};

#endif
