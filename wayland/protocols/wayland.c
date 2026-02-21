#include "../server.h"
#include "wayland.h"

#include <stdint.h>


WL_EVENT wl_display_error(struct wl_connection *conn, wl_object_id wl_display, wl_object_id object_id, wl_uint code, wl_string message) {
  struct wl_message msg = {wl_display, 0, 0, "ous"};
  return wl_connection_send(conn, &msg, 3, object_id, code, message);
}
WL_EVENT wl_display_delete_id(struct wl_connection *conn, wl_object_id wl_display, wl_uint id) {
  struct wl_message msg = {wl_display, 1, 0, "u"};
  return wl_connection_send(conn, &msg, 1, id);
}
WL_REQUEST wl_display_sync(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_display_get_registry(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_display_interface, "wl_display", 1, 2, 
    {"sync",                   wl_display_sync,               1,  "n"},
    {"get_registry",           wl_display_get_registry,       1,  "n"},
)

WL_EVENT wl_registry_global(struct wl_connection *conn, wl_object_id wl_registry, wl_uint name, wl_string interface, wl_uint version) {
  struct wl_message msg = {wl_registry, 0, 0, "usu"};
  return wl_connection_send(conn, &msg, 3, name, interface, version);
}
WL_EVENT wl_registry_global_remove(struct wl_connection *conn, wl_object_id wl_registry, wl_uint name) {
  struct wl_message msg = {wl_registry, 1, 0, "u"};
  return wl_connection_send(conn, &msg, 1, name);
}
WL_REQUEST wl_registry_bind(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_registry_interface, "wl_registry", 1, 1, 
    {"bind",                   wl_registry_bind,              4,  "usun"},
)

WL_EVENT wl_callback_done(struct wl_connection *conn, wl_object_id wl_callback, wl_uint callback_data) {
  struct wl_message msg = {wl_callback, 0, 0, "u"};
  return wl_connection_send(conn, &msg, 1, callback_data);
}

WL_REQUEST wl_compositor_create_surface(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_compositor_create_region(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_compositor_interface, "wl_compositor", 6, 2, 
    {"create_surface",         wl_compositor_create_surface,  1,  "n"},
    {"create_region",          wl_compositor_create_region,   1,  "n"},
)

WL_REQUEST wl_shm_pool_create_buffer(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shm_pool_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shm_pool_resize(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_shm_pool_interface, "wl_shm_pool", 2, 3, 
    {"create_buffer",          wl_shm_pool_create_buffer,     6,  "niiiiu"},
    {"destroy",                wl_shm_pool_destroy,           0,  {0}},
    {"resize",                 wl_shm_pool_resize,            1,  "i"},
)

WL_EVENT wl_shm_format(struct wl_connection *conn, wl_object_id wl_shm, enum wl_shm_format_enum format) {
  struct wl_message msg = {wl_shm, 0, 0, "u"};
  return wl_connection_send(conn, &msg, 1, format);
}
WL_REQUEST wl_shm_create_pool(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shm_release(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_shm_interface, "wl_shm", 2, 2, 
    {"create_pool",            wl_shm_create_pool,            3,  "nFi"},
    {"release",                wl_shm_release,                0,  {0}},
)

WL_EVENT wl_buffer_release(struct wl_connection *conn, wl_object_id wl_buffer) {
  struct wl_message msg = {wl_buffer, 0, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_REQUEST wl_buffer_destroy(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_buffer_interface, "wl_buffer", 1, 1, 
    {"destroy",                wl_buffer_destroy,             0,  {0}},
)

WL_EVENT wl_data_offer_offer(struct wl_connection *conn, wl_object_id wl_data_offer, wl_string mime_type) {
  struct wl_message msg = {wl_data_offer, 0, 0, "s"};
  return wl_connection_send(conn, &msg, 1, mime_type);
}
WL_EVENT wl_data_offer_source_actions(struct wl_connection *conn, wl_object_id wl_data_offer, enum wl_data_device_manager_dnd_action_enum source_actions) {
  struct wl_message msg = {wl_data_offer, 1, 0, "u"};
  return wl_connection_send(conn, &msg, 1, source_actions);
}
WL_EVENT wl_data_offer_action(struct wl_connection *conn, wl_object_id wl_data_offer, enum wl_data_device_manager_dnd_action_enum dnd_action) {
  struct wl_message msg = {wl_data_offer, 2, 0, "u"};
  return wl_connection_send(conn, &msg, 1, dnd_action);
}
WL_REQUEST wl_data_offer_accept(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_offer_receive(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_offer_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_offer_finish(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_offer_set_actions(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_data_offer_interface, "wl_data_offer", 3, 5, 
    {"accept",                 wl_data_offer_accept,          2,  "us"},
    {"receive",                wl_data_offer_receive,         2,  "sF"},
    {"destroy",                wl_data_offer_destroy,         0,  {0}},
    {"finish",                 wl_data_offer_finish,          0,  {0}},
    {"set_actions",            wl_data_offer_set_actions,     2,  "uu"},
)

WL_EVENT wl_data_source_target(struct wl_connection *conn, wl_object_id wl_data_source, wl_string mime_type) {
  struct wl_message msg = {wl_data_source, 0, 0, "s"};
  return wl_connection_send(conn, &msg, 1, mime_type);
}
WL_EVENT wl_data_source_send(struct wl_connection *conn, wl_object_id wl_data_source, wl_string mime_type, wl_fd fd) {
  struct wl_message msg = {wl_data_source, 1, fd, "sF"};
  return wl_connection_send(conn, &msg, 2, mime_type, fd);
}
WL_EVENT wl_data_source_cancelled(struct wl_connection *conn, wl_object_id wl_data_source) {
  struct wl_message msg = {wl_data_source, 2, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_data_source_dnd_drop_performed(struct wl_connection *conn, wl_object_id wl_data_source) {
  struct wl_message msg = {wl_data_source, 3, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_data_source_dnd_finished(struct wl_connection *conn, wl_object_id wl_data_source) {
  struct wl_message msg = {wl_data_source, 4, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_data_source_action(struct wl_connection *conn, wl_object_id wl_data_source, enum wl_data_device_manager_dnd_action_enum dnd_action) {
  struct wl_message msg = {wl_data_source, 5, 0, "u"};
  return wl_connection_send(conn, &msg, 1, dnd_action);
}
WL_REQUEST wl_data_source_offer(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_source_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_source_set_actions(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_data_source_interface, "wl_data_source", 3, 3, 
    {"offer",                  wl_data_source_offer,          1,  "s"},
    {"destroy",                wl_data_source_destroy,        0,  {0}},
    {"set_actions",            wl_data_source_set_actions,    1,  "u"},
)

WL_EVENT wl_data_device_data_offer(struct wl_connection *conn, wl_object_id wl_data_device, wl_new_id wl_data_offer) {
  struct wl_message msg = {wl_data_device, 0, 0, "n"};
  return wl_connection_send(conn, &msg, 1, wl_data_offer);
}
WL_EVENT wl_data_device_enter(struct wl_connection *conn, wl_object_id wl_data_device, wl_uint serial, wl_object_id wl_surface, wl_fixed x, wl_fixed y, wl_object_id wl_data_offer) {
  struct wl_message msg = {wl_data_device, 1, 0, "uoffo"};
  return wl_connection_send(conn, &msg, 5, serial, wl_surface, x, y, wl_data_offer);
}
WL_EVENT wl_data_device_leave(struct wl_connection *conn, wl_object_id wl_data_device) {
  struct wl_message msg = {wl_data_device, 2, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_data_device_motion(struct wl_connection *conn, wl_object_id wl_data_device, wl_uint time, wl_fixed x, wl_fixed y) {
  struct wl_message msg = {wl_data_device, 3, 0, "uff"};
  return wl_connection_send(conn, &msg, 3, time, x, y);
}
WL_EVENT wl_data_device_drop(struct wl_connection *conn, wl_object_id wl_data_device) {
  struct wl_message msg = {wl_data_device, 4, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_data_device_selection(struct wl_connection *conn, wl_object_id wl_data_device, wl_object_id wl_data_offer) {
  struct wl_message msg = {wl_data_device, 5, 0, "o"};
  return wl_connection_send(conn, &msg, 1, wl_data_offer);
}
WL_REQUEST wl_data_device_start_drag(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_device_set_selection(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_device_release(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_data_device_interface, "wl_data_device", 3, 3, 
    {"start_drag",             wl_data_device_start_drag,     4,  "ooou"},
    {"set_selection",          wl_data_device_set_selection,  2,  "ou"},
    {"release",                wl_data_device_release,        0,  {0}},
)

WL_REQUEST wl_data_device_manager_create_data_source(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_data_device_manager_get_data_device(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_data_device_manager_interface, "wl_data_device_manager", 3, 2, 
    {"create_data_source",     wl_data_device_manager_create_data_source, 1,  "n"},
    {"get_data_device",        wl_data_device_manager_get_data_device, 2,  "no"},
)

WL_REQUEST wl_shell_get_shell_surface(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_shell_interface, "wl_shell", 1, 1, 
    {"get_shell_surface",      wl_shell_get_shell_surface,    2,  "no"},
)

WL_EVENT wl_shell_surface_ping(struct wl_connection *conn, wl_object_id wl_shell_surface, wl_uint serial) {
  struct wl_message msg = {wl_shell_surface, 0, 0, "u"};
  return wl_connection_send(conn, &msg, 1, serial);
}
WL_EVENT wl_shell_surface_configure(struct wl_connection *conn, wl_object_id wl_shell_surface, enum wl_shell_surface_resize_enum edges, wl_int width, wl_int height) {
  struct wl_message msg = {wl_shell_surface, 1, 0, "uii"};
  return wl_connection_send(conn, &msg, 3, edges, width, height);
}
WL_EVENT wl_shell_surface_popup_done(struct wl_connection *conn, wl_object_id wl_shell_surface) {
  struct wl_message msg = {wl_shell_surface, 2, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_REQUEST wl_shell_surface_pong(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_move(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_resize(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_set_toplevel(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_set_transient(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_set_fullscreen(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_set_popup(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_set_maximized(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_set_title(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_shell_surface_set_class(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_shell_surface_interface, "wl_shell_surface", 1, 10, 
    {"pong",                   wl_shell_surface_pong,         1,  "u"},
    {"move",                   wl_shell_surface_move,         2,  "ou"},
    {"resize",                 wl_shell_surface_resize,       3,  "ouu"},
    {"set_toplevel",           wl_shell_surface_set_toplevel, 0,  {0}},
    {"set_transient",          wl_shell_surface_set_transient, 4,  "oiiu"},
    {"set_fullscreen",         wl_shell_surface_set_fullscreen, 3,  "uuo"},
    {"set_popup",              wl_shell_surface_set_popup,    6,  "ouoiiu"},
    {"set_maximized",          wl_shell_surface_set_maximized, 1,  "o"},
    {"set_title",              wl_shell_surface_set_title,    1,  "s"},
    {"set_class",              wl_shell_surface_set_class,    1,  "s"},
)

WL_EVENT wl_surface_enter(struct wl_connection *conn, wl_object_id wl_surface, wl_object_id wl_output) {
  struct wl_message msg = {wl_surface, 0, 0, "o"};
  return wl_connection_send(conn, &msg, 1, wl_output);
}
WL_EVENT wl_surface_leave(struct wl_connection *conn, wl_object_id wl_surface, wl_object_id wl_output) {
  struct wl_message msg = {wl_surface, 1, 0, "o"};
  return wl_connection_send(conn, &msg, 1, wl_output);
}
WL_EVENT wl_surface_preferred_buffer_scale(struct wl_connection *conn, wl_object_id wl_surface, wl_int factor) {
  struct wl_message msg = {wl_surface, 2, 0, "i"};
  return wl_connection_send(conn, &msg, 1, factor);
}
WL_EVENT wl_surface_preferred_buffer_transform(struct wl_connection *conn, wl_object_id wl_surface, enum wl_output_transform_enum transform) {
  struct wl_message msg = {wl_surface, 3, 0, "u"};
  return wl_connection_send(conn, &msg, 1, transform);
}
WL_REQUEST wl_surface_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_attach(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_damage(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_frame(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_set_opaque_region(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_set_input_region(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_commit(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_set_buffer_transform(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_set_buffer_scale(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_damage_buffer(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_surface_offset(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_surface_interface, "wl_surface", 6, 11, 
    {"destroy",                wl_surface_destroy,            0,  {0}},
    {"attach",                 wl_surface_attach,             3,  "oii"},
    {"damage",                 wl_surface_damage,             4,  "iiii"},
    {"frame",                  wl_surface_frame,              1,  "n"},
    {"set_opaque_region",      wl_surface_set_opaque_region,  1,  "o"},
    {"set_input_region",       wl_surface_set_input_region,   1,  "o"},
    {"commit",                 wl_surface_commit,             0,  {0}},
    {"set_buffer_transform",   wl_surface_set_buffer_transform, 1,  "i"},
    {"set_buffer_scale",       wl_surface_set_buffer_scale,   1,  "i"},
    {"damage_buffer",          wl_surface_damage_buffer,      4,  "iiii"},
    {"offset",                 wl_surface_offset,             2,  "ii"},
)

WL_EVENT wl_seat_capabilities(struct wl_connection *conn, wl_object_id wl_seat, enum wl_seat_capability_enum capabilities) {
  struct wl_message msg = {wl_seat, 0, 0, "u"};
  return wl_connection_send(conn, &msg, 1, capabilities);
}
WL_EVENT wl_seat_name(struct wl_connection *conn, wl_object_id wl_seat, wl_string name) {
  struct wl_message msg = {wl_seat, 1, 0, "s"};
  return wl_connection_send(conn, &msg, 1, name);
}
WL_REQUEST wl_seat_get_pointer(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_seat_get_keyboard(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_seat_get_touch(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_seat_release(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_seat_interface, "wl_seat", 10, 4, 
    {"get_pointer",            wl_seat_get_pointer,           1,  "n"},
    {"get_keyboard",           wl_seat_get_keyboard,          1,  "n"},
    {"get_touch",              wl_seat_get_touch,             1,  "n"},
    {"release",                wl_seat_release,               0,  {0}},
)

WL_EVENT wl_pointer_enter(struct wl_connection *conn, wl_object_id wl_pointer, wl_uint serial, wl_object_id wl_surface, wl_fixed surface_x, wl_fixed surface_y) {
  struct wl_message msg = {wl_pointer, 0, 0, "uoff"};
  return wl_connection_send(conn, &msg, 4, serial, wl_surface, surface_x, surface_y);
}
WL_EVENT wl_pointer_leave(struct wl_connection *conn, wl_object_id wl_pointer, wl_uint serial, wl_object_id wl_surface) {
  struct wl_message msg = {wl_pointer, 1, 0, "uo"};
  return wl_connection_send(conn, &msg, 2, serial, wl_surface);
}
WL_EVENT wl_pointer_motion(struct wl_connection *conn, wl_object_id wl_pointer, wl_uint time, wl_fixed surface_x, wl_fixed surface_y) {
  struct wl_message msg = {wl_pointer, 2, 0, "uff"};
  return wl_connection_send(conn, &msg, 3, time, surface_x, surface_y);
}
WL_EVENT wl_pointer_button(struct wl_connection *conn, wl_object_id wl_pointer, wl_uint serial, wl_uint time, wl_uint button, enum wl_pointer_button_state_enum state) {
  struct wl_message msg = {wl_pointer, 3, 0, "uuuu"};
  return wl_connection_send(conn, &msg, 4, serial, time, button, state);
}
WL_EVENT wl_pointer_axis(struct wl_connection *conn, wl_object_id wl_pointer, wl_uint time, enum wl_pointer_axis_enum axis, wl_fixed value) {
  struct wl_message msg = {wl_pointer, 4, 0, "uuf"};
  return wl_connection_send(conn, &msg, 3, time, axis, value);
}
WL_EVENT wl_pointer_frame(struct wl_connection *conn, wl_object_id wl_pointer) {
  struct wl_message msg = {wl_pointer, 5, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_pointer_axis_source(struct wl_connection *conn, wl_object_id wl_pointer, enum wl_pointer_axis_source_enum axis_source) {
  struct wl_message msg = {wl_pointer, 6, 0, "u"};
  return wl_connection_send(conn, &msg, 1, axis_source);
}
WL_EVENT wl_pointer_axis_stop(struct wl_connection *conn, wl_object_id wl_pointer, wl_uint time, enum wl_pointer_axis_enum axis) {
  struct wl_message msg = {wl_pointer, 7, 0, "uu"};
  return wl_connection_send(conn, &msg, 2, time, axis);
}
WL_EVENT wl_pointer_axis_discrete(struct wl_connection *conn, wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, wl_int discrete) {
  struct wl_message msg = {wl_pointer, 8, 0, "ui"};
  return wl_connection_send(conn, &msg, 2, axis, discrete);
}
WL_EVENT wl_pointer_axis_value120(struct wl_connection *conn, wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, wl_int value120) {
  struct wl_message msg = {wl_pointer, 9, 0, "ui"};
  return wl_connection_send(conn, &msg, 2, axis, value120);
}
WL_EVENT wl_pointer_axis_relative_direction(struct wl_connection *conn, wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, enum wl_pointer_axis_relative_direction_enum direction) {
  struct wl_message msg = {wl_pointer, 10, 0, "uu"};
  return wl_connection_send(conn, &msg, 2, axis, direction);
}
WL_REQUEST wl_pointer_set_cursor(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_pointer_release(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_pointer_interface, "wl_pointer", 10, 2, 
    {"set_cursor",             wl_pointer_set_cursor,         4,  "uoii"},
    {"release",                wl_pointer_release,            0,  {0}},
)

WL_EVENT wl_keyboard_keymap(struct wl_connection *conn, wl_object_id wl_keyboard, enum wl_keyboard_keymap_format_enum format, wl_fd fd, wl_uint size) {
  struct wl_message msg = {wl_keyboard, 0, fd, "uFu"};
  return wl_connection_send(conn, &msg, 3, format, fd, size);
}
WL_EVENT wl_keyboard_enter(struct wl_connection *conn, wl_object_id wl_keyboard, wl_uint serial, wl_object_id wl_surface, wl_array keys, wl_uint keys_size) {
  struct wl_message msg = {wl_keyboard, 1, 0, "uoa"};
  return wl_connection_send(conn, &msg, 3, serial, wl_surface, keys);
}
WL_EVENT wl_keyboard_leave(struct wl_connection *conn, wl_object_id wl_keyboard, wl_uint serial, wl_object_id wl_surface) {
  struct wl_message msg = {wl_keyboard, 2, 0, "uo"};
  return wl_connection_send(conn, &msg, 2, serial, wl_surface);
}
WL_EVENT wl_keyboard_key(struct wl_connection *conn, wl_object_id wl_keyboard, wl_uint serial, wl_uint time, wl_uint key, enum wl_keyboard_key_state_enum state) {
  struct wl_message msg = {wl_keyboard, 3, 0, "uuuu"};
  return wl_connection_send(conn, &msg, 4, serial, time, key, state);
}
WL_EVENT wl_keyboard_modifiers(struct wl_connection *conn, wl_object_id wl_keyboard, wl_uint serial, wl_uint mods_depressed, wl_uint mods_latched, wl_uint mods_locked, wl_uint group) {
  struct wl_message msg = {wl_keyboard, 4, 0, "uuuuu"};
  return wl_connection_send(conn, &msg, 5, serial, mods_depressed, mods_latched, mods_locked, group);
}
WL_EVENT wl_keyboard_repeat_info(struct wl_connection *conn, wl_object_id wl_keyboard, wl_int rate, wl_int delay) {
  struct wl_message msg = {wl_keyboard, 5, 0, "ii"};
  return wl_connection_send(conn, &msg, 2, rate, delay);
}
WL_REQUEST wl_keyboard_release(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_keyboard_interface, "wl_keyboard", 10, 1, 
    {"release",                wl_keyboard_release,           0,  {0}},
)

WL_EVENT wl_touch_down(struct wl_connection *conn, wl_object_id wl_touch, wl_uint serial, wl_uint time, wl_object_id wl_surface, wl_int id, wl_fixed x, wl_fixed y) {
  struct wl_message msg = {wl_touch, 0, 0, "uuoiff"};
  return wl_connection_send(conn, &msg, 6, serial, time, wl_surface, id, x, y);
}
WL_EVENT wl_touch_up(struct wl_connection *conn, wl_object_id wl_touch, wl_uint serial, wl_uint time, wl_int id) {
  struct wl_message msg = {wl_touch, 1, 0, "uui"};
  return wl_connection_send(conn, &msg, 3, serial, time, id);
}
WL_EVENT wl_touch_motion(struct wl_connection *conn, wl_object_id wl_touch, wl_uint time, wl_int id, wl_fixed x, wl_fixed y) {
  struct wl_message msg = {wl_touch, 2, 0, "uiff"};
  return wl_connection_send(conn, &msg, 4, time, id, x, y);
}
WL_EVENT wl_touch_frame(struct wl_connection *conn, wl_object_id wl_touch) {
  struct wl_message msg = {wl_touch, 3, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_touch_cancel(struct wl_connection *conn, wl_object_id wl_touch) {
  struct wl_message msg = {wl_touch, 4, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_touch_shape(struct wl_connection *conn, wl_object_id wl_touch, wl_int id, wl_fixed major, wl_fixed minor) {
  struct wl_message msg = {wl_touch, 5, 0, "iff"};
  return wl_connection_send(conn, &msg, 3, id, major, minor);
}
WL_EVENT wl_touch_orientation(struct wl_connection *conn, wl_object_id wl_touch, wl_int id, wl_fixed orientation) {
  struct wl_message msg = {wl_touch, 6, 0, "if"};
  return wl_connection_send(conn, &msg, 2, id, orientation);
}
WL_REQUEST wl_touch_release(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_touch_interface, "wl_touch", 10, 1, 
    {"release",                wl_touch_release,              0,  {0}},
)

WL_EVENT wl_output_geometry(struct wl_connection *conn, wl_object_id wl_output, wl_int x, wl_int y, wl_int physical_width, wl_int physical_height, enum wl_output_subpixel_enum subpixel, wl_string make, wl_string model, enum wl_output_transform_enum transform) {
  struct wl_message msg = {wl_output, 0, 0, "iiiiissi"};
  return wl_connection_send(conn, &msg, 8, x, y, physical_width, physical_height, subpixel, make, model, transform);
}
WL_EVENT wl_output_mode(struct wl_connection *conn, wl_object_id wl_output, enum wl_output_mode_enum flags, wl_int width, wl_int height, wl_int refresh) {
  struct wl_message msg = {wl_output, 1, 0, "uiii"};
  return wl_connection_send(conn, &msg, 4, flags, width, height, refresh);
}
WL_EVENT wl_output_done(struct wl_connection *conn, wl_object_id wl_output) {
  struct wl_message msg = {wl_output, 2, 0, {0}};
  return wl_connection_send(conn, &msg, 0);
}
WL_EVENT wl_output_scale(struct wl_connection *conn, wl_object_id wl_output, wl_int factor) {
  struct wl_message msg = {wl_output, 3, 0, "i"};
  return wl_connection_send(conn, &msg, 1, factor);
}
WL_EVENT wl_output_name(struct wl_connection *conn, wl_object_id wl_output, wl_string name) {
  struct wl_message msg = {wl_output, 4, 0, "s"};
  return wl_connection_send(conn, &msg, 1, name);
}
WL_EVENT wl_output_description(struct wl_connection *conn, wl_object_id wl_output, wl_string description) {
  struct wl_message msg = {wl_output, 5, 0, "s"};
  return wl_connection_send(conn, &msg, 1, description);
}
WL_REQUEST wl_output_release(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_output_interface, "wl_output", 4, 1, 
    {"release",                wl_output_release,             0,  {0}},
)

WL_REQUEST wl_region_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_region_add(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_region_subtract(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_region_interface, "wl_region", 1, 3, 
    {"destroy",                wl_region_destroy,             0,  {0}},
    {"add",                    wl_region_add,                 4,  "iiii"},
    {"subtract",               wl_region_subtract,            4,  "iiii"},
)

WL_REQUEST wl_subcompositor_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_subcompositor_get_subsurface(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_subcompositor_interface, "wl_subcompositor", 1, 2, 
    {"destroy",                wl_subcompositor_destroy,      0,  {0}},
    {"get_subsurface",         wl_subcompositor_get_subsurface, 3,  "noo"},
)

WL_REQUEST wl_subsurface_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_subsurface_set_position(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_subsurface_place_above(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_subsurface_place_below(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_subsurface_set_sync(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_subsurface_set_desync(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_subsurface_interface, "wl_subsurface", 1, 6, 
    {"destroy",                wl_subsurface_destroy,         0,  {0}},
    {"set_position",           wl_subsurface_set_position,    2,  "ii"},
    {"place_above",            wl_subsurface_place_above,     1,  "o"},
    {"place_below",            wl_subsurface_place_below,     1,  "o"},
    {"set_sync",               wl_subsurface_set_sync,        0,  {0}},
    {"set_desync",             wl_subsurface_set_desync,      0,  {0}},
)

WL_REQUEST wl_fixes_destroy(struct wl_connection *conn, union wl_arg *args);

WL_REQUEST wl_fixes_destroy_registry(struct wl_connection *conn, union wl_arg *args);

WL_INTERFACE_REGISTER(wl_fixes_interface, "wl_fixes", 1, 2, 
    {"destroy",                wl_fixes_destroy,              0,  {0}},
    {"destroy_registry",       wl_fixes_destroy_registry,     1,  "o"},
)

