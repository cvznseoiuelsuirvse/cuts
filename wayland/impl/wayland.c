#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "wayland/proto/wayland.h"
#include "wayland/display.h"

#include "util/log.h"
#include "util/malloc.h"
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

  struct c_wl_interface *iface = c_wl_interface_get("wl_registry");
  c_wl_object_add(conn, object_id, iface->version, iface, 0);

  int i = 1;
  struct c_wl_display_supported_iface *supported_iface;
  c_list_for_each(c_wl_connection_get_dpy(conn)->supported_ifaces, supported_iface) {
    wl_registry_global(conn, object_id, i++, supported_iface->iface->name, supported_iface->iface->version);
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

  const struct c_wl_display_supported_iface *interface =
      c_list_get(c_wl_connection_get_dpy(conn)->supported_ifaces, name - 1);

  if (!interface) c_wl_error_set_and_return(new_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "interface is not supported");

  c_wl_object_add(conn, new_id, version, interface->iface, NULL);

  if (interface->on_bind) {
    void *bind_data = interface->on_bind(conn, new_id, version, interface->userdata);
    if (bind_data)
      c_wl_object_get(conn, new_id)->data = bind_data;
    
  }

  return 0;
}

int wl_shm_create_pool(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_shm_id = args[0].o;
  struct c_wl_object *wl_shm = c_wl_object_get(conn, wl_shm_id);

  c_wl_new_id wl_shm_pool_id = args[1].n;
  struct c_wl_object *wl_shm_pool;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_shm_pool_id, wl_shm_pool);

  c_wl_fd pool_fd = args[2].F;
  c_wl_int buffer_size = args[3].i;

  struct c_wl_shm_pool *pool = c_malloc(sizeof(*pool));
  if (!pool) c_wl_error_set_and_return(wl_shm_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");

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

    c_free(pool);
    c_log_errno(C_LOG_ERROR, "mmap() failed");
    c_wl_error_set_and_return(wl_shm_id, error_code, "failed to mmap");
  }

  pool->ptr = buffer;
  pool->size = buffer_size;
  struct c_wl_formats *supported_formats = c_wl_object_get(conn, wl_shm_id)->data;
  pool->supported_formats = supported_formats->formats;
  pool->n_supported_formats = supported_formats->n_formats;

  c_wl_object_add(conn, wl_shm_pool_id, wl_shm->version, c_wl_interface_get("wl_shm_pool"), pool);
  return 0;
}

int wl_shm_pool_create_buffer(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_shm_pool_id = args[0].o;
  struct c_wl_shm_pool *pool = c_wl_object_get(conn, wl_shm_pool_id)->data;

  c_wl_new_id wl_buffer_id = args[1].n;
  struct c_wl_object *wl_buffer;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_buffer_id, wl_buffer);

  c_wl_int offset = args[2].i;
  c_wl_int width =  args[3].i;
  c_wl_int height = args[4].i;
  c_wl_int stride = args[5].i;
  enum wl_shm_format_enum format = args[6].e;

  for (size_t i = 0; i < pool->n_supported_formats; i++) {
    uint32_t format2 = pool->supported_formats[i];
    if (format == format2) goto format_supported;
  }
  c_wl_error_set_and_return(wl_shm_pool_id, WL_SHM_ERROR_INVALID_FORMAT, "format not supported");

format_supported:
  if (stride % 4 != 0)
    c_wl_error_set_and_return(wl_shm_pool_id, WL_SHM_ERROR_INVALID_STRIDE, "invalid offset");
  

  if (pool->size - offset < height * stride)
    c_wl_error_set_and_return(wl_shm_pool_id, WL_SHM_ERROR_INVALID_STRIDE, "invalid offset");


  uint32_t region_size = (uint32_t)stride * height;
  if ((region_size > pool->size) || (offset > pool->size - region_size)) {
    c_wl_error_set_and_return(wl_shm_pool_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "requested region too large");
    return -1;
  }

  struct c_wl_buffer *c_wl_buffer = c_malloc(sizeof(*c_wl_buffer));
  struct c_shm *c_shm = c_malloc(sizeof(*c_shm));

  c_wl_buffer->id = wl_buffer_id;
  c_wl_buffer->conn = conn;
  c_wl_buffer->width = width;
  c_wl_buffer->height = height;
  c_wl_buffer->type = C_WL_BUFFER_SHM;
  c_wl_buffer->scale = 1;

  c_shm->stride = stride;
  c_shm->format = format;
  c_shm->offset = offset;
  c_shm->base_ptr = &pool->ptr;
  c_wl_buffer->shm = c_shm;

  struct c_wl_interface *iface = c_wl_interface_get("wl_buffer");
  c_wl_object_add(conn, wl_buffer_id, iface->version, iface, c_wl_buffer);
  return 0;
}

int wl_shm_pool_resize(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_shm_pool_id = args[0].o;
  c_wl_int new_size = args[1].i;
  struct c_wl_shm_pool *pool = c_wl_object_get(conn, wl_shm_pool_id)->data;

  uint8_t *new_buffer = mremap(pool->ptr, pool->size, new_size, MREMAP_MAYMOVE);
  if (new_buffer == MAP_FAILED) {
    c_wl_error_set_and_return(wl_shm_pool_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to mremap: %s", strerror(errno));
    return -1;
  }

  pool->ptr = new_buffer;
  pool->size = new_size;

  return 0;
}

int wl_shm_pool_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_shm_pool_id = args[0].o;
  struct c_wl_shm_pool *pool = c_wl_object_get(conn, wl_shm_pool_id)->data;

  munmap(pool->ptr, pool->size);
  c_wl_object_del(conn, wl_shm_pool_id);

  return 0;
}

int wl_surface_set_buffer_transform(struct c_wl_connection *conn, union c_wl_arg *args) { return 0; }
int wl_surface_offset(struct c_wl_connection *conn, union c_wl_arg *args) { return 0; }

int wl_surface_set_buffer_scale(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_surface *surface = c_wl_object_get(conn, args[0].o)->data;
  c_wl_int scale = args[1].i;
  if (scale < 0)
    c_wl_error_set_and_return(args[0].o, WL_SURFACE_ERROR_INVALID_SCALE, "scale must be >0");

  if (surface->pending)
    surface->pending->scale = scale;

  return 0;
}

int wl_buffer_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_buffer *wl_buffer = self->data;

  struct c_wl_display *dpy = c_wl_connection_get_dpy(conn);
  c_wl_display_notify(dpy, wl_buffer, C_WL_DISPLAY_ON_BUFFER_DESTROY);

  wl_buffer->id = 0;
  c_wl_object_del(conn, self->id);
  return 0;
}

int wl_compositor_create_region(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id wl_region_id = args[1].o;
  struct c_wl_object *wl_region;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_region_id, wl_region);

  struct c_wl_region *c_wl_region = c_malloc(sizeof(struct c_wl_region));
  if (!c_wl_region) {
    c_log(C_LOG_ERROR, "calloc failed");
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  c_wl_object_add(conn, wl_region_id, self->version, c_wl_interface_get("wl_region"), c_wl_region);
  return 0;
}

int wl_region_add(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_region_id = args[0].u;
  struct c_wl_region *c_wl_region = c_wl_object_get(conn, wl_region_id)->data;;

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
  c_wl_object_del(conn, wl_region_id);
  return 0;
}

int wl_surface_damage(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_surface *c_wl_surface = c_wl_object_get(conn, wl_surface_id)->data;;

  damage_surface(c_wl_surface, args);

  return 0;
}

int wl_surface_damage_buffer(struct c_wl_connection *conn, union c_wl_arg *args) {
    c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_surface *c_wl_surface = c_wl_object_get(conn, wl_surface_id)->data;

  damage_surface(c_wl_surface, args);

  return 0;
}


int wl_surface_frame(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_callback_id = args[1].o;
  struct c_wl_object *wl_callback;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_callback_id, wl_callback);

  struct c_wl_surface *surface = self->data;
  assert(surface->n_frames < sizeof(surface->frames));

  surface->frames[surface->n_frames++] = wl_callback_id;
  // wl_surface->frame_id = wl_callback_id;
  
  struct c_wl_interface *iface = c_wl_interface_get("wl_callback");
  c_wl_object_add(conn, wl_callback_id, iface->version, iface, NULL);
  return 0;
}

int wl_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  
  struct c_wl_surface *wl_surface = self->data;
  struct c_wl_display *dpy = c_wl_connection_get_dpy(conn);
  c_wl_display_notify(dpy, wl_surface, C_WL_DISPLAY_ON_SURFACE_DESTROY);

  if (wl_surface->active) {
    c_unref(wl_surface->active);
  }

  if (wl_surface->pending && wl_surface->pending != wl_surface->active) {
    c_unref(wl_surface->pending);
  }

  if (wl_surface->xdg_surface && wl_surface->xdg_surface->surface == wl_surface)
      wl_surface->xdg_surface->surface = NULL;

  if (wl_surface->sub.surface)
    wl_surface->sub.surface->surface = NULL;

  else if (wl_surface->sub.children) {
    struct c_wl_subsurface *ss;
    c_list_for_each(wl_surface->sub.children, ss)
      ss->parent = NULL;
    c_list_destroy(wl_surface->sub.children);
  }

  c_wl_object_del(conn, self->id);
  return 0;
}

int wl_surface_set_opaque_region(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_surface *surface = c_wl_object_get(conn, wl_surface_id)->data;

  c_wl_object_id wl_region_id = args[1].o;
  if (wl_region_id == 0) {
    memset(&surface->opaque, 0, sizeof(surface->opaque));
    return 0;
  }

  struct c_wl_object *c_wl_region;
  C_WL_CHECK_IF_REGISTERED(wl_region_id, c_wl_region);

  struct c_wl_region *region = c_wl_region->data;
  memcpy(&surface->opaque, region, sizeof(surface->opaque));
  return 0;
}

int wl_surface_set_input_region(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_surface *surface = c_wl_object_get(conn, wl_surface_id)->data;

  c_wl_object_id wl_region_id = args[1].o;
  if (wl_region_id == 0) {
    memset(&surface->input, 0, sizeof(surface->input));
    return 0;
  }

  struct c_wl_object *c_wl_region;
  C_WL_CHECK_IF_REGISTERED(wl_region_id, c_wl_region);

  struct c_wl_region *region = c_wl_region->data;
  memcpy(&surface->input, region, sizeof(surface->input));

  return 0;
}

int wl_surface_attach(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_surface *wl_surface = self->data;

  c_wl_object_id buf_id = args[1].o;
  c_wl_int x = args[2].i;
  c_wl_int y = args[3].i;

  if (self->version >= 5 && (x > 0 || y > 0))
    c_wl_error_set_and_return(args[0].o, WL_SURFACE_ERROR_INVALID_OFFSET,
                              "wl_surface version is %d, but x or y are not 0",
                              self->version);
  
  if (buf_id > 0) {
    struct c_wl_object *wl_buffer_o;
    C_WL_CHECK_IF_REGISTERED(buf_id, wl_buffer_o);
    struct c_wl_buffer *wl_buffer = wl_buffer_o->data;
    wl_surface->pending = wl_buffer;

  } else {
    wl_surface->pending = NULL;
  }

  return 0;
}

int wl_surface_commit(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_surface *wl_surface = self->data;

  if (wl_surface->active)
    c_log(C_LOG_DEBUG, "ACTIVE #%d %p", wl_surface->active->id, wl_surface->active);
  else
    c_log(C_LOG_DEBUG, "ACTIVE %p",  wl_surface->active);

  if (wl_surface->pending)
    c_log(C_LOG_DEBUG, "PNDING #%d %p", wl_surface->pending->id, wl_surface->pending);
  else
    c_log(C_LOG_DEBUG, "PNDING %p", wl_surface->pending);

  if (wl_surface->pending != wl_surface->active) {
    if (wl_surface->active && wl_surface->active->id > 0) {
      wl_buffer_release(conn, wl_surface->active->id);
      c_unref(wl_surface->active);
    }

    if (wl_surface->pending) 
      c_ref(wl_surface->pending);

    wl_surface->active = wl_surface->pending;
    
  }
  
  struct c_wl_display *dpy = c_wl_connection_get_dpy(conn);
  c_wl_display_notify(dpy, wl_surface, C_WL_DISPLAY_ON_SURFACE_COMMIT);

  for (size_t i = 0; i < wl_surface->n_frames; i++) {
    c_wl_connection_callback_done(conn, wl_surface->frames[i]);
    wl_surface->frames[i] = 0;
  }
  wl_surface->n_frames = 0;

  return 0;
}

int wl_compositor_create_surface(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id wl_surface_id = args[1].o;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_surface_id, wl_surface);

  struct c_wl_surface *c_wl_surface = c_malloc(sizeof(struct c_wl_surface));
  if (!c_wl_surface) {
    c_log(C_LOG_ERROR, "calloc failed");
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  }

  c_wl_surface->id = wl_surface_id;
  c_wl_surface->conn = conn;

  c_wl_object_add(conn, wl_surface_id, self->version, c_wl_interface_get("wl_surface"), c_wl_surface);
  return 0;
}


int wl_subcompositor_get_subsurface(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_subsurface_id = args[1].n;
  struct c_wl_object *wl_subsurface;
  struct c_wl_object *wl_surface;
  struct c_wl_object *wl_surface_parent;

  C_WL_CHECK_IF_NOT_REGISTERED(wl_subsurface_id, wl_subsurface);
  C_WL_CHECK_IF_REGISTERED(args[2].o, wl_surface);
  C_WL_CHECK_IF_REGISTERED(args[3].o, wl_surface_parent);

  struct c_wl_subsurface *subsurface = c_malloc(sizeof(*subsurface));
  if (!subsurface) 
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed to allocate c_wl_subsurface");


  struct c_wl_surface *surface = wl_surface->data;
  struct c_wl_surface *surface_parent = wl_surface_parent->data;

  if (surface == surface_parent) {
    c_free(subsurface);
    c_wl_error_set_and_return(args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_PARENT,
                              "parent and child cannot be the same objects");
  }

  if (surface->role) {
    c_free(subsurface);
    c_wl_error_set_and_return(args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                              "child surface already holds a role");
  }

  subsurface->id = wl_subsurface_id;
  subsurface->parent = surface_parent;

  surface->sub.surface = subsurface;
  subsurface->surface = surface;

  subsurface->surface->role = C_WL_SURFACE_ROLE_SUBSURFACE;

  if (surface->sub.children && c_list_idx(surface->sub.children, surface_parent) != -1)
    c_wl_error_set_and_return(
        args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
        "parent surface is one of the child's descendants");

  c_log(C_LOG_DEBUG, "new sub %d: adding %p to %p", wl_subsurface_id, subsurface->surface, subsurface->parent);

  if (!surface_parent->sub.children)
    surface_parent->sub.children = c_list_new();

  c_list_push(surface_parent->sub.children, subsurface, 0);
  c_wl_object_add(conn, wl_subsurface_id, self->version, c_wl_interface_get("wl_subsurface"), subsurface);

  return 0;
}

int wl_subsurface_set_position(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_subsurface *surface = c_wl_object_get(conn, args[0].o)->data;
  surface->x = args[1].i;
  surface->y = args[2].i;
  return 0;
}

int wl_subsurface_set_sync(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_subsurface *surface = c_wl_object_get(conn, args[0].o)->data;
  surface->sync = 1;
  return 0;
}

int wl_subsurface_set_desync(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_subsurface *surface = c_wl_object_get(conn, args[0].o)->data;
  surface->sync = 0;
  return 0;
}

int wl_subsurface_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_subsurface *subsurface     = c_wl_object_get(conn, args[0].o)->data;
  struct c_wl_surface    *surface        = subsurface->surface;
  struct c_wl_surface    *surface_parent = subsurface->parent;

  if (surface_parent)
    c_list_remove_ptr(&surface_parent->sub.children, subsurface);

  surface->role = 0;
  surface->sub.surface = NULL;

  c_wl_object_del(conn, args[0].o);
  return 0;
}

int wl_subsurface_place_above(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *wl_surface_sibling;
  C_WL_CHECK_IF_REGISTERED(args[1].o, wl_surface_sibling);

  struct c_wl_subsurface *surface = c_wl_object_get(conn, args[0].o)->data;
  struct c_wl_surface    *sibling = wl_surface_sibling->data;
  struct c_wl_surface    *parent = surface->parent;

  c_list_remove_ptr(&parent->sub.children, surface);
  int sibling_idx = c_list_idx(parent->sub.children, sibling);
  assert(sibling_idx != -1);

  c_list_insert(&parent->sub.children, sibling_idx, surface, 0);
  return 0;
};

int wl_subsurface_place_below(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *wl_surface_sibling;
  C_WL_CHECK_IF_REGISTERED(args[1].o, wl_surface_sibling);

  struct c_wl_subsurface *surface = c_wl_object_get(conn, args[0].o)->data;
  struct c_wl_surface    *sibling = wl_surface_sibling->data;
  struct c_wl_surface    *parent = surface->parent;

  c_list_remove_ptr(&parent->sub.children, surface);
  int sibling_idx = c_list_idx(parent->sub.children, sibling);
  assert(sibling_idx != -1);

  c_list_insert(&parent->sub.children, sibling_idx+1, surface, 0);
  return 0;
};

int wl_pointer_set_cursor(struct c_wl_connection *conn, union c_wl_arg *args) {
  return 0;
}

int wl_pointer_release(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_del(conn, args[0].o);
  return 0;
}

int wl_keyboard_release(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_del(conn, args[0].o);
  return 0;
}

int wl_data_device_manager_get_data_device(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_object_id wl_data_device_id = args[1].n;
  struct c_wl_object *wl_data_device;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_data_device_id, wl_data_device);

  struct c_wl_object *wl_seat;
  C_WL_CHECK_IF_REGISTERED(args[2].o, wl_seat);

  c_wl_object_add(conn, wl_data_device_id, self->version, c_wl_interface_get("wl_data_device"), NULL);
  return 0;
}

int wl_data_device_release(struct c_wl_connection *conn, union c_wl_arg *args) {
  c_wl_object_del(conn, args[0].o);
  return 0;
}
