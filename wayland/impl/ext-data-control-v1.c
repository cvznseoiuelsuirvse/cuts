#include <stdlib.h>

#include "wayland/proto/wayland.h"
#include "wayland/proto/ext-data-control-v1.h"
#include "wayland/impl/ext-data-control-v1.h"
#include "wayland/server.h"
#include "util/mem.h"
#include "util/helpers.h"

int ext_data_control_manager_v1_get_data_device(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wlr_data_device_id = args[1].n;
  struct c_wl_object *wlr_data_device;
  C_WL_CHECK_IF_NOT_REGISTERED(wlr_data_device_id, wlr_data_device);

  struct c_wl_object *wl_seat;
  C_WL_CHECK_IF_REGISTERED(args[2].o, wl_seat);

  struct c_ext_data_control_device *data_device = c_malloc(sizeof(*data_device));
  if (!data_device)
    c_wl_error_set_and_return(self->id, WL_DISPLAY_ERROR_NO_MEMORY, "failed to allocate a new data device");

  data_device->obj =
      c_wl_object_add(conn, wlr_data_device_id, self->version,
                      c_wl_interface_get("ext_data_control_device_v1"), data_device);
  return 0;
}

int ext_data_control_device_v1_set_primary_selection(struct c_wl_connection *conn, c_wl_args args) { return 0;}

int ext_data_control_device_v1_destroy(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_ext_data_control_device *data_device = self->data;

  if (data_device->selection) c_unref(data_device->selection);
  if (data_device->primary_selection) c_unref(data_device->primary_selection);

  c_wl_object_del(conn, self->id);
  return 0;
}

int ext_data_control_manager_v1_create_data_source(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);

  c_wl_new_id wlr_data_source_id = args[1].n;
  struct c_wl_object *wlr_data_source;
  C_WL_CHECK_IF_NOT_REGISTERED(wlr_data_source_id, wlr_data_source);

  struct c_ext_data_control_source *data_source = c_malloc(sizeof(*data_source));
  if (!data_source)
    c_wl_error_set_and_return(self->id, WL_DISPLAY_ERROR_NO_MEMORY, "failed to allocate a new data source");

  data_source->obj =
      c_wl_object_add(conn, wlr_data_source_id, self->version,
                      c_wl_interface_get("ext_data_control_source_v1"), data_source);

  return 0;
}

int ext_data_control_source_v1_offer(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_ext_data_control_source *data_source = self->data;

  c_wl_string mime_type = args[1].s;

  if (data_source->mimes >= LENGTH(data_source->mimetypes))
    c_wl_error_set_and_return(self->id, WL_DISPLAY_ERROR_IMPLEMENTATION, "too many mime types (max 32)");

  data_source->mimetypes[data_source->mimes++] = strdup(mime_type);
  return 0;

}

int ext_data_control_source_v1_destroy(struct c_wl_connection *conn, c_wl_args args) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_ext_data_control_source *data_source = self->data;

  for (size_t i = 0; i < data_source->mimes; i++)
    free((char *)data_source->mimetypes[i]);

  c_wl_object_del(conn, args[0].o);
  return 0;
}


int ext_data_control_offer_v1_destroy(struct c_wl_connection *conn, c_wl_args args) { C_WL_DESTRUCTOR(conn, args); }
