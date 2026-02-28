#include "wayland/server.h"
#include "wayland/types/wayland.h"

#include <stdint.h>


C_WL_EVENT wl_display_error(struct c_wl_connection *conn, c_wl_object_id wl_display, c_wl_object_id object_id, c_wl_uint code, c_wl_string message) {
  struct c_wl_message msg = {wl_display, 0, "ous", "error"};
  return c_wl_connection_send(conn, &msg, 3, object_id, code, message);
}
C_WL_EVENT wl_display_delete_id(struct c_wl_connection *conn, c_wl_object_id wl_display, c_wl_uint id) {
  struct c_wl_message msg = {wl_display, 1, "u", "delete_id"};
  return c_wl_connection_send(conn, &msg, 1, id);
}
C_WL_REQUEST wl_display_sync(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_display_get_registry(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_display_interface, "wl_display", 1, 2, 
    {"sync",                   wl_display_sync,              NULL, 1,  "n"},
    {"get_registry",           wl_display_get_registry,      NULL, 1,  "n"},
)

C_WL_EVENT wl_registry_global(struct c_wl_connection *conn, c_wl_object_id wl_registry, c_wl_uint name, c_wl_string interface, c_wl_uint version) {
  struct c_wl_message msg = {wl_registry, 0, "usu", "global"};
  return c_wl_connection_send(conn, &msg, 3, name, interface, version);
}
C_WL_EVENT wl_registry_global_remove(struct c_wl_connection *conn, c_wl_object_id wl_registry, c_wl_uint name) {
  struct c_wl_message msg = {wl_registry, 1, "u", "global_remove"};
  return c_wl_connection_send(conn, &msg, 1, name);
}
C_WL_REQUEST wl_registry_bind(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_registry_interface, "wl_registry", 1, 1, 
    {"bind",                   wl_registry_bind,             NULL, 4,  "usun"},
)

C_WL_EVENT wl_callback_done(struct c_wl_connection *conn, c_wl_object_id wl_callback, c_wl_uint callback_data) {
  struct c_wl_message msg = {wl_callback, 0, "u", "done"};
  return c_wl_connection_send(conn, &msg, 1, callback_data);
}

C_WL_INTERFACE_REGISTER(wl_callback_interface, "wl_callback", 1, 0);

C_WL_REQUEST wl_compositor_create_surface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_compositor_create_region(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_compositor_interface, "wl_compositor", 6, 2, 
    {"create_surface",         wl_compositor_create_surface, NULL, 1,  "n"},
    {"create_region",          wl_compositor_create_region,  NULL, 1,  "n"},
)

C_WL_REQUEST wl_shm_pool_create_buffer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shm_pool_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shm_pool_resize(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_shm_pool_interface, "wl_shm_pool", 2, 3, 
    {"create_buffer",          wl_shm_pool_create_buffer,    NULL, 6,  "niiiiu"},
    {"destroy",                wl_shm_pool_destroy,          NULL, 0,  {0}},
    {"resize",                 wl_shm_pool_resize,           NULL, 1,  "i"},
)

C_WL_EVENT wl_shm_format(struct c_wl_connection *conn, c_wl_object_id wl_shm, enum wl_shm_format_enum format) {
  struct c_wl_message msg = {wl_shm, 0, "u", "format"};
  return c_wl_connection_send(conn, &msg, 1, format);
}
C_WL_REQUEST wl_shm_create_pool(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shm_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_shm_interface, "wl_shm", 2, 2, 
    {"create_pool",            wl_shm_create_pool,           NULL, 3,  "nFi"},
    {"release",                wl_shm_release,               NULL, 0,  {0}},
)

C_WL_EVENT wl_buffer_release(struct c_wl_connection *conn, c_wl_object_id wl_buffer) {
  struct c_wl_message msg = {wl_buffer, 0, {0}, "release"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_REQUEST wl_buffer_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_buffer_interface, "wl_buffer", 1, 1, 
    {"destroy",                wl_buffer_destroy,            NULL, 0,  {0}},
)

C_WL_EVENT wl_data_offer_offer(struct c_wl_connection *conn, c_wl_object_id wl_data_offer, c_wl_string mime_type) {
  struct c_wl_message msg = {wl_data_offer, 0, "s", "offer"};
  return c_wl_connection_send(conn, &msg, 1, mime_type);
}
C_WL_EVENT wl_data_offer_source_actions(struct c_wl_connection *conn, c_wl_object_id wl_data_offer, enum wl_data_device_manager_dnd_action_enum source_actions) {
  struct c_wl_message msg = {wl_data_offer, 1, "u", "source_actions"};
  return c_wl_connection_send(conn, &msg, 1, source_actions);
}
C_WL_EVENT wl_data_offer_action(struct c_wl_connection *conn, c_wl_object_id wl_data_offer, enum wl_data_device_manager_dnd_action_enum dnd_action) {
  struct c_wl_message msg = {wl_data_offer, 2, "u", "action"};
  return c_wl_connection_send(conn, &msg, 1, dnd_action);
}
C_WL_REQUEST wl_data_offer_accept(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_offer_receive(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_offer_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_offer_finish(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_offer_set_actions(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_data_offer_interface, "wl_data_offer", 3, 5, 
    {"accept",                 wl_data_offer_accept,         NULL, 2,  "us"},
    {"receive",                wl_data_offer_receive,        NULL, 2,  "sF"},
    {"destroy",                wl_data_offer_destroy,        NULL, 0,  {0}},
    {"finish",                 wl_data_offer_finish,         NULL, 0,  {0}},
    {"set_actions",            wl_data_offer_set_actions,    NULL, 2,  "uu"},
)

C_WL_EVENT wl_data_source_target(struct c_wl_connection *conn, c_wl_object_id wl_data_source, c_wl_string mime_type) {
  struct c_wl_message msg = {wl_data_source, 0, "s", "target"};
  return c_wl_connection_send(conn, &msg, 1, mime_type);
}
C_WL_EVENT wl_data_source_send(struct c_wl_connection *conn, c_wl_object_id wl_data_source, c_wl_string mime_type, c_wl_fd fd) {
  struct c_wl_message msg = {wl_data_source, 1, "sF", "send"};
  return c_wl_connection_send(conn, &msg, 2, mime_type, fd);
}
C_WL_EVENT wl_data_source_cancelled(struct c_wl_connection *conn, c_wl_object_id wl_data_source) {
  struct c_wl_message msg = {wl_data_source, 2, {0}, "cancelled"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_data_source_dnd_drop_performed(struct c_wl_connection *conn, c_wl_object_id wl_data_source) {
  struct c_wl_message msg = {wl_data_source, 3, {0}, "dnd_drop_performed"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_data_source_dnd_finished(struct c_wl_connection *conn, c_wl_object_id wl_data_source) {
  struct c_wl_message msg = {wl_data_source, 4, {0}, "dnd_finished"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_data_source_action(struct c_wl_connection *conn, c_wl_object_id wl_data_source, enum wl_data_device_manager_dnd_action_enum dnd_action) {
  struct c_wl_message msg = {wl_data_source, 5, "u", "action"};
  return c_wl_connection_send(conn, &msg, 1, dnd_action);
}
C_WL_REQUEST wl_data_source_offer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_source_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_source_set_actions(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_data_source_interface, "wl_data_source", 3, 3, 
    {"offer",                  wl_data_source_offer,         NULL, 1,  "s"},
    {"destroy",                wl_data_source_destroy,       NULL, 0,  {0}},
    {"set_actions",            wl_data_source_set_actions,   NULL, 1,  "u"},
)

C_WL_EVENT wl_data_device_data_offer(struct c_wl_connection *conn, c_wl_object_id wl_data_device, c_wl_new_id wl_data_offer) {
  struct c_wl_message msg = {wl_data_device, 0, "n", "data_offer"};
  return c_wl_connection_send(conn, &msg, 1, wl_data_offer);
}
C_WL_EVENT wl_data_device_enter(struct c_wl_connection *conn, c_wl_object_id wl_data_device, c_wl_uint serial, c_wl_object_id wl_surface, c_wl_fixed x, c_wl_fixed y, c_wl_object_id wl_data_offer) {
  struct c_wl_message msg = {wl_data_device, 1, "uoffo", "enter"};
  return c_wl_connection_send(conn, &msg, 5, serial, wl_surface, x, y, wl_data_offer);
}
C_WL_EVENT wl_data_device_leave(struct c_wl_connection *conn, c_wl_object_id wl_data_device) {
  struct c_wl_message msg = {wl_data_device, 2, {0}, "leave"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_data_device_motion(struct c_wl_connection *conn, c_wl_object_id wl_data_device, c_wl_uint time, c_wl_fixed x, c_wl_fixed y) {
  struct c_wl_message msg = {wl_data_device, 3, "uff", "motion"};
  return c_wl_connection_send(conn, &msg, 3, time, x, y);
}
C_WL_EVENT wl_data_device_drop(struct c_wl_connection *conn, c_wl_object_id wl_data_device) {
  struct c_wl_message msg = {wl_data_device, 4, {0}, "drop"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_data_device_selection(struct c_wl_connection *conn, c_wl_object_id wl_data_device, c_wl_object_id wl_data_offer) {
  struct c_wl_message msg = {wl_data_device, 5, "o", "selection"};
  return c_wl_connection_send(conn, &msg, 1, wl_data_offer);
}
C_WL_REQUEST wl_data_device_start_drag(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_device_set_selection(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_device_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_data_device_interface, "wl_data_device", 3, 3, 
    {"start_drag",             wl_data_device_start_drag,    NULL, 4,  "ooou"},
    {"set_selection",          wl_data_device_set_selection, NULL, 2,  "ou"},
    {"release",                wl_data_device_release,       NULL, 0,  {0}},
)

C_WL_REQUEST wl_data_device_manager_create_data_source(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_data_device_manager_get_data_device(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_data_device_manager_interface, "wl_data_device_manager", 3, 2, 
    {"create_data_source",     wl_data_device_manager_create_data_source,NULL, 1,  "n"},
    {"get_data_device",        wl_data_device_manager_get_data_device,NULL, 2,  "no"},
)

C_WL_REQUEST wl_shell_get_shell_surface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_shell_interface, "wl_shell", 1, 1, 
    {"get_shell_surface",      wl_shell_get_shell_surface,   NULL, 2,  "no"},
)

C_WL_EVENT wl_shell_surface_ping(struct c_wl_connection *conn, c_wl_object_id wl_shell_surface, c_wl_uint serial) {
  struct c_wl_message msg = {wl_shell_surface, 0, "u", "ping"};
  return c_wl_connection_send(conn, &msg, 1, serial);
}
C_WL_EVENT wl_shell_surface_configure(struct c_wl_connection *conn, c_wl_object_id wl_shell_surface, enum wl_shell_surface_resize_enum edges, c_wl_int width, c_wl_int height) {
  struct c_wl_message msg = {wl_shell_surface, 1, "uii", "configure"};
  return c_wl_connection_send(conn, &msg, 3, edges, width, height);
}
C_WL_EVENT wl_shell_surface_popup_done(struct c_wl_connection *conn, c_wl_object_id wl_shell_surface) {
  struct c_wl_message msg = {wl_shell_surface, 2, {0}, "popup_done"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_REQUEST wl_shell_surface_pong(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_move(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_resize(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_set_toplevel(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_set_transient(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_set_fullscreen(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_set_popup(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_set_maximized(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_set_title(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_shell_surface_set_class(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_shell_surface_interface, "wl_shell_surface", 1, 10, 
    {"pong",                   wl_shell_surface_pong,        NULL, 1,  "u"},
    {"move",                   wl_shell_surface_move,        NULL, 2,  "ou"},
    {"resize",                 wl_shell_surface_resize,      NULL, 3,  "ouu"},
    {"set_toplevel",           wl_shell_surface_set_toplevel,NULL, 0,  {0}},
    {"set_transient",          wl_shell_surface_set_transient,NULL, 4,  "oiiu"},
    {"set_fullscreen",         wl_shell_surface_set_fullscreen,NULL, 3,  "uuo"},
    {"set_popup",              wl_shell_surface_set_popup,   NULL, 6,  "ouoiiu"},
    {"set_maximized",          wl_shell_surface_set_maximized,NULL, 1,  "o"},
    {"set_title",              wl_shell_surface_set_title,   NULL, 1,  "s"},
    {"set_class",              wl_shell_surface_set_class,   NULL, 1,  "s"},
)

C_WL_EVENT wl_surface_enter(struct c_wl_connection *conn, c_wl_object_id wl_surface, c_wl_object_id wl_output) {
  struct c_wl_message msg = {wl_surface, 0, "o", "enter"};
  return c_wl_connection_send(conn, &msg, 1, wl_output);
}
C_WL_EVENT wl_surface_leave(struct c_wl_connection *conn, c_wl_object_id wl_surface, c_wl_object_id wl_output) {
  struct c_wl_message msg = {wl_surface, 1, "o", "leave"};
  return c_wl_connection_send(conn, &msg, 1, wl_output);
}
C_WL_EVENT wl_surface_preferred_buffer_scale(struct c_wl_connection *conn, c_wl_object_id wl_surface, c_wl_int factor) {
  struct c_wl_message msg = {wl_surface, 2, "i", "preferred_buffer_scale"};
  return c_wl_connection_send(conn, &msg, 1, factor);
}
C_WL_EVENT wl_surface_preferred_buffer_transform(struct c_wl_connection *conn, c_wl_object_id wl_surface, enum wl_output_transform_enum transform) {
  struct c_wl_message msg = {wl_surface, 3, "u", "preferred_buffer_transform"};
  return c_wl_connection_send(conn, &msg, 1, transform);
}
C_WL_REQUEST wl_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_attach(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_damage(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_frame(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_set_opaque_region(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_set_input_region(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_commit(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_set_buffer_transform(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_set_buffer_scale(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_damage_buffer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_surface_offset(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_surface_interface, "wl_surface", 6, 11, 
    {"destroy",                wl_surface_destroy,           NULL, 0,  {0}},
    {"attach",                 wl_surface_attach,            NULL, 3,  "oii"},
    {"damage",                 wl_surface_damage,            NULL, 4,  "iiii"},
    {"frame",                  wl_surface_frame,             NULL, 1,  "n"},
    {"set_opaque_region",      wl_surface_set_opaque_region, NULL, 1,  "o"},
    {"set_input_region",       wl_surface_set_input_region,  NULL, 1,  "o"},
    {"commit",                 wl_surface_commit,            NULL, 0,  {0}},
    {"set_buffer_transform",   wl_surface_set_buffer_transform,NULL, 1,  "i"},
    {"set_buffer_scale",       wl_surface_set_buffer_scale,  NULL, 1,  "i"},
    {"damage_buffer",          wl_surface_damage_buffer,     NULL, 4,  "iiii"},
    {"offset",                 wl_surface_offset,            NULL, 2,  "ii"},
)

C_WL_EVENT wl_seat_capabilities(struct c_wl_connection *conn, c_wl_object_id wl_seat, enum wl_seat_capability_enum capabilities) {
  struct c_wl_message msg = {wl_seat, 0, "u", "capabilities"};
  return c_wl_connection_send(conn, &msg, 1, capabilities);
}
C_WL_EVENT wl_seat_name(struct c_wl_connection *conn, c_wl_object_id wl_seat, c_wl_string name) {
  struct c_wl_message msg = {wl_seat, 1, "s", "name"};
  return c_wl_connection_send(conn, &msg, 1, name);
}
C_WL_REQUEST wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_seat_get_keyboard(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_seat_get_touch(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_seat_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_seat_interface, "wl_seat", 10, 4, 
    {"get_pointer",            wl_seat_get_pointer,          NULL, 1,  "n"},
    {"get_keyboard",           wl_seat_get_keyboard,         NULL, 1,  "n"},
    {"get_touch",              wl_seat_get_touch,            NULL, 1,  "n"},
    {"release",                wl_seat_release,              NULL, 0,  {0}},
)

C_WL_EVENT wl_pointer_enter(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint serial, c_wl_object_id wl_surface, c_wl_fixed surface_x, c_wl_fixed surface_y) {
  struct c_wl_message msg = {wl_pointer, 0, "uoff", "enter"};
  return c_wl_connection_send(conn, &msg, 4, serial, wl_surface, surface_x, surface_y);
}
C_WL_EVENT wl_pointer_leave(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint serial, c_wl_object_id wl_surface) {
  struct c_wl_message msg = {wl_pointer, 1, "uo", "leave"};
  return c_wl_connection_send(conn, &msg, 2, serial, wl_surface);
}
C_WL_EVENT wl_pointer_motion(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint time, c_wl_fixed surface_x, c_wl_fixed surface_y) {
  struct c_wl_message msg = {wl_pointer, 2, "uff", "motion"};
  return c_wl_connection_send(conn, &msg, 3, time, surface_x, surface_y);
}
C_WL_EVENT wl_pointer_button(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint serial, c_wl_uint time, c_wl_uint button, enum wl_pointer_button_state_enum state) {
  struct c_wl_message msg = {wl_pointer, 3, "uuuu", "button"};
  return c_wl_connection_send(conn, &msg, 4, serial, time, button, state);
}
C_WL_EVENT wl_pointer_axis(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint time, enum wl_pointer_axis_enum axis, c_wl_fixed value) {
  struct c_wl_message msg = {wl_pointer, 4, "uuf", "axis"};
  return c_wl_connection_send(conn, &msg, 3, time, axis, value);
}
C_WL_EVENT wl_pointer_frame(struct c_wl_connection *conn, c_wl_object_id wl_pointer) {
  struct c_wl_message msg = {wl_pointer, 5, {0}, "frame"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_pointer_axis_source(struct c_wl_connection *conn, c_wl_object_id wl_pointer, enum wl_pointer_axis_source_enum axis_source) {
  struct c_wl_message msg = {wl_pointer, 6, "u", "axis_source"};
  return c_wl_connection_send(conn, &msg, 1, axis_source);
}
C_WL_EVENT wl_pointer_axis_stop(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint time, enum wl_pointer_axis_enum axis) {
  struct c_wl_message msg = {wl_pointer, 7, "uu", "axis_stop"};
  return c_wl_connection_send(conn, &msg, 2, time, axis);
}
C_WL_EVENT wl_pointer_axis_discrete(struct c_wl_connection *conn, c_wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, c_wl_int discrete) {
  struct c_wl_message msg = {wl_pointer, 8, "ui", "axis_discrete"};
  return c_wl_connection_send(conn, &msg, 2, axis, discrete);
}
C_WL_EVENT wl_pointer_axis_value120(struct c_wl_connection *conn, c_wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, c_wl_int value120) {
  struct c_wl_message msg = {wl_pointer, 9, "ui", "axis_value120"};
  return c_wl_connection_send(conn, &msg, 2, axis, value120);
}
C_WL_EVENT wl_pointer_axis_relative_direction(struct c_wl_connection *conn, c_wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, enum wl_pointer_axis_relative_direction_enum direction) {
  struct c_wl_message msg = {wl_pointer, 10, "uu", "axis_relative_direction"};
  return c_wl_connection_send(conn, &msg, 2, axis, direction);
}
C_WL_REQUEST wl_pointer_set_cursor(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_pointer_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_pointer_interface, "wl_pointer", 10, 2, 
    {"set_cursor",             wl_pointer_set_cursor,        NULL, 4,  "uoii"},
    {"release",                wl_pointer_release,           NULL, 0,  {0}},
)

C_WL_EVENT wl_keyboard_keymap(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, enum wl_keyboard_keymap_format_enum format, c_wl_fd fd, c_wl_uint size) {
  struct c_wl_message msg = {wl_keyboard, 0, "uFu", "keymap"};
  return c_wl_connection_send(conn, &msg, 3, format, fd, size);
}
C_WL_EVENT wl_keyboard_enter(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_uint serial, c_wl_object_id wl_surface, c_wl_array *keys) {
  struct c_wl_message msg = {wl_keyboard, 1, "uoa", "enter"};
  return c_wl_connection_send(conn, &msg, 3, serial, wl_surface, keys);
}
C_WL_EVENT wl_keyboard_leave(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_uint serial, c_wl_object_id wl_surface) {
  struct c_wl_message msg = {wl_keyboard, 2, "uo", "leave"};
  return c_wl_connection_send(conn, &msg, 2, serial, wl_surface);
}
C_WL_EVENT wl_keyboard_key(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_uint serial, c_wl_uint time, c_wl_uint key, enum wl_keyboard_key_state_enum state) {
  struct c_wl_message msg = {wl_keyboard, 3, "uuuu", "key"};
  return c_wl_connection_send(conn, &msg, 4, serial, time, key, state);
}
C_WL_EVENT wl_keyboard_modifiers(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_uint serial, c_wl_uint mods_depressed, c_wl_uint mods_latched, c_wl_uint mods_locked, c_wl_uint group) {
  struct c_wl_message msg = {wl_keyboard, 4, "uuuuu", "modifiers"};
  return c_wl_connection_send(conn, &msg, 5, serial, mods_depressed, mods_latched, mods_locked, group);
}
C_WL_EVENT wl_keyboard_repeat_info(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_int rate, c_wl_int delay) {
  struct c_wl_message msg = {wl_keyboard, 5, "ii", "repeat_info"};
  return c_wl_connection_send(conn, &msg, 2, rate, delay);
}
C_WL_REQUEST wl_keyboard_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_keyboard_interface, "wl_keyboard", 10, 1, 
    {"release",                wl_keyboard_release,          NULL, 0,  {0}},
)

C_WL_EVENT wl_touch_down(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_uint serial, c_wl_uint time, c_wl_object_id wl_surface, c_wl_int id, c_wl_fixed x, c_wl_fixed y) {
  struct c_wl_message msg = {wl_touch, 0, "uuoiff", "down"};
  return c_wl_connection_send(conn, &msg, 6, serial, time, wl_surface, id, x, y);
}
C_WL_EVENT wl_touch_up(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_uint serial, c_wl_uint time, c_wl_int id) {
  struct c_wl_message msg = {wl_touch, 1, "uui", "up"};
  return c_wl_connection_send(conn, &msg, 3, serial, time, id);
}
C_WL_EVENT wl_touch_motion(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_uint time, c_wl_int id, c_wl_fixed x, c_wl_fixed y) {
  struct c_wl_message msg = {wl_touch, 2, "uiff", "motion"};
  return c_wl_connection_send(conn, &msg, 4, time, id, x, y);
}
C_WL_EVENT wl_touch_frame(struct c_wl_connection *conn, c_wl_object_id wl_touch) {
  struct c_wl_message msg = {wl_touch, 3, {0}, "frame"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_touch_cancel(struct c_wl_connection *conn, c_wl_object_id wl_touch) {
  struct c_wl_message msg = {wl_touch, 4, {0}, "cancel"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_touch_shape(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_int id, c_wl_fixed major, c_wl_fixed minor) {
  struct c_wl_message msg = {wl_touch, 5, "iff", "shape"};
  return c_wl_connection_send(conn, &msg, 3, id, major, minor);
}
C_WL_EVENT wl_touch_orientation(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_int id, c_wl_fixed orientation) {
  struct c_wl_message msg = {wl_touch, 6, "if", "orientation"};
  return c_wl_connection_send(conn, &msg, 2, id, orientation);
}
C_WL_REQUEST wl_touch_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_touch_interface, "wl_touch", 10, 1, 
    {"release",                wl_touch_release,             NULL, 0,  {0}},
)

C_WL_EVENT wl_output_geometry(struct c_wl_connection *conn, c_wl_object_id wl_output, c_wl_int x, c_wl_int y, c_wl_int physical_width, c_wl_int physical_height, enum wl_output_subpixel_enum subpixel, c_wl_string make, c_wl_string model, enum wl_output_transform_enum transform) {
  struct c_wl_message msg = {wl_output, 0, "iiiiissi", "geometry"};
  return c_wl_connection_send(conn, &msg, 8, x, y, physical_width, physical_height, subpixel, make, model, transform);
}
C_WL_EVENT wl_output_mode(struct c_wl_connection *conn, c_wl_object_id wl_output, enum wl_output_mode_enum flags, c_wl_int width, c_wl_int height, c_wl_int refresh) {
  struct c_wl_message msg = {wl_output, 1, "uiii", "mode"};
  return c_wl_connection_send(conn, &msg, 4, flags, width, height, refresh);
}
C_WL_EVENT wl_output_done(struct c_wl_connection *conn, c_wl_object_id wl_output) {
  struct c_wl_message msg = {wl_output, 2, {0}, "done"};
  return c_wl_connection_send(conn, &msg, 0);
}
C_WL_EVENT wl_output_scale(struct c_wl_connection *conn, c_wl_object_id wl_output, c_wl_int factor) {
  struct c_wl_message msg = {wl_output, 3, "i", "scale"};
  return c_wl_connection_send(conn, &msg, 1, factor);
}
C_WL_EVENT wl_output_name(struct c_wl_connection *conn, c_wl_object_id wl_output, c_wl_string name) {
  struct c_wl_message msg = {wl_output, 4, "s", "name"};
  return c_wl_connection_send(conn, &msg, 1, name);
}
C_WL_EVENT wl_output_description(struct c_wl_connection *conn, c_wl_object_id wl_output, c_wl_string description) {
  struct c_wl_message msg = {wl_output, 5, "s", "description"};
  return c_wl_connection_send(conn, &msg, 1, description);
}
C_WL_REQUEST wl_output_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_output_interface, "wl_output", 4, 1, 
    {"release",                wl_output_release,            NULL, 0,  {0}},
)

C_WL_REQUEST wl_region_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_region_add(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_region_subtract(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_region_interface, "wl_region", 1, 3, 
    {"destroy",                wl_region_destroy,            NULL, 0,  {0}},
    {"add",                    wl_region_add,                NULL, 4,  "iiii"},
    {"subtract",               wl_region_subtract,           NULL, 4,  "iiii"},
)

C_WL_REQUEST wl_subcompositor_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_subcompositor_get_subsurface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_subcompositor_interface, "wl_subcompositor", 1, 2, 
    {"destroy",                wl_subcompositor_destroy,     NULL, 0,  {0}},
    {"get_subsurface",         wl_subcompositor_get_subsurface,NULL, 3,  "noo"},
)

C_WL_REQUEST wl_subsurface_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_subsurface_set_position(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_subsurface_place_above(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_subsurface_place_below(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_subsurface_set_sync(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_subsurface_set_desync(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_subsurface_interface, "wl_subsurface", 1, 6, 
    {"destroy",                wl_subsurface_destroy,        NULL, 0,  {0}},
    {"set_position",           wl_subsurface_set_position,   NULL, 2,  "ii"},
    {"place_above",            wl_subsurface_place_above,    NULL, 1,  "o"},
    {"place_below",            wl_subsurface_place_below,    NULL, 1,  "o"},
    {"set_sync",               wl_subsurface_set_sync,       NULL, 0,  {0}},
    {"set_desync",             wl_subsurface_set_desync,     NULL, 0,  {0}},
)

C_WL_REQUEST wl_fixes_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_REQUEST wl_fixes_destroy_registry(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

C_WL_INTERFACE_REGISTER(wl_fixes_interface, "wl_fixes", 1, 2, 
    {"destroy",                wl_fixes_destroy,             NULL, 0,  {0}},
    {"destroy_registry",       wl_fixes_destroy_registry,    NULL, 1,  "o"},
)

