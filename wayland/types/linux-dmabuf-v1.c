#include "wayland/server.h"
#include "wayland/types/linux-dmabuf-v1.h"

#include <stdint.h>


C_WL_EVENT zwp_linux_dmabuf_v1_format(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_v1, c_wl_uint format) {
  struct c_wl_message msg = {zwp_linux_dmabuf_v1, 0, "u", "format"};
  return c_wl_connection_send(conn, &msg, 1, format);
}
C_WL_EVENT zwp_linux_dmabuf_v1_modifier(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_v1, c_wl_uint format, c_wl_uint modifier_hi, c_wl_uint modifier_lo) {
  struct c_wl_message msg = {zwp_linux_dmabuf_v1, 1, "uuu", "modifier"};
  return c_wl_connection_send(conn, &msg, 3, format, modifier_hi, modifier_lo);
}
C_WL_REQUEST zwp_linux_dmabuf_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST zwp_linux_dmabuf_v1_create_params(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST zwp_linux_dmabuf_v1_get_default_feedback(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST zwp_linux_dmabuf_v1_get_surface_feedback(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(zwp_linux_dmabuf_v1_interface, "zwp_linux_dmabuf_v1", 5, 4, 
    {"destroy",                zwp_linux_dmabuf_v1_destroy,  NULL, 0,  {0}},
    {"create_params",          zwp_linux_dmabuf_v1_create_params,NULL, 1,  "n"},
    {"get_default_feedback",   zwp_linux_dmabuf_v1_get_default_feedback,NULL, 1,  "n"},
    {"get_surface_feedback",   zwp_linux_dmabuf_v1_get_surface_feedback,NULL, 2,  "no"},
)

C_WL_EVENT zwp_linux_buffer_params_v1_created(struct c_wl_connection *conn, c_wl_object_id zwp_linux_buffer_params_v1, c_wl_new_id wl_buffer) {
  struct c_wl_message msg = {zwp_linux_buffer_params_v1, 0, "n", "created"};
  return c_wl_connection_send(conn, &msg, 1, wl_buffer);
}
C_WL_EVENT zwp_linux_buffer_params_v1_failed(struct c_wl_connection *conn, c_wl_object_id zwp_linux_buffer_params_v1) {
  struct c_wl_message msg = {zwp_linux_buffer_params_v1, 1, {0}, "failed"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_REQUEST zwp_linux_buffer_params_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST zwp_linux_buffer_params_v1_add(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST zwp_linux_buffer_params_v1_create(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST zwp_linux_buffer_params_v1_create_immed(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(zwp_linux_buffer_params_v1_interface, "zwp_linux_buffer_params_v1", 5, 4, 
    {"destroy",                zwp_linux_buffer_params_v1_destroy,NULL, 0,  {0}},
    {"add",                    zwp_linux_buffer_params_v1_add,NULL, 6,  "Fuuuuu"},
    {"create",                 zwp_linux_buffer_params_v1_create,NULL, 4,  "iiuu"},
    {"create_immed",           zwp_linux_buffer_params_v1_create_immed,NULL, 5,  "niiuu"},
)

C_WL_EVENT zwp_linux_dmabuf_feedback_v1_done(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_feedback_v1) {
  struct c_wl_message msg = {zwp_linux_dmabuf_feedback_v1, 0, {0}, "done"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT zwp_linux_dmabuf_feedback_v1_format_table(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_feedback_v1, c_wl_fd fd, c_wl_uint size) {
  struct c_wl_message msg = {zwp_linux_dmabuf_feedback_v1, 1, "Fu", "format_table"};
  return c_wl_connection_send(conn, &msg, 2, fd, size);
}
C_WL_EVENT zwp_linux_dmabuf_feedback_v1_main_device(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_feedback_v1, c_wl_array *device) {
  struct c_wl_message msg = {zwp_linux_dmabuf_feedback_v1, 2, "a", "main_device"};
  return c_wl_connection_send(conn, &msg, 1, device);
}
C_WL_EVENT zwp_linux_dmabuf_feedback_v1_tranche_done(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_feedback_v1) {
  struct c_wl_message msg = {zwp_linux_dmabuf_feedback_v1, 3, {0}, "tranche_done"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT zwp_linux_dmabuf_feedback_v1_tranche_target_device(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_feedback_v1, c_wl_array *device) {
  struct c_wl_message msg = {zwp_linux_dmabuf_feedback_v1, 4, "a", "tranche_target_device"};
  return c_wl_connection_send(conn, &msg, 1, device);
}
C_WL_EVENT zwp_linux_dmabuf_feedback_v1_tranche_formats(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_feedback_v1, c_wl_array *indices) {
  struct c_wl_message msg = {zwp_linux_dmabuf_feedback_v1, 5, "a", "tranche_formats"};
  return c_wl_connection_send(conn, &msg, 1, indices);
}
C_WL_EVENT zwp_linux_dmabuf_feedback_v1_tranche_flags(struct c_wl_connection *conn, c_wl_object_id zwp_linux_dmabuf_feedback_v1, enum zwp_linux_dmabuf_feedback_v1_tranche_flags_enum flags) {
  struct c_wl_message msg = {zwp_linux_dmabuf_feedback_v1, 6, "u", "tranche_flags"};
  return c_wl_connection_send(conn, &msg, 1, flags);
}
C_WL_REQUEST zwp_linux_dmabuf_feedback_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(zwp_linux_dmabuf_feedback_v1_interface, "zwp_linux_dmabuf_feedback_v1", 5, 1, 
    {"destroy",                zwp_linux_dmabuf_feedback_v1_destroy,NULL, 0,  {0}},
)

