#ifndef CUTS_WAYLAND_ERROR_H
#define CUTS_WAYLAND_ERROR_H

#include "wayland/server.h"

int c_wl_error_set(c_wl_object_id object_id, c_wl_int code, c_wl_string msg, ...);
void c_wl_error_send(struct c_wl_connection *conn);

#endif
