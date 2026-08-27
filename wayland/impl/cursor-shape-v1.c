#include "wayland/server.h"
#include "wayland/display.h"
#include "util/malloc.h"

int wp_cursor_shape_manager_v1_get_pointer(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wp_cursor_shape_device_id = args[1].n;
  struct c_wl_object *wp_cursor_shape_device;
  C_WL_CHECK_IF_NOT_REGISTERED(wp_cursor_shape_device_id, wp_cursor_shape_device);

  c_wl_object_id wl_pointer_id = args[2].o;
  struct c_wl_object *wl_pointer;
  C_WL_CHECK_IF_REGISTERED(wl_pointer_id, wl_pointer);

  c_ref(wl_pointer->data);
  c_wl_object_add(conn, wp_cursor_shape_device_id, self->version,
                  c_wl_interface_get("wp_cursor_shape_device_v1"),
                  wl_pointer->data);

  return 0;
}

int wp_cursor_shape_manager_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }
int wp_cursor_shape_device_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }
