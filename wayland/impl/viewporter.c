#include "wayland/proto/viewporter.h"
#include "wayland/impl/wayland.h"
#include "wayland/impl/viewporter.h"
#include "wayland/server.h"
#include "util/mem.h"

int wp_viewporter_get_viewport(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wp_viewport_id = args[1].n;
  struct c_wl_object *wp_viewport;
  C_WL_CHECK_IF_NOT_REGISTERED(wp_viewport_id, wp_viewport)

  c_wl_object_id wl_surface_id = args[2].o;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_REGISTERED(wl_surface_id, wl_surface);


  struct c_wl_surface *surface = wl_surface->data;
  if (surface->viewport) {
    c_wl_error_set_and_return(self->id, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
                              "surface#%d already has a viewport",
                              wl_surface_id);
  }

  struct c_wp_viewport *viewport = c_malloc(sizeof(*viewport));

  viewport->surface = surface;
  c_ref(surface);

  surface->viewport = viewport;
  c_ref(viewport);

  viewport->obj = c_wl_object_add(conn, wp_viewport_id, self->version, c_wl_interface_get("wp_viewport"), viewport);
  return 0;
}

int wp_viewporter_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }


int wp_viewport_set_source(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *wp_viewport = c_wl_self(conn, args); 
  struct c_wp_viewport *viewport = wp_viewport->data;

  if (!viewport->surface) {
    c_wl_error_set_and_return(wp_viewport->id, WP_VIEWPORT_ERROR_NO_SURFACE, "associated wl_surface was already destroyed");
  }

  double x      = C_WL_FIXED_TO_DOUBLE(args[1].f);
  double y      = C_WL_FIXED_TO_DOUBLE(args[2].f);
  double width  = C_WL_FIXED_TO_DOUBLE(args[3].f);
  double height = C_WL_FIXED_TO_DOUBLE(args[4].f);

  if (x == -1 && y == -1 && width == -1 && height == -1) {
    goto out;
  } else if (x < 0 || y < 0) {
    c_wl_error_set_and_return(wp_viewport->id, WP_VIEWPORT_ERROR_BAD_SIZE, "x or y are negative");

  } else if (width <= 0 || height <= 0) {
    c_wl_error_set_and_return(wp_viewport->id, WP_VIEWPORT_ERROR_BAD_VALUE, "width or height are equal or less than 0");
  }

out:
  viewport->src.x = x;
  viewport->src.y = y;
  viewport->src.width = width;
  viewport->src.height = height;

  return 0;
}

int wp_viewport_set_destination(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *wp_viewport = c_wl_self(conn, args); 
  struct c_wp_viewport *viewport = wp_viewport->data;

  if (!viewport->surface) {
    c_wl_error_set_and_return(wp_viewport->id, WP_VIEWPORT_ERROR_NO_SURFACE, "associated wl_surface was already destroyed");
  }

  c_wl_int width  = args[1].i;
  c_wl_int height = args[2].i;

  if (width == -1 && height == -1) {
    goto out;
  } else if (width <= 0 || height <= 0) {
    c_wl_error_set_and_return(wp_viewport->id, WP_VIEWPORT_ERROR_BAD_VALUE, "width or height are equal or less than 0");
  }

out:
  viewport->dst.width = width;
  viewport->dst.height = height;

  return 0;
}


int wp_viewport_destroy(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *wp_viewport = c_wl_self(conn, args); 
  struct c_wp_viewport *viewport = wp_viewport->data;

  if (viewport->surface) {
    viewport->surface->viewport = NULL;
    c_unref(viewport->surface);
    viewport->surface = NULL;
    c_unref(viewport);
  }

  C_WL_DESTRUCTOR(conn, args);
}
