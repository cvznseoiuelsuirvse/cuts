#include <stdarg.h>
#include <stdio.h>

#include "wayland/server.h"
#include "wayland/types/wayland.h"

static char       __error_msg[C_WL_MAX_STRING_SIZE] = {0};
static c_wl_int     __error_code = 0;
static c_wl_object_id  __error_object_id = 0;


int c_wl_error_set(c_wl_object_id object_id, c_wl_int code, c_wl_string msg, ...) {
  __error_code = code;
  __error_object_id = object_id;
  
  va_list args;
  va_start(args, msg);
  vsnprintf(__error_msg, C_WL_MAX_STRING_SIZE, msg, args);
  va_end(args);
  return -1;
}

void c_wl_error_send(struct c_wl_connection *conn) {
  wl_display_error(conn, 1, __error_object_id, __error_code, __error_msg);
  *__error_msg = 0;
  __error_code = 0;
  __error_object_id = 0;
}
