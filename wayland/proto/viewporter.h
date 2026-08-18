#ifndef CUTS_VIEWPORTER_H
#define CUTS_VIEWPORTER_H

#include <stdint.h>

#include "wayland/server.h"
#include "wayland/display.h"
#include "wayland/types.h"


enum wp_viewporter_error_enum {
  WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS = 0,
};

enum wp_viewport_error_enum {
  WP_VIEWPORT_ERROR_BAD_VALUE = 0,
  WP_VIEWPORT_ERROR_BAD_SIZE = 1,
  WP_VIEWPORT_ERROR_OUT_OF_BUFFER = 2,
  WP_VIEWPORT_ERROR_NO_SURFACE = 3,
};

   /* Informs the server that the client will not be using this
	protocol object anymore. This does not affect any other objects,
	wp_viewport objects included. */
C_WL_REQUEST wp_viewporter_destroy(struct c_wl_connection *conn, c_wl_args args);

   /* Instantiate an interface extension for the given wl_surface to
	crop and scale its content. If the given wl_surface already has
	a wp_viewport object associated, the viewport_exists
	protocol error is raised.

    @[1] id: c_wl_new_id [[wp_viewport]]
    @[2] surface: c_wl_object_id [[wl_surface]]
   */
C_WL_REQUEST wp_viewporter_get_viewport(struct c_wl_connection *conn, c_wl_args args);

struct c_wp_viewporter_listeners {
  c_wl_interface_listener_handler destroy;
  c_wl_interface_listener_handler get_viewport;
};
void wp_viewporter_add_listener(struct c_wl_display *display, struct c_wp_viewporter_listeners *listeners, void *userdata);

   /* The associated wl_surface's crop and scale state is removed.
	The change is applied on the next wl_surface.commit. */
C_WL_REQUEST wp_viewport_destroy(struct c_wl_connection *conn, c_wl_args args);

   /* Set the source rectangle of the associated wl_surface. See
	wp_viewport for the description, and relation to the wl_buffer
	size.

	If all of x, y, width and height are -1.0, the source rectangle is
	unset instead. Any other set of values where width or height are zero
	or negative, or x or y are negative, raise the bad_value protocol
	error.

	The crop and scale state is double-buffered, see wl_surface.commit.

    @[1] x: c_wl_fixed
    @[2] y: c_wl_fixed
    @[3] width: c_wl_fixed
    @[4] height: c_wl_fixed
   */
C_WL_REQUEST wp_viewport_set_source(struct c_wl_connection *conn, c_wl_args args);

   /* Set the destination size of the associated wl_surface. See
	wp_viewport for the description, and relation to the wl_buffer
	size.

	If width is -1 and height is -1, the destination size is unset
	instead. Any other pair of values for width and height that
	contains zero or negative values raises the bad_value protocol
	error.

	The crop and scale state is double-buffered, see wl_surface.commit.

    @[1] width: c_wl_int
    @[2] height: c_wl_int
   */
C_WL_REQUEST wp_viewport_set_destination(struct c_wl_connection *conn, c_wl_args args);

struct c_wp_viewport_listeners {
  c_wl_interface_listener_handler destroy;
  c_wl_interface_listener_handler set_source;
  c_wl_interface_listener_handler set_destination;
};
void wp_viewport_add_listener(struct c_wl_display *display, struct c_wp_viewport_listeners *listeners, void *userdata);

#endif