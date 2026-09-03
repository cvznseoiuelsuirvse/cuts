#include "wayland/server.h"
#include "wayland/types.h"

#include "wayland/display.h"
#include "wayland/proto/presentation-time.h"
#include "wayland/proto/wayland.h"
#include "wayland/impl/wayland.h"

#include "util/helpers.h"

int wp_presentation_feedback(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wp_feedback_id = args[2].n;
  struct c_wl_object *wp_feedback;
  C_WL_CHECK_IF_NOT_REGISTERED(wp_feedback_id, wp_feedback);

  c_wl_object_id wl_surface_id = args[1].o;
  struct c_wl_object *wl_surface;
  C_WL_CHECK_IF_REGISTERED(wl_surface_id, wl_surface);

  struct c_wl_surface *surface = wl_surface->data;

  if (surface->feedbacks_n >= LENGTH(surface->feedbacks)) {
    c_wl_error_set_and_return(self->id, WL_DISPLAY_ERROR_IMPLEMENTATION,
                              "too many feebacks per surface");
  }

  surface->feedbacks[surface->feedbacks_n++] = wp_feedback_id;
  c_wl_object_add(conn, wp_feedback_id, self->version, c_wl_interface_get("wp_presentation_feedback"), NULL);

  return 0;
}

int wp_presentation_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }
