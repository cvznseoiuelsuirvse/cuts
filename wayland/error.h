#ifndef CUTS_SERVER_ERROR_H
#define CUTS_SERVER_ERROR_H

#include "wayland/server.h"

int wl_error_set(wl_object_id object_id, wl_int code, wl_string msg, ...);
void wl_error_send(struct wl_connection *conn);

#endif
