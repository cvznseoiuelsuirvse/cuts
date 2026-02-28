#define _GNU_SOURCE
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include "wayland/types/wayland.h"

#include "wayland/server.h"
#include "wayland/error.h"

#include "util/helpers.h"
#include "util/log.h"


static enum wl_shm_format_enum supported_formats[] = {
  WL_SHM_FORMAT_ARGB8888,
  WL_SHM_FORMAT_XRGB8888,
};

static void damage_surface(struct c_wl_surface *surface, union c_wl_arg *args) {
  c_wl_int x =      args[1].i;
  c_wl_int y =      args[2].i;
  c_wl_int width =  args[3].i;
  c_wl_int height = args[4].i;

  surface->damage.x = x;
  surface->damage.y = y;
  surface->damage.width = width;
  surface->damage.height = height;
  surface->damage.damaged = 1;

}


int wl_display_get_registry(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_new_id object_id = args[1].n;
  struct c_wl_object *c_wl_registry;
  C_WL_CHECK_IF_NOT_REGISTERED(object_id, c_wl_registry);

  c_wl_object_add(conn, object_id, (struct c_wl_interface *)c_wl_interface_get("wl_registry"), 0);

  const struct c_wl_interface *ifaces[] = {
    c_wl_interface_get("wl_compositor"),
    c_wl_interface_get("wl_shm"),
    c_wl_interface_get("xdg_wm_base"),
    c_wl_interface_get("zwp_linux_dmabuf_v1"),
  };

  for (size_t i = 0; i < LENGTH(ifaces); i++) {
    wl_registry_global(conn, object_id, i+1, ifaces[i]->name, ifaces[i]->version);
  }

  return 0;
};

int wl_display_sync(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  return 0;
}

int wl_registry_bind(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_new_id new_id = args[4].n;
  struct c_wl_object *c_wl_object;
  C_WL_CHECK_IF_NOT_REGISTERED(new_id, c_wl_object);

  c_wl_string interface_name = args[2].s;
  const struct c_wl_interface *interface = c_wl_interface_get(interface_name);
  if (!interface) return c_wl_error_set(new_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "interface not supported");

  c_wl_object_add(conn, new_id, interface, 0);

  if (STREQ(interface_name, "wl_shm")) {
    for (size_t i = 0; i < LENGTH(supported_formats); i++) {
      wl_shm_format(conn, new_id, supported_formats[i]);
    }
  }

  return 0;
}

int wl_shm_create_pool(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_object_id wl_shm_id = args[0].o;
  c_wl_new_id c_wl_shm_pool_id = args[1].n;
  struct c_wl_object *c_wl_shm_pool;
  C_WL_CHECK_IF_NOT_REGISTERED(c_wl_shm_pool_id, c_wl_shm_pool);

  c_wl_fd pool_fd = args[2].F;
  c_wl_int buffer_size = args[3].i;

  struct c_wl_shm *shm = calloc(1, sizeof(struct c_wl_shm));
  shm->fd = pool_fd;

  uint8_t *buffer = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, pool_fd, 0);
  if (buffer == MAP_FAILED) {
    c_wl_uint error_code = 0;
    switch (errno) {
      case EBADF:
      case EACCES:    
        error_code = WL_SHM_ERROR_INVALID_FD;
        break;

      case ENOMEM:
        error_code = WL_DISPLAY_ERROR_NO_MEMORY;
        break;

      default:
        error_code = WL_DISPLAY_ERROR_IMPLEMENTATION;
        break;
    }

    perror("mmap");
    free(shm);
    return c_wl_error_set(wl_shm_id, error_code, "failed to mmap");
  }

  shm->buffer = buffer;
  shm->size = buffer_size;

  c_wl_object_add(conn, c_wl_shm_pool_id, c_wl_interface_get("wl_shm_pool"), shm);
  return 0;
}

int wl_shm_pool_create_buffer(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_object_id wl_shm_pool_id = args[0].o;
  c_wl_new_id c_wl_buffer_id = args[1].n;
  struct c_wl_object *c_wl_buffer;
  C_WL_CHECK_IF_NOT_REGISTERED(c_wl_buffer_id, c_wl_buffer);

  c_wl_int offset = args[2].i;
  c_wl_int width =  args[3].i;
  c_wl_int height = args[4].i;
  c_wl_int stride = args[5].i;
  enum wl_shm_format_enum format = args[6].e;

  int format_supported = 0;
  for (size_t i = 0; i < LENGTH(supported_formats); i++) {
    if (supported_formats[i] == format) {
      format_supported = 1;
      break;
    }
  }

  if (!format_supported) {
    c_wl_error_set(wl_shm_pool_id, WL_SHM_ERROR_INVALID_FORMAT, "format not supported");
    return -1;
  }

  if (offset % 4 != 0) {
    c_wl_error_set(wl_shm_pool_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid offset");
    return -1;
  }

  struct c_wl_object *c_wl_shm_pool = c_wl_object_get(conn, wl_shm_pool_id);
  struct c_wl_shm *shm = c_wl_shm_pool->data;

  uint32_t region_size = (uint32_t)stride * height;
  if ((region_size > shm->size) || (offset > shm->size - region_size)) {
    c_wl_error_set(wl_shm_pool_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "requested region too big");
    return -1;
  }

  struct c_wl_buffer *buffer = calloc(1, sizeof(struct c_wl_buffer));
  buffer->offset = offset;
  buffer->width = width;
  buffer->height = height;
  buffer->stride = stride;
  buffer->format = format;
  buffer->shm = shm;

  c_wl_object_add(conn, c_wl_buffer_id, c_wl_interface_get("wl_buffer"), buffer);
  return 0;
}

int wl_shm_pool_resize(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_object_id wl_shm_pool_id = args[0].o;
  c_wl_int new_size = args[1].i;

  struct c_wl_object *c_wl_shm_pool = c_wl_object_get(conn, wl_shm_pool_id);
  struct c_wl_shm *shm = c_wl_shm_pool->data;

  uint8_t *new_buffer = mremap(shm->buffer, shm->size, new_size, MREMAP_MAYMOVE);
  if (new_buffer == MAP_FAILED) {
    c_wl_error_set(wl_shm_pool_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to mremap: %s", strerror(errno));
    return -1;
  }

  shm->buffer = new_buffer;
  shm->size = new_size;

  return 0;
}

int wl_shm_pool_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_object_id wl_shm_pool_id = args[0].o;

  struct c_wl_object *c_wl_shm_pool = c_wl_object_get(conn, wl_shm_pool_id);
  struct c_wl_shm *shm = c_wl_shm_pool->data;

  munmap(shm->buffer, shm->size);
  free(shm);
  c_wl_object_del(conn, wl_shm_pool_id);

  return 0;
}

int wl_buffer_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_object_id wl_buffer_id = args[0].o;

  struct c_wl_object *c_wl_buffer = c_wl_object_get(conn, wl_buffer_id);
  struct c_wl_buffer *buffer = c_wl_buffer->data;
  if (buffer)
    free(buffer);
  c_wl_object_del(conn, wl_buffer_id);

  return 0;
}

int wl_compositor_create_region(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id wl_region_id = args[1].o;
  struct c_wl_object *wl_region;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_region_id, wl_region);

  struct c_wl_region *c_wl_region = calloc(1, sizeof(struct c_wl_region));
  if (!c_wl_region) {
    c_log(C_LOG_ERROR, "calloc failed");
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  c_wl_region->id = wl_region_id;
  c_wl_object_add(conn, wl_region_id, c_wl_interface_get("wl_region"), c_wl_region);
  return 0;
}

int wl_region_add(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id wl_region_id = args[0].u;
  struct c_wl_object *wl_region = c_wl_object_get(conn, wl_region_id);
  struct c_wl_region *c_wl_region = wl_region->data;

  c_wl_int x =      args[1].i;
  c_wl_int y =      args[2].i;
  c_wl_int width =  args[3].i;
  c_wl_int height = args[4].i;

  c_wl_region->x = x;
  c_wl_region->y = y;
  c_wl_region->width = width;
  c_wl_region->height = height;

  return 0;
}
int wl_region_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id wl_region_id = args[0].u;
  struct c_wl_object *wl_region = c_wl_object_get(conn, wl_region_id);
  free(wl_region->data);
  c_wl_object_del(conn, wl_region_id);
  return 0;
}


int _wl_compositor_create_surface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id wl_surface_id = args[1].o;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_surface_id, wl_surface);

  struct c_wl_surface *c_wl_surface = calloc(1, sizeof(struct c_wl_surface));
  if (!c_wl_surface) {
    c_log(C_LOG_ERROR, "calloc failed");
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  c_wl_surface->id = wl_surface_id;
  c_wl_surface->conn = conn;
  c_wl_object_add(conn, wl_surface_id, c_wl_interface_get("wl_surface"), c_wl_surface);
  return 0;
}

int wl_surface_attach(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_object *c_wl_surface = c_wl_object_get(conn, wl_surface_id);

  c_wl_object_id wl_buffer_id = args[1].o;
  struct c_wl_object *c_wl_buffer;
  C_WL_CHECK_IF_REGISTERED(wl_buffer_id, c_wl_buffer);

  struct c_wl_surface *surface = c_wl_surface->data;
  struct c_wl_buffer *buffer = c_wl_buffer->data;

  buffer->id = wl_buffer_id;
  surface->pending = buffer;

  return 0;
}

int wl_surface_damage(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_object *wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *c_wl_surface = wl_surface->data;

  damage_surface(c_wl_surface, args);

  return 0;
}

int wl_surface_damage_buffer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
    c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_object *wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *c_wl_surface = wl_surface->data;

  damage_surface(c_wl_surface, args);

  return 0;
}


int wl_surface_frame(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_new_id c_wl_callback_id = args[1].o;
  struct c_wl_object *c_wl_callback;
  C_WL_CHECK_IF_NOT_REGISTERED(c_wl_callback_id, c_wl_callback);
  if (c_wl_connection_callback_add(conn, c_wl_callback_id, args[0].o) == -1)     
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_INVALID_OBJECT, "object %d already registered", c_wl_callback_id);
  return 0;
}

int wl_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *user_data) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_object *c_wl_surface_obj = c_wl_object_get(conn, wl_surface_id);
  free(c_wl_surface_obj->data);
  c_wl_object_del(conn, wl_surface_id);
  return 0;
}

int wl_surface_set_opaque_region(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_object *c_wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *surface = c_wl_surface->data;

  c_wl_object_id wl_region_id = args[1].o;
  if (wl_region_id == 0) {
    surface->opaque = NULL;
    return 0;
  }

  struct c_wl_object *c_wl_region;
  C_WL_CHECK_IF_REGISTERED(wl_region_id, c_wl_region);

  surface->opaque = c_wl_region->data;

  return 0;
}

