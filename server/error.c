#include "server/server.h"
#include "server/wayland.h"
#include <stdarg.h>
#include <stdio.h>

static char       __error_msg[WL_MAX_STRING_SIZE] = {0};
static wl_int     __error_code = 0;
static wl_object  __error_object_id = 0;


int c_error_set(wl_object object_id, wl_int code, wl_string msg, ...) {
  __error_code = code;
  __error_object_id = object_id;
  
  va_list args;
  va_start(args, msg);
  vsnprintf(__error_msg, WL_MAX_STRING_SIZE, msg, args);
  va_end(args);
  return -1;
}

void c_error_send(struct wl_connection *conn) {
  wl_display_error(conn, 1, __error_object_id, __error_code, __error_msg);
  *__error_msg = 0;
  __error_code = 0;
  __error_object_id = 0;
}
