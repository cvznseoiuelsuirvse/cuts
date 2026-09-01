#include "wayland/server.h"
#include "wayland/impl/wayland.h"
#include "wayland/proto/fractional-scale-v1.h"
#include "util/mem.h"

int wp_fractional_scale_manager_v1_get_fractional_scale(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wp_fractional_scale_id = args[1].n;
  struct c_wl_object *wp_fractional_scale;
  C_WL_CHECK_IF_NOT_REGISTERED(wp_fractional_scale_id, wp_fractional_scale);

  c_wl_object_id wl_surface_id = args[2].o;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_REGISTERED(wl_surface_id, wl_surface);

  struct c_wl_surface *surface = wl_surface->data;
  if (surface->fscale)
    c_wl_error_set_and_return(
        args[0].o, WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS,
        "this surface already has fractional scale of %f",
        surface->fscale / 120.0f);

  c_wl_object_add(conn, wp_fractional_scale_id, self->id, c_wl_interface_get("wp_fractional_scale_v1"), surface);
  c_ref(surface);

  return 0;
}
int wp_fractional_scale_manager_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }


int wp_fractional_scale_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { 
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_surface *surface = self->data;

  surface->fscale = 0;
  C_WL_DESTRUCTOR(conn, args);
}
