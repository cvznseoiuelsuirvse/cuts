#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "wayland/types/wayland.h"

#include "wayland/display.h"

#include "util/log.h"
#include "render/render.h"

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

int wl_display_get_registry(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_new_id object_id = args[1].n;
  struct c_wl_object *c_wl_registry;
  C_WL_CHECK_IF_NOT_REGISTERED(object_id, c_wl_registry);

  c_wl_object_add(conn, object_id, (struct c_wl_interface *)c_wl_interface_get("wl_registry"), 0);

  int i = 1;
  struct c_wl_display_supported_iface *iface;
  c_list_for_each(conn->dpy->supported_ifaces, iface) {
    wl_registry_global(conn, object_id, i++, iface->iface->name, iface->iface->version);
  }

  return 0;
};

// only for log purposes
int wl_display_sync(struct c_wl_connection *conn, union c_wl_arg *args) {
  return 0;
}

int wl_registry_bind(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_new_id name =   args[1].u;
  c_wl_uint version =  args[3].u;
  c_wl_new_id new_id = args[4].n;
  struct c_wl_object *c_wl_object;
  C_WL_CHECK_IF_NOT_REGISTERED(new_id, c_wl_object);

  const struct c_wl_display_supported_iface *interface = c_list_get(conn->dpy->supported_ifaces, name - 1);
  if (!interface) return c_wl_error_set(new_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "interface is not supported");

  c_wl_object_add(conn, new_id, interface->iface, NULL);

  void *bind_data = NULL;
  if (interface->on_bind) {
    bind_data = interface->on_bind(conn, new_id, version, interface->userdata);
  }

  c_wl_object_get(conn, new_id)->data = bind_data;

  return 0;
}

int wl_shm_create_pool(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_shm_id = args[0].o;
  c_wl_new_id wl_shm_pool_id = args[1].n;
  struct c_wl_object *wl_shm_pool;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_shm_pool_id, wl_shm_pool);

  c_wl_fd pool_fd = args[2].F;
  c_wl_int buffer_size = args[3].i;

  struct c_wl_shm_pool *pool = calloc(1, sizeof(*pool));
  if (!pool) return c_wl_error_set(wl_shm_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");

  pool->fd = pool_fd;

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
    free(pool);
    return c_wl_error_set(wl_shm_id, error_code, "failed to mmap");
  }

  pool->ptr = buffer;
  pool->size = buffer_size;
  pool->supported_formats = c_wl_object_get(conn, wl_shm_id)->data;

  c_wl_object_add(conn, wl_shm_pool_id, c_wl_interface_get("wl_shm_pool"), pool);
  return 0;
}

int wl_shm_pool_create_buffer(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_shm_pool_id = args[0].o;
  struct c_wl_object *wl_shm_pool = c_wl_object_get(conn, wl_shm_pool_id);
  struct c_wl_shm_pool *pool = wl_shm_pool->data;

  c_wl_new_id wl_buffer_id = args[1].n;
  struct c_wl_object *wl_buffer;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_buffer_id, wl_buffer);

  c_wl_int offset = args[2].i;
  c_wl_int width =  args[3].i;
  c_wl_int height = args[4].i;
  c_wl_int stride = args[5].i;
  enum wl_shm_format_enum format = args[6].e;

  int format_supported = 0;
  int *fmt;
  c_list_for_each(pool->supported_formats, fmt) {
    if ((uint64_t)fmt == format) {
      format_supported = 1;
      break;
    }
  }

  if (!format_supported)
    return c_wl_error_set(wl_shm_pool_id, WL_SHM_ERROR_INVALID_FORMAT, "format not supported");

  if (stride % 4 != 0)
    return c_wl_error_set(wl_shm_pool_id, WL_SHM_ERROR_INVALID_STRIDE, "invalid offset");
  

  if (pool->size - offset < height * stride)
    return c_wl_error_set(wl_shm_pool_id, WL_SHM_ERROR_INVALID_STRIDE, "invalid offset");


  uint32_t region_size = (uint32_t)stride * height;
  if ((region_size > pool->size) || (offset > pool->size - region_size)) {
    c_wl_error_set(wl_shm_pool_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "requested region too large");
    return -1;
  }

  struct c_wl_buffer *c_wl_buffer = calloc(1, sizeof(*c_wl_buffer));
  struct c_shm *c_shm = calloc(1, sizeof(*c_shm));

  c_wl_buffer->id = wl_buffer_id;
  c_wl_buffer->width = width;
  c_wl_buffer->height = height;
  c_wl_buffer->type = C_WL_BUFFER_SHM;

  c_shm->stride = stride;
  c_shm->format = format;
  c_shm->offset = offset;
  c_shm->base_ptr = &pool->ptr;
  c_wl_buffer->shm = c_shm;

  c_wl_object_add(conn, wl_buffer_id, c_wl_interface_get("wl_buffer"), c_wl_buffer);
  return 0;
}

int wl_shm_pool_resize(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_shm_pool_id = args[0].o;
  c_wl_int new_size = args[1].i;

  struct c_wl_object *wl_shm_pool = c_wl_object_get(conn, wl_shm_pool_id);
  struct c_wl_shm_pool *pool = wl_shm_pool->data;

  uint8_t *new_buffer = mremap(pool->ptr, pool->size, new_size, MREMAP_MAYMOVE);
  if (new_buffer == MAP_FAILED) {
    c_wl_error_set(wl_shm_pool_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to mremap: %s", strerror(errno));
    return -1;
  }

  pool->ptr = new_buffer;
  pool->size = new_size;

  return 0;
}

int wl_shm_pool_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_shm_pool_id = args[0].o;

  struct c_wl_object *wl_shm_pool = c_wl_object_get(conn, wl_shm_pool_id);
  struct c_wl_shm_pool *pool = wl_shm_pool->data;

  munmap(pool->ptr, pool->size);
  free(pool);
  c_wl_object_del(conn, wl_shm_pool_id);

  return 0;
}

int wl_surface_set_buffer_scale(struct c_wl_connection *conn, union c_wl_arg *args) {
  return 0;
}

int wl_buffer_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_buffer_id = args[0].o;

  struct c_wl_object *wl_buffer = c_wl_object_get(conn, wl_buffer_id);
  struct c_wl_buffer *c_wl_buffer = wl_buffer->data;
  struct c_wl_surface *c_wl_surface = c_wl_buffer->surface;

  if (c_wl_surface->pending == c_wl_buffer)
    c_wl_surface->pending = NULL;

  if (c_wl_surface->active == c_wl_buffer)
    c_wl_surface->active = NULL;

  free(c_wl_buffer);
  c_wl_object_del(conn, wl_buffer_id);
  return 0;
}

int wl_compositor_create_region(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_region_id = args[1].o;
  struct c_wl_object *wl_region;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_region_id, wl_region);

  struct c_wl_region *c_wl_region = calloc(1, sizeof(struct c_wl_region));
  if (!c_wl_region) {
    c_log(C_LOG_ERROR, "calloc failed");
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  c_wl_object_add(conn, wl_region_id, c_wl_interface_get("wl_region"), c_wl_region);
  return 0;
}

int wl_region_add(struct c_wl_connection *conn, union c_wl_arg *args) {
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

int wl_region_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_region_id = args[0].u;
  struct c_wl_object *wl_region = c_wl_object_get(conn, wl_region_id);
  free(wl_region->data);
  c_wl_object_del(conn, wl_region_id);
  return 0;
}

int wl_surface_damage(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_object *wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *c_wl_surface = wl_surface->data;

  damage_surface(c_wl_surface, args);

  return 0;
}

int wl_surface_damage_buffer(struct c_wl_connection *conn, union c_wl_arg *args) {
    c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_object *wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *c_wl_surface = wl_surface->data;

  damage_surface(c_wl_surface, args);

  return 0;
}


int wl_surface_frame(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_new_id c_wl_callback_id = args[1].o;
  struct c_wl_object *c_wl_callback;
  C_WL_CHECK_IF_NOT_REGISTERED(c_wl_callback_id, c_wl_callback);
  if (c_wl_connection_callback_add(conn, c_wl_callback_id, args[0].o) == -1)     
    return c_wl_error_set(args[0].o, WL_DISPLAY_ERROR_INVALID_OBJECT, "object %d already registered", c_wl_callback_id);
  return 0;
}

int wl_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_object *wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *c_wl_surface = wl_surface->data;

  struct c_wl_display *dpy = conn->dpy;
  c_wl_display_notify(dpy, c_wl_surface, C_WL_DISPLAY_ON_SURFACE_DESTROY);

  if (c_wl_surface->xdg.children)
    c_list_destroy(c_wl_surface->xdg.children);

  if (c_wl_surface->sub.children)
    c_list_destroy(c_wl_surface->sub.children);

  c_wl_object_del(conn, wl_surface_id);
  return 0;
}

int wl_surface_set_opaque_region(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_object *c_wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *surface = c_wl_surface->data;

  c_wl_object_id wl_region_id = args[1].o;
  if (wl_region_id == 0) {
    memset(&surface->opaque, 0, sizeof(surface->opaque));
    return 0;
  }

  struct c_wl_object *c_wl_region;
  C_WL_CHECK_IF_REGISTERED(wl_region_id, c_wl_region);

  memcpy(&surface->opaque, c_wl_region->data, sizeof(surface->opaque));

  return 0;
}

int wl_surface_set_input_region(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_object *c_wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *surface = c_wl_surface->data;

  c_wl_object_id wl_region_id = args[1].o;
  if (wl_region_id == 0) {
    memset(&surface->input, 0, sizeof(surface->input));
    return 0;
  }

  struct c_wl_object *c_wl_region;
  C_WL_CHECK_IF_REGISTERED(wl_region_id, c_wl_region);

  memcpy(&surface->input, c_wl_region->data, sizeof(surface->input));

  return 0;
}

int wl_surface_attach(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_object *wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *c_wl_surface = wl_surface->data;

  c_wl_object_id wl_buffer_id = args[1].o;
  if (wl_buffer_id == 0) {
    c_wl_surface->pending = NULL;
    return 0;
  }

  struct c_wl_object *wl_buffer = c_wl_object_get(conn, wl_buffer_id);
  C_WL_CHECK_IF_REGISTERED(wl_buffer_id, wl_buffer);
  struct c_wl_buffer *c_wl_buffer = wl_buffer->data;
  c_wl_buffer->surface = c_wl_surface;
  c_wl_surface->pending = c_wl_buffer;

  return 0;
}

int wl_surface_commit(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_object *wl_surface = c_wl_object_get(conn, wl_surface_id);
  struct c_wl_surface *c_wl_surface = wl_surface->data;
  
  if (c_wl_surface->active) {
    wl_buffer_release(c_wl_surface->conn, c_wl_surface->active->id);
  }

  if (c_wl_surface->pending) {
    c_wl_surface->active = c_wl_surface->pending;
    c_wl_surface->pending = NULL;
  }

  struct c_wl_display *dpy = conn->dpy;
  c_wl_display_notify(dpy, c_wl_surface, C_WL_DISPLAY_ON_SURFACE_UPDATE);

  return 0;
}

int wl_compositor_create_surface(struct c_wl_connection *conn, union c_wl_arg *args) {
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
  c_wl_surface->sub.children = c_list_new();
  struct c_wl_display *dpy = conn->dpy;
  c_wl_display_notify(dpy, c_wl_surface,  C_WL_DISPLAY_ON_SURFACE_NEW);
  c_wl_object_add(conn, wl_surface_id, c_wl_interface_get("wl_surface"), c_wl_surface);
  return 0;
}


int wl_subcompositor_get_subsurface(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_new_id wl_subsurface_id = args[1].n;
  struct c_wl_object *wl_subsurface;
  struct c_wl_object *wl_surface_child;
  struct c_wl_object *wl_surface_parent;

  C_WL_CHECK_IF_NOT_REGISTERED(wl_subsurface_id, wl_subsurface);
  C_WL_CHECK_IF_REGISTERED(args[2].o, wl_surface_child);
  C_WL_CHECK_IF_REGISTERED(args[3].o, wl_surface_parent);

  struct c_wl_surface *surface_child = wl_surface_child->data;
  struct c_wl_surface *surface_parent = wl_surface_parent->data;

  if (surface_child == surface_parent)
    return c_wl_error_set(args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_PARENT, "parent and child cannot be the same objects");

  if (surface_child->role)
    return c_wl_error_set(args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "child surface already holds a role");

  if (surface_child->sub.children && c_list_idx(surface_child->sub.children, surface_parent) != -1)
    return c_wl_error_set(args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "parent surface is one of the child's descendants,");
  c_log(C_LOG_DEBUG, "adding %p to %p", surface_child, surface_parent);

  c_list_push(surface_parent->sub.children, surface_child, 0);
  surface_child->sub.parent = surface_parent;
  surface_child->role = C_WL_SURFACE_ROLE_SUBSURFACE;

  c_wl_object_add(conn, wl_subsurface_id, c_wl_interface_get("wl_subsurface"), surface_child);

  return 0;
}

int wl_subsurface_set_position(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_surface *surface = c_wl_object_get(conn, args[0].o)->data;
  surface->sub.x = args[1].i;
  surface->sub.y = args[2].i;
  return 0;
}

int wl_subsurface_set_sync(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_surface *surface = c_wl_object_get(conn, args[0].o)->data;
  surface->sub.sync = 1;
  return 0;
}

int wl_subsurface_set_desync(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_surface *surface = c_wl_object_get(conn, args[0].o)->data;
  surface->sub.sync = 0;
  return 0;
}

int wl_subsurface_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_surface *surface_child = c_wl_object_get(conn, args[0].o)->data;
  struct c_wl_surface *surface_parent = surface_child->sub.parent;

  if (surface_parent)
    c_list_remove_ptr(&surface_parent->sub.children, surface_child);

  memset(&surface_child->sub, 0, sizeof(surface_child->sub));

  return 0;
}

int wl_subsurface_place_above(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *wl_surface_sibling;
  C_WL_CHECK_IF_REGISTERED(args[1].o, wl_surface_sibling);

  struct c_wl_surface *surface = c_wl_object_get(conn, args[0].o)->data;
  struct c_wl_surface *sibling = wl_surface_sibling->data;
  struct c_wl_surface *parent = surface->sub.parent;

  c_list_remove_ptr(&parent->sub.children, surface);
  int sibling_idx = c_list_idx(parent->sub.children, sibling);
  assert(sibling_idx != -1);

  c_list_insert(&parent->sub.children, sibling_idx, surface, 0);
  return 0;
};

int wl_subsurface_place_below(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *wl_surface_sibling;
  C_WL_CHECK_IF_REGISTERED(args[1].o, wl_surface_sibling);

  struct c_wl_surface *surface = c_wl_object_get(conn, args[0].o)->data;
  struct c_wl_surface *sibling = wl_surface_sibling->data;
  struct c_wl_surface *parent = surface->sub.parent;

  c_list_remove_ptr(&parent->sub.children, surface);
  int sibling_idx = c_list_idx(parent->sub.children, sibling);
  assert(sibling_idx != -1);

  c_list_insert(&parent->sub.children, sibling_idx+1, surface, 0);
  return 0;
};
