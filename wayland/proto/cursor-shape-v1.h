#ifndef CUTS_CURSOR_SHAPE_V1_H
#define CUTS_CURSOR_SHAPE_V1_H

#include <stdint.h>

#include "wayland/server.h"
#include "wayland/display.h"
#include "wayland/types.h"


 /* This enum describes cursor shapes.

        The names are taken from the CSS W3C specification:
        https://w3c.github.io/csswg-drafts/css-ui/#cursor
        with a few additions.

        Note that there are some groups of cursor shapes that are related:
        The first group is drag-and-drop cursors which are used to indicate
        the selected action during dnd operations. The second group is resize
        cursors which are used to indicate resizing and moving possibilities
        on window borders. It is recommended that the shapes in these groups
        should use visually compatible images and metaphors. */
enum wp_cursor_shape_device_v1_shape_enum {
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT = 1,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU = 2,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP = 3,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER = 4,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS = 5,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT = 6,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL = 7,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR = 8,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT = 9,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT = 10,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS = 11,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY = 12,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE = 13,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP = 14,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED = 15,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB = 16,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING = 17,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE = 18,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE = 19,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE = 20,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE = 21,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE = 22,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE = 23,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE = 24,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE = 25,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE = 26,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE = 27,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE = 28,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE = 29,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE = 30,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE = 31,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL = 32,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN = 33,
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT = 34,
#define C_WP_CURSOR_SHAPE_DEVICE_V1_DND_ASK_SINCE 2
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DND_ASK = 35,
#define C_WP_CURSOR_SHAPE_DEVICE_V1_ALL_RESIZE_SINCE 2
  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_RESIZE = 36,
};

enum wp_cursor_shape_device_v1_error_enum {
  WP_CURSOR_SHAPE_DEVICE_V1_ERROR_INVALID_SHAPE = 1,
};

   /* Destroy the cursor shape manager. */
C_WL_REQUEST wp_cursor_shape_manager_v1_destroy(struct c_wl_connection *conn, c_wl_args args);

   /* Obtain a wp_cursor_shape_device_v1 for a wl_pointer object.

        When the pointer capability is removed from the wl_seat, the
        wp_cursor_shape_device_v1 object becomes inert.

    @[1] cursor_shape_device: c_wl_new_id [[wp_cursor_shape_device_v1]]
    @[2] pointer: c_wl_object_id [[wl_pointer]]
   */
C_WL_REQUEST wp_cursor_shape_manager_v1_get_pointer(struct c_wl_connection *conn, c_wl_args args);

   /* Obtain a wp_cursor_shape_device_v1 for a zwp_tablet_tool_v2 object.

        When the zwp_tablet_tool_v2 is removed, the wp_cursor_shape_device_v1
        object becomes inert.

    @[1] cursor_shape_device: c_wl_new_id [[wp_cursor_shape_device_v1]]
    @[2] tablet_tool: c_wl_object_id [[zwp_tablet_tool_v2]]
   */
C_WL_REQUEST wp_cursor_shape_manager_v1_get_tablet_tool_v2(struct c_wl_connection *conn, c_wl_args args);

struct c_wp_cursor_shape_manager_v1_listeners {
  c_wl_interface_listener_handler destroy;
  c_wl_interface_listener_handler get_pointer;
  c_wl_interface_listener_handler get_tablet_tool_v2;
};
void wp_cursor_shape_manager_v1_add_listener(struct c_wl_display *display, struct c_wp_cursor_shape_manager_v1_listeners *listeners, void *userdata);

   /* Destroy the cursor shape device.

        The device cursor shape remains unchanged. */
C_WL_REQUEST wp_cursor_shape_device_v1_destroy(struct c_wl_connection *conn, c_wl_args args);

   /* Sets the device cursor to the specified shape. The compositor will
        change the cursor image based on the specified shape.

        The cursor actually changes only if the input device focus is one of
        the requesting client's surfaces. If any, the previous cursor image
        (surface or shape) is replaced.

        The "shape" argument must be a valid enum entry, otherwise the
        invalid_shape protocol error is raised.

        This is similar to the wl_pointer.set_cursor and
        zwp_tablet_tool_v2.set_cursor requests, but this request accepts a
        shape instead of contents in the form of a surface. Clients can mix
        set_cursor and set_shape requests.

        The serial parameter must match the latest wl_pointer.enter or
        zwp_tablet_tool_v2.proximity_in serial number sent to the client.
        Otherwise the request will be ignored.

    @[1] serial: c_wl_uint
    @[2] shape: enum wp_cursor_shape_device_v1_shape_enum
   */
C_WL_REQUEST wp_cursor_shape_device_v1_set_shape(struct c_wl_connection *conn, c_wl_args args);

struct c_wp_cursor_shape_device_v1_listeners {
  c_wl_interface_listener_handler destroy;
  c_wl_interface_listener_handler set_shape;
};
void wp_cursor_shape_device_v1_add_listener(struct c_wl_display *display, struct c_wp_cursor_shape_device_v1_listeners *listeners, void *userdata);

#endif