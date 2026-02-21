#define _GNU_SOURCE
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "protocols/wayland.h"
#include "protocols/xdg-shell.h"

#include "wayland/server.h"
#include "wayland/error.h"

#include "util/helpers.h"


static enum wl_shm_format_enum supported_formats[] = {
  WL_SHM_FORMAT_ARGB8888,
  WL_SHM_FORMAT_XRGB8888,
};

int wl_display_get_registry(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id object_id = args[1].n;
  struct wl_object *wl_registry;
  WL_CHECK_IF_NOT_REGISTERED(object_id, wl_registry);

  wl_object_add(conn, object_id, (struct wl_interface *)wl_interface_get("wl_registry"), 0);

  const struct wl_interface *ifaces[] = {
    wl_interface_get("wl_compositor"),
    wl_interface_get("wl_shm"),
    wl_interface_get("xdg_wm_base"),
  };

  for (size_t i = 0; i < LENGTH(ifaces); i++) {
    wl_registry_global(conn, object_id, i+1, ifaces[i]->name, ifaces[i]->version);
  }

  wl_connection_callback_done(conn, 1);
  return 0;
};

int wl_display_sync(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id wl_callback_id = args[1].o;
  struct wl_object *wl_callback = wl_object_get(conn, wl_callback_id);

  if (wl_callback) { 
    struct wl_callback *c;
    c_list *_c = conn->callback_queue;
    c_list_for_each(_c, c) {
      if (c->callback_id == wl_callback_id) {
        c_list_remove_ptr(&conn->callback_queue, c);
        break;
      }
    }
    return wl_error_set(args[0].u, WL_DISPLAY_ERROR_INVALID_OBJECT, "object %d already registered", wl_callback_id);
  }

  return 0;
}

int wl_registry_bind(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id new_id = args[4].n;
  struct wl_object *wl_object;
  WL_CHECK_IF_NOT_REGISTERED(new_id, wl_object);

  wl_string interface_name = args[2].s;
  printf("%s\n", interface_name);
  const struct wl_interface *interface = wl_interface_get(interface_name);
  if (!interface) return wl_error_set(new_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "interface not supported");

  wl_object_add(conn, new_id, interface, 0);

  if (STREQ(interface_name, "wl_shm")) {
    for (size_t i = 0; i < sizeof(supported_formats) / sizeof(enum wl_shm_format_enum); i++) {
      wl_shm_format(conn, new_id, supported_formats[i]);
    }
  }

  return 0;
}

int wl_shm_create_pool(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id wl_shm_id = args[0].o;
  wl_new_id wl_shm_pool_id = args[1].n;
  struct wl_object *wl_shm_pool;
  WL_CHECK_IF_NOT_REGISTERED(wl_shm_pool_id, wl_shm_pool);

  wl_fd pool_fd = args[2].F;
  wl_int buffer_size = args[3].i;

  struct wl_shm *shm = calloc(1, sizeof(struct wl_shm));
  shm->fd = pool_fd;

  uint8_t *buffer = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, pool_fd, 0);
  if (buffer == MAP_FAILED) {
    wl_uint error_code = 0;
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
    return wl_error_set(wl_shm_id, error_code, "failed to mmap");
  }

  shm->buffer = buffer;
  shm->size = buffer_size;

  wl_object_add(conn, wl_shm_pool_id, wl_interface_get("wl_shm_pool"), shm);
  return 0;
}

int wl_shm_pool_create_buffer(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id wl_shm_pool_id = args[0].o;
  wl_new_id wl_buffer_id = args[1].n;
  struct wl_object *wl_buffer;
  WL_CHECK_IF_NOT_REGISTERED(wl_buffer_id, wl_buffer);

  wl_int offset = args[2].i;
  wl_int width =  args[3].i;
  wl_int height = args[4].i;
  wl_int stride = args[5].i;
  enum wl_shm_format_enum format = args[6].e;

  int format_supported = 0;
  for (size_t i = 0; i < LENGTH(supported_formats); i++) {
    if (supported_formats[i] == format) {
      format_supported = 1;
      break;
    }
  }

  if (!format_supported) {
    wl_error_set(wl_shm_pool_id, WL_SHM_ERROR_INVALID_FORMAT, "format not supported");
    return -1;
  }

  if (offset % 4 != 0) {
    wl_error_set(wl_shm_pool_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid offset");
    return -1;
  }

  struct wl_object *wl_shm_pool = wl_object_get(conn, wl_shm_pool_id);
  struct wl_shm *shm = wl_shm_pool->data;

  uint32_t region_size = (uint32_t)stride * height;
  if ((region_size > shm->size) || (offset > shm->size - region_size)) {
    wl_error_set(wl_shm_pool_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "requested region too big");
    return -1;
  }

  struct wl_buffer *buffer = calloc(1, sizeof(struct wl_buffer));
  buffer->offset = offset;
  buffer->width = width;
  buffer->height = height;
  buffer->stride = stride;
  buffer->format = format;
  buffer->shm = shm;

  wl_object_add(conn, wl_buffer_id, wl_interface_get("wl_buffer"), buffer);
  return 0;
}

int wl_shm_pool_resize(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id wl_shm_pool_id = args[0].o;
  wl_int new_size = args[1].i;

  struct wl_object *wl_shm_pool = wl_object_get(conn, wl_shm_pool_id);
  struct wl_shm *shm = wl_shm_pool->data;

  uint8_t *new_buffer = mremap(shm->buffer, shm->size, new_size, MREMAP_MAYMOVE | MREMAP_DONTUNMAP);
  if (new_buffer == MAP_FAILED) {
    wl_error_set(wl_shm_pool_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to mremap");
    return -1;
  }

  shm->buffer = new_buffer;
  shm->size = new_size;

  return 0;
}

int wl_shm_pool_destroy(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id wl_shm_pool_id = args[0].o;

  struct wl_object *wl_shm_pool = wl_object_get(conn, wl_shm_pool_id);
  struct wl_shm *shm = wl_shm_pool->data;

  munmap(shm->buffer, shm->size);
  free(shm);
  wl_object_del(conn, wl_shm_pool_id);

  return 0;
}

int wl_buffer_destroy(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id wl_buffer_id = args[0].o;

  struct wl_object *wl_buffer = wl_object_get(conn, wl_buffer_id);
  struct wl_buffer *buffer = wl_buffer->data;
  if (buffer)
    free(buffer);
  wl_object_del(conn, wl_buffer_id);

  return 0;
}


int wl_surface_attach(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id wl_surface_id = args[0].u;
  struct wl_object *wl_surface = wl_object_get(conn, wl_surface_id);

  wl_object_id wl_buffer_id = args[1].o;
  struct wl_object *wl_buffer;
  WL_CHECK_IF_REGISTERED(wl_buffer_id, wl_buffer);

  struct wl_surface *surface = wl_surface->data;
  struct wl_buffer *buffer = wl_buffer->data;

  buffer->id = wl_buffer_id;
  surface->pending = buffer;

  return 0;
}

int wl_surface_damage(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id wl_surface_id = args[0].u;
  struct wl_object *wl_surface = wl_object_get(conn, wl_surface_id);

  struct wl_surface *surface = wl_surface->data;

  wl_int x =      args[1].i;
  wl_int y =      args[2].i;
  wl_int width =  args[3].i;
  wl_int height = args[4].i;

  surface->x = x;
  surface->y = y;
  surface->width = width;
  surface->height = height;
  surface->damaged = 1;

  return 0;
}


int wl_surface_frame(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id wl_callback_id = args[1].o;
  struct wl_object *wl_callback;
  WL_CHECK_IF_NOT_REGISTERED(wl_callback_id, wl_callback);
  wl_connection_callback_add(conn, args[0].o, wl_callback_id);
  return 0;
}

int xdg_wm_base_get_xdg_surface(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id xdg_surface_id = args[1].n;
  struct wl_object *xdg_surface_obj;
  WL_CHECK_IF_NOT_REGISTERED(xdg_surface_id, xdg_surface_obj);

  wl_object_id wl_surface_id = args[2].o;
  struct wl_object *wl_surface_obj;
  WL_CHECK_IF_REGISTERED(wl_surface_id, wl_surface_obj);

  struct wl_xdg_surface *xdg_surface = calloc(1, sizeof(struct wl_xdg_surface));
  if (!xdg_surface) {
    wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
    return -1;
  }

  xdg_surface->surface = wl_surface_obj->data;
  wl_object_add(conn, xdg_surface_id, wl_interface_get("xdg_surface"), xdg_surface);

  return 0;
}

int xdg_surface_ack_configure(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id xdg_surface_id = args[0].o;
  struct wl_object *xdg_surface_obj = wl_object_get(conn, xdg_surface_id);

  struct wl_xdg_surface *xdg_surface = xdg_surface_obj->data;

  if (args[1].u != xdg_surface->serial) {
    wl_error_set(xdg_surface_id, XDG_SURFACE_ERROR_INVALID_SERIAL, "invalid serial");
    return -1;
  }

  if (!xdg_surface->serial) {
    wl_error_set(xdg_surface_id, XDG_SURFACE_ERROR_INVALID_SERIAL, "unexpected xdg_surface.ack_configure()");
    return -1;
  }

  xdg_surface->serial = 0;

  return 0;
}

int xdg_surface_get_toplevel(struct wl_connection *conn, union wl_arg *args) {
  wl_object_id xdg_surface_id = args[0].o;
  struct wl_object *xdg_surface_obj = wl_object_get(conn, xdg_surface_id);

  wl_new_id xdg_toplevel_id = args[1].n;
  struct wl_object *xdg_toplevel;
  WL_CHECK_IF_NOT_REGISTERED(xdg_toplevel_id, xdg_toplevel);

  struct wl_xdg_surface *xdg_surface = xdg_surface_obj->data;
  xdg_surface->surface->role = WL_SURFACE_TOPLEVEL;

  wl_object_add(conn, xdg_toplevel_id, wl_interface_get("xdg_toplevel"), NULL);
  xdg_toplevel_configure(conn, xdg_toplevel_id, 0, 0, NULL, 0);

  xdg_surface->serial = WL_SERIAL;
  xdg_surface_configure(conn, xdg_surface_id, xdg_surface->serial);

  return 0;
}
