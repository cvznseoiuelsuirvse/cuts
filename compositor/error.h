#ifndef CUTS_SERVER_ERROR_H
#define CUTS_SERVER_ERROR_H

#include "server.h"

int c_error_set(wl_object_id object_id, wl_int code, wl_string msg, ...);
void c_error_send(struct wl_connection *conn);

#endif
