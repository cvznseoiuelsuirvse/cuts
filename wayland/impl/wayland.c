#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include "wayland/proto/wayland.h"
#include "wayland/proto/presentation-time.h"

#include "wayland/impl/wayland.h"
#include "wayland/impl/xdg-shell.h"
#include "wayland/impl/viewporter.h"
#include "wayland/impl/wlr-layer-shell-unstable-v1.h"

#include "wayland/display.h"
#include "output/drm/util.h"

#include "util/log.h"
#include "util/mem.h"
#include "util/helpers.h"
#include "render/types.h"

static void damage_surface(struct c_wl_surface *surface, c_wl_args args) {
  c_wl_int x =      args[1].i;
  c_wl_int y =      args[2].i;
  c_wl_int width =  args[3].i;
  c_wl_int height = args[4].i;

  surface->pending.damage.x = x;
  surface->pending.damage.y = y;
  surface->pending.damage.width = width;
  surface->pending.damage.height = height;
}

static void defer_pool(void *data) {
  struct c_wl_shm_pool *pool = data;
  munmap(pool->ptr, pool->size);
  close(pool->fd);
}

void defer_wl_buffer(void *data) {
  struct c_wl_buffer *buf = data;
  if (buf->dma)  c_unref(buf->dma);
  if (buf->shm)  c_unref(buf->shm);
  if (buf->pool) c_unref(buf->pool);
}

int c_wl_surface_state_apply(struct c_wl_surface *surface) {
  struct c_wl_surface_state *p = &surface->pending;
  struct c_wl_surface_state *a = &surface->active;

  if (p->commited & C_WL_SURFACE_STATE_DAMAGE) {
    a->damage = p->damage;
    p->commited &= ~C_WL_SURFACE_STATE_DAMAGE;
  };

  if (p->commited & C_WL_SURFACE_STATE_INPUT) {
    a->input = p->input;
    p->commited &= ~C_WL_SURFACE_STATE_INPUT;
  };

  if (p->commited & C_WL_SURFACE_STATE_OPAQUE) {
    a->opaque = p->opaque;
    p->commited &= ~C_WL_SURFACE_STATE_OPAQUE;
  };
  if (p->commited & C_WL_SURFACE_STATE_SCALE) {
    a->scale = p->scale;
    p->commited &= ~C_WL_SURFACE_STATE_SCALE;
  };
  if (p->commited & C_WL_SURFACE_STATE_TRANSFORM) {
    a->transform = p->transform;
    p->commited &= ~C_WL_SURFACE_STATE_TRANSFORM;
  };

  int error = 0;
  if (surface->xdg_surface)       c_xdg_surface_state_apply(surface->xdg_surface);
  if (surface->wlr_layer_surface) c_zwlr_layer_surface_state_apply(surface->wlr_layer_surface);
  if (surface->viewport)          error = c_wp_viewport_state_apply(surface->viewport);

  return error;
};

int wl_display_get_registry(struct c_wl_connection *conn, c_wl_args args) {
  c_wl_new_id object_id = args[1].n;
  struct c_wl_object *c_wl_registry;
  C_WL_CHECK_IF_NOT_REGISTERED(object_id, c_wl_registry);

  const struct c_wl_interface *iface = c_wl_interface_get("wl_registry");
  c_wl_object_add(conn, object_id, iface->version, iface, 0);

  const struct c_wl_interface **interfaces;
  size_t n_interfaces = c_wl_interface_get_all(&interfaces);
  int iface_idx = 1;
  for (size_t i = 0; i < n_interfaces; i++) {
    iface = interfaces[i];
    if (iface->is_supported)
      wl_registry_global(conn, object_id, iface_idx++, iface->name, iface->version);
  }

  return 0;
};

int wl_display_sync(struct c_wl_connection *conn, c_wl_args args) { return 0; }

int wl_registry_bind(struct c_wl_connection *conn, c_wl_args args) {
  c_wl_string interface_name = args[2].s;
  c_wl_uint version          = args[3].u;
  c_wl_new_id new_id         = args[4].n;

  struct c_wl_object *c_wl_object;
  C_WL_CHECK_IF_NOT_REGISTERED(new_id, c_wl_object);

  const struct c_wl_interface *interface = c_wl_interface_get(interface_name);
  if (!interface) c_wl_error_set_and_return(new_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "interface is not supported");

  struct c_wl_object *object = c_wl_object_add(conn, new_id, version, interface, NULL);

  if (interface->on_bind) {
    void *bind_data = interface->on_bind(conn, object, interface->on_bind_userdata);
    if (bind_data)
      object->data = bind_data;
  }

  return 0;
}

int wl_shm_create_pool(struct c_wl_connection *conn, c_wl_args args) {
  c_wl_object_id wl_shm_id = args[0].o;
  struct c_wl_object *wl_shm = c_wl_object_get(conn, wl_shm_id);

  c_wl_new_id wl_shm_pool_id = args[1].n;
  struct c_wl_object *wl_shm_pool;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_shm_pool_id, wl_shm_pool);

  c_wl_fd pool_fd = args[2].F;
  c_wl_int buffer_size = args[3].i;

  struct c_wl_shm_pool *pool = c_malloc(sizeof(*pool));
  if (!pool) c_wl_error_set_and_return(wl_shm_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "calloc failed");
  c_defer(pool, defer_pool);

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

    c_unref(pool);
    c_log_errno(C_LOG_ERROR, "mmap() failed");
    c_wl_error_set_and_return(wl_shm_id, error_code, "failed to mmap");
  }

  pool->ptr = buffer;
  pool->size = buffer_size;

  pool->obj = c_wl_object_add(conn, wl_shm_pool_id, wl_shm->version, c_wl_interface_get("wl_shm_pool"), pool);
  return 0;
}

int wl_shm_pool_create_buffer(struct c_wl_connection *conn, c_wl_args args) {
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

  if (stride % 4 != 0)
    c_wl_error_set_and_return(wl_shm_pool_id, WL_SHM_ERROR_INVALID_STRIDE, "invalid offset");
  

  if ((c_wl_int)pool->size - offset < height * stride)
    c_wl_error_set_and_return(wl_shm_pool_id, WL_SHM_ERROR_INVALID_STRIDE, "invalid offset");


  uint32_t region_size = (uint32_t)stride * height;
  if ((region_size > pool->size) || (offset > (c_wl_int)(pool->size - region_size))) {
    c_wl_error_set_and_return(wl_shm_pool_id, WL_DISPLAY_ERROR_INVALID_OBJECT, "requested region is too large");
    return -1;
  }

  struct c_wl_buffer *buffer = c_malloc(sizeof(*buffer));
  c_defer(buffer, defer_wl_buffer); 

  struct c_rawbuf *buf = c_malloc(sizeof(*buf));

  buffer->pool = pool;
  c_ref(pool);

  buf->width = width;
  buf->height = height;
  buf->stride = stride;
  buf->format = wl_shm_fmt_to_drm_fmt(format);
  buf->offset = offset;
  buf->base_ptr = pool->ptr;

  buffer->shm = buf;

  const struct c_wl_interface *iface = c_wl_interface_get("wl_buffer");
  buffer->obj = c_wl_object_add(conn, wl_buffer_id, iface->version, iface, buffer);

  return 0;
}

int wl_buffer_destroy(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_buffer *wl_buffer = self->data;

  wl_buffer->obj = NULL;
  c_wl_object_del(conn, self->id);
  return 0;
}


int wl_shm_pool_resize(struct c_wl_connection *conn, c_wl_args args) {
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

int wl_shm_pool_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }

int wl_surface_set_buffer_scale(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_surface *surface = self->data;
  c_wl_int scale = args[1].i;
  if (scale < 0)
    c_wl_error_set_and_return(self->id, WL_SURFACE_ERROR_INVALID_SCALE, "scale must be > 0");

  surface->pending.scale = scale;
  surface->pending.commited |= C_WL_SURFACE_STATE_SCALE;

  return 0;
}

int wl_surface_set_buffer_transform(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_surface *surface = self->data;
  c_wl_enum transform = args[1].e;

  if (transform > WL_OUTPUT_TRANSFORM_FLIPPED_270)
    c_wl_error_set_and_return(self->id, WL_SURFACE_ERROR_INVALID_TRANSFORM, "invalid transform");

  surface->pending.transform = transform;
  surface->pending.commited |= C_WL_SURFACE_STATE_TRANSFORM;

  return 0;
}

int wl_compositor_create_region(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_region_id = args[1].n;
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

int wl_region_add(struct c_wl_connection *conn, c_wl_args args) {
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

int wl_region_destroy(struct c_wl_connection *conn, c_wl_args args) {
  c_wl_object_id wl_region_id = args[0].u;
  c_wl_object_del(conn, wl_region_id);
  return 0;
}

int wl_surface_damage(struct c_wl_connection *conn, c_wl_args args) {
  c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_surface *surface = c_wl_object_get(conn, wl_surface_id)->data;;

  damage_surface(surface, args);

  return 0;
}

int wl_surface_damage_buffer(struct c_wl_connection *conn, c_wl_args args) {
    c_wl_object_id wl_surface_id = args[0].u;
  struct c_wl_surface *surface = c_wl_object_get(conn, wl_surface_id)->data;

  damage_surface(surface, args);

  return 0;
}


int wl_surface_frame(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_callback_id = args[1].n;
  struct c_wl_object *wl_callback;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_callback_id, wl_callback);

  struct c_wl_surface *surface = self->data;
  if (surface->frames_n >= LENGTH(surface->frames)) {
    c_wl_error_set_and_return(self->id, WL_DISPLAY_ERROR_IMPLEMENTATION,
                              "too many frames per surface");
  }

  surface->frames[surface->frames_n++] = wl_callback_id;
  
  const struct c_wl_interface *iface = c_wl_interface_get("wl_callback");
  c_wl_object_add(conn, wl_callback_id, iface->version, iface, NULL);
  return 0;
}

int wl_surface_destroy(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_surface *surface = self->data;

  for (size_t i = 0; i < surface->feedbacks_n; i++) {
    wp_presentation_feedback_discarded(conn, surface->feedbacks[i]);
    surface->feedbacks_n = 0;
  }

  if (surface->active.buffer) {
    c_unref(surface->active.buffer);
  }

  if (surface->pending.buffer) {
    c_unref(surface->pending.buffer);
  }

  if (surface->wlr_layer_surface) {
    surface->wlr_layer_surface->surface = NULL;
    c_unref(surface->wlr_layer_surface);
    surface->wlr_layer_surface = NULL;
    c_unref(surface);
  }

  if (surface->xdg_surface) {
      surface->xdg_surface->surface = NULL;
      c_unref(surface->xdg_surface);
      surface->xdg_surface = NULL;
      c_unref(surface);
  }

  if (surface->subsurface) {
      surface->subsurface->surface = NULL;
      c_unref(surface->subsurface);
      surface->subsurface = NULL;
      c_unref(surface);
  }

  if (surface->children) {
    struct c_wl_subsurface *ss;
    c_list_for_each(surface->children, ss) {
      ss->parent = NULL;
      c_unref(ss);
      c_unref(surface);
    }

    c_list_destroy(surface->children);
    surface->children = NULL;
  }

  if (surface->viewport) {
    surface->viewport->surface = NULL;
    c_unref(surface->viewport);
    surface->viewport = NULL;
    c_unref(surface);
  }

  c_wl_object_del(conn, self->id);
  return 0;
}

int wl_surface_set_opaque_region(struct c_wl_connection *conn, c_wl_args args) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_surface *surface = c_wl_object_get(conn, wl_surface_id)->data;

  c_wl_object_id wl_region_id = args[1].o;
  if (wl_region_id == 0) {
    memset(&surface->pending.opaque, 0, sizeof(surface->pending.opaque));
    surface->pending.commited |= C_WL_SURFACE_STATE_OPAQUE;
    return 0;
  }

  struct c_wl_object *c_wl_region;
  C_WL_CHECK_IF_REGISTERED(wl_region_id, c_wl_region);

  struct c_wl_region *region = c_wl_region->data;
  memcpy(&surface->pending.opaque, region, sizeof(surface->pending.opaque));
  surface->pending.commited |= C_WL_SURFACE_STATE_OPAQUE;
  return 0;
}

int wl_surface_set_input_region(struct c_wl_connection *conn, c_wl_args args) {
  c_wl_object_id wl_surface_id = args[0].o;
  struct c_wl_surface *surface = c_wl_object_get(conn, wl_surface_id)->data;

  c_wl_object_id wl_region_id = args[1].o;
  if (wl_region_id == 0) {
    surface->pending.input.x     = surface->pending.input.y = 0;
    surface->pending.input.width = surface->pending.input.height = -1;
    surface->pending.commited |= C_WL_SURFACE_STATE_INPUT;
    return 0;
  }

  struct c_wl_object *c_wl_region;
  C_WL_CHECK_IF_REGISTERED(wl_region_id, c_wl_region);

  struct c_wl_region *region = c_wl_region->data;
  memcpy(&surface->pending.input, region, sizeof(surface->pending.input));
  surface->pending.commited |= C_WL_SURFACE_STATE_INPUT;
  return 0;
}

int wl_surface_attach(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_surface *surface = self->data;

  c_wl_object_id buf_id = args[1].o;
  c_wl_int x = args[2].i;
  c_wl_int y = args[3].i;

  if (self->version >= 5 && (x > 0 || y > 0))
    c_wl_error_set_and_return(args[0].o, WL_SURFACE_ERROR_INVALID_OFFSET,
                              "invalid x or y", self->version);
  struct c_wl_buffer *buffer = NULL;
  if (buf_id > 0) {
    struct c_wl_object *wl_buffer;
    C_WL_CHECK_IF_REGISTERED(buf_id, wl_buffer);
    buffer = wl_buffer->data;
  }

  if (surface->pending.commited & C_WL_SURFACE_STATE_BUFFER && surface->pending.buffer) {
    c_unref(surface->pending.buffer);
  }

  surface->pending.buffer = buffer;
  if (buffer) c_ref(buffer);

  surface->pending.commited |= C_WL_SURFACE_STATE_BUFFER;

  return 0;
}

int wl_surface_commit(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_surface *surface = self->data;

  int error = 0;

  if (surface->subsurface && surface->subsurface->sync) return 0;

  if ((error = c_wl_surface_state_apply(surface))) return error;
  
  if (surface->children) {
    struct c_wl_subsurface *ss;
    c_list_for_each(surface->children, ss) {
      if (ss->sync && ((error = c_wl_surface_state_apply(ss->surface))))
        return error;
    }
  }

  return error;
}

int wl_compositor_create_surface(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_surface_id = args[1].n;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_surface_id, wl_surface);

  struct c_wl_surface *surface = c_malloc(sizeof(struct c_wl_surface));
  surface->obj = c_wl_object_add(conn, wl_surface_id, self->version,
                      c_wl_interface_get("wl_surface"), surface);

  surface->pending.scale = 1;
  surface->pending.commited |= C_WL_SURFACE_STATE_SCALE;

  surface->pending.transform = WL_OUTPUT_TRANSFORM_NORMAL;
  surface->pending.commited |= C_WL_SURFACE_STATE_TRANSFORM;

  surface->pending.input.width = -1;
  surface->pending.input.height = -1;
  surface->pending.commited |= C_WL_SURFACE_STATE_INPUT;
  return 0;
}


int wl_subcompositor_get_subsurface(struct c_wl_connection *conn, c_wl_args args) {
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
    c_unref(subsurface);
    c_wl_error_set_and_return(args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_PARENT,
                              "parent and child cannot be the same objects");
  }

  if (surface->role) {
    c_unref(subsurface);
    c_wl_error_set_and_return(args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                              "child surface already has a role");
  }

  subsurface->sync = 1;
  subsurface->parent = surface_parent;
  c_ref(surface_parent);

  surface->subsurface = subsurface;
  c_ref(subsurface);

  subsurface->surface = surface;
  c_ref(surface);

  subsurface->surface->role = C_WL_SURFACE_ROLE_SUBSURFACE;

  if (surface->children && c_list_idx(surface->children, surface_parent) != -1)
    c_wl_error_set_and_return(
        args[0].o, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
        "parent surface is one of the child's descendants");

  if (!surface_parent->children)
    surface_parent->children = c_list_new();

  c_list_push(surface_parent->children, subsurface, 0);
  c_ref(subsurface);

  subsurface->obj = c_wl_object_add(conn, wl_subsurface_id, self->version, c_wl_interface_get("wl_subsurface"), subsurface);

  return 0;
}

int wl_subsurface_set_position(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_subsurface *surface = c_wl_self(conn, args)->data;
  surface->x = args[1].i;
  surface->y = args[2].i;
  return 0;
}

int wl_subsurface_set_sync(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_subsurface *surface = c_wl_self(conn, args)->data;
  surface->sync = 1;

  if (surface->surface->children) {
    struct c_wl_subsurface *ss;
    c_list_for_each(surface->surface->children, ss) {
      c_wl_arg arg = {.o = ss->obj->id};
      wl_subsurface_set_sync(conn, &arg);
    }
  }

  return 0;
}

int wl_subsurface_set_desync(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_subsurface *surface = c_wl_self(conn, args)->data;
  surface->sync = 0;

  if (surface->surface->children) {
    struct c_wl_subsurface *ss;
    c_list_for_each(surface->surface->children, ss) {
      c_wl_arg arg = {.o = ss->obj->id};
      wl_subsurface_set_desync(conn, &arg);
    }
  }

  return 0;
}

int wl_subsurface_destroy(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_subsurface *subsurface     = c_wl_self(conn, args)->data;
  struct c_wl_surface    *surface        = subsurface->surface;
  struct c_wl_surface    *surface_parent = subsurface->parent;

  if (surface_parent) {
    c_list_remove(&surface_parent->children, subsurface);
    c_unref(subsurface);
    c_unref(surface_parent);
  }

  if (surface) {
    surface->role = 0;
    surface->subsurface = NULL;
    c_unref(surface);
    c_unref(subsurface);
  }

  c_wl_object_del(conn, args[0].o);
  return 0;
}

int wl_subsurface_place_above(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *wl_surface_sibling;
  C_WL_CHECK_IF_REGISTERED(args[1].o, wl_surface_sibling);

  struct c_wl_subsurface *surface = c_wl_self(conn, args)->data;
  struct c_wl_surface    *sibling = wl_surface_sibling->data;
  struct c_wl_surface    *parent = surface->parent;

  c_list_remove(&parent->children, surface);
  int sibling_idx = c_list_idx(parent->children, sibling);
  assert(sibling_idx != -1);

  c_list_insert(&parent->children, sibling_idx, surface, 0);
  return 0;
};

int wl_subsurface_place_below(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *wl_surface_sibling;
  C_WL_CHECK_IF_REGISTERED(args[1].o, wl_surface_sibling);

  struct c_wl_subsurface *surface = c_wl_self(conn, args)->data;
  struct c_wl_surface    *sibling = wl_surface_sibling->data;
  struct c_wl_surface    *parent = surface->parent;

  c_list_remove(&parent->children, surface);
  int sibling_idx = c_list_idx(parent->children, sibling);
  assert(sibling_idx != -1);

  c_list_insert(&parent->children, sibling_idx+1, surface, 0);
  return 0;
};

int wl_seat_release(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }

int wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_pointer_id = args[1].n;
  struct c_wl_object *wl_pointer;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_pointer_id, wl_pointer);


  struct c_wl_pointer *pointer = c_malloc(sizeof(*pointer));
  pointer->seat = self;
  pointer->obj = c_wl_object_add(conn, wl_pointer_id, self->version, c_wl_interface_get("wl_pointer"), pointer);
  return 0;
}

int wl_pointer_set_cursor(struct c_wl_connection *conn, c_wl_args args) {
  return 0;
}

int wl_pointer_release(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }

int wl_seat_get_keyboard(struct c_wl_connection *conn, union c_wl_arg *args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_keyboard_id = args[1].n;
  struct c_wl_object *wl_keyboard;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_keyboard_id, wl_keyboard);

  c_wl_object_add(conn, wl_keyboard_id, self->version, c_wl_interface_get("wl_keyboard"), NULL);

  return 0;

}

int wl_keyboard_release(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }

int wl_data_device_manager_get_data_device(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_data_device_id = args[1].n;
  struct c_wl_object *wl_data_device;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_data_device_id, wl_data_device);

  struct c_wl_object *wl_seat;
  C_WL_CHECK_IF_REGISTERED(args[2].o, wl_seat);

  struct c_wl_data_device *data_device = c_malloc(sizeof(*data_device));
  if (!data_device)
    c_wl_error_set_and_return(self->id, WL_DISPLAY_ERROR_NO_MEMORY, "failed to allocate a new data device");

  data_device->obj = c_wl_object_add(conn, wl_data_device_id, self->version, c_wl_interface_get("wl_data_device"), data_device);
  return 0;
}

int wl_data_device_set_data_source(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_device *data_device = self->data;

  c_wl_object_id wl_data_source_id = args[1].o;
  if (!wl_data_source_id && data_device->data_source) {
    c_unref(data_device->data_source);
    data_device->data_source = NULL;
    return 0;
  }

  struct c_wl_object *wl_data_source;
  C_WL_CHECK_IF_REGISTERED(wl_data_source_id, wl_data_source);

  struct c_wl_data_source *data_source = wl_data_source->data;

  if (data_device->data_source)
    c_unref(data_device->data_source);

  data_device->data_source = data_source;
  c_ref(data_source);

  return 0;
}

int wl_data_device_start_drag(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_device *data_device = self->data;

  c_wl_object_id source_id = args[1].o;
  struct c_wl_object *wl_data_source;

  c_wl_object_id origin_id = args[2].o;
  struct c_wl_object *origin_wl_surface;

  c_wl_object_id icon_id   = args[3].o;
  struct c_wl_object *icon_wl_surface;

  if (source_id) {
    C_WL_CHECK_IF_REGISTERED(source_id, wl_data_source);
    data_device->dnd.data_source = wl_data_source->data;
    c_ref(data_device->dnd.data_source);
  }

  C_WL_CHECK_IF_REGISTERED(origin_id, origin_wl_surface);
  data_device->dnd.origin = origin_wl_surface->data;
  c_ref(data_device->dnd.origin);

  if (icon_id) {
    C_WL_CHECK_IF_REGISTERED(icon_id, icon_wl_surface);
    struct c_wl_surface *surface = icon_wl_surface->data;

    if (surface->role)
      c_wl_error_set_and_return(self->id, WL_DATA_DEVICE_ERROR_ROLE, "specified surface already has a role");

    data_device->dnd.icon = surface;
    surface->role = C_WL_SURFACE_ROLE_DND_ICON;
    c_ref(surface);
  }

  return 0;
}

int wl_data_device_release(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_device *data_device = self->data;

  if (data_device->data_source) c_unref(data_device->data_source);

  if (data_device->dnd.origin) c_unref(data_device->dnd.origin);
  if (data_device->dnd.data_source) c_unref(data_device->dnd.data_source);

  if (data_device->dnd.icon)   {
    data_device->dnd.icon->role = 0;
    c_unref(data_device->dnd.icon);
  }

  c_wl_object_del(conn, self->id);
  return 0;
}

int wl_data_device_manager_create_data_source(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wl_data_source_id = args[1].n;
  struct c_wl_object *wl_data_source;
  C_WL_CHECK_IF_NOT_REGISTERED(wl_data_source_id, wl_data_source);

  struct c_wl_data_source *data_source = c_malloc(sizeof(*data_source));
  if (!data_source)
    c_wl_error_set_and_return(self->id, WL_DISPLAY_ERROR_NO_MEMORY, "failed to allocate a new data source");

  data_source->obj =
      c_wl_object_add(conn, wl_data_source_id, self->version,
                      c_wl_interface_get("wl_data_source"), data_source);

  return 0;
}

int wl_data_source_offer(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_source *data_source = self->data;

  c_wl_string mime_type = args[1].s;

  if (data_source->mimes >= LENGTH(data_source->mimetypes))
    c_wl_error_set_and_return(self->id, WL_DISPLAY_ERROR_IMPLEMENTATION, "too many mime types (max 32)");

  data_source->mimetypes[data_source->mimes++] = strdup(mime_type);
  return 0;
}

int wl_data_source_set_actions(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_source *data_source = self->data;

  c_wl_enum dnd_actions = args[1].e;
  if (dnd_actions > 0b111)
    c_wl_error_set_and_return(self->id, WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK, "invalid mask");

  if (data_source->actions)
    c_wl_error_set_and_return(self->id, WL_DATA_SOURCE_ERROR_INVALID_SOURCE,
                              "this data source already has assigned actions");

  data_source->actions = dnd_actions;
  return 0;
}

int wl_data_offer_accept(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_offer *data_offer = self->data;

  c_wl_string mimetype = args[2].s;
  if (data_offer->mimetype)
    free(data_offer->mimetype);

  if (mimetype)
    data_offer->mimetype = strdup(mimetype);
  else
    data_offer->mimetype = NULL;

  return 0;
}

int wl_data_offer_destroy(struct c_wl_connection *conn, c_wl_args args) { 
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_offer *data_offer = self->data;

  if (data_offer->mimetype)
    free(data_offer->mimetype);

  C_WL_DESTRUCTOR(conn, args); 
}

int wl_data_offer_set_actions(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_offer *data_offer = self->data;
  
  c_wl_enum dnd_actions = args[1].e;
  c_wl_enum preferred_action = args[2].e;

  if (dnd_actions > 0b111)
    c_wl_error_set_and_return(self->id, WL_DATA_OFFER_ERROR_INVALID_ACTION, "invalid dnd_actions");


  if (preferred_action & (preferred_action - 1))
    c_wl_error_set_and_return(self->id, WL_DATA_OFFER_ERROR_INVALID_ACTION_MASK, "invalid preferred_action");

  assert(data_offer->data_device);
  if (!data_offer->data_device->dnd.origin)
    c_wl_error_set_and_return(
        self->id, WL_DATA_OFFER_ERROR_INVALID_ACTION,
        "this data offer isn't associated with drag-and-drop");

  data_offer->actions = dnd_actions;
  data_offer->preferred = preferred_action;

  return 0;
}

int wl_data_source_destroy(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_source *data_source = self->data;

  for (size_t i = 0; i < data_source->mimes; i++)
    free((char *)data_source->mimetypes[i]);

  c_wl_object_del(conn, args[0].o);
  return 0;
}

int wl_output_release(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }
