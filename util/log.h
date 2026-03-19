#ifndef CUTS_UTIL_LOG_H
#define CUTS_UTIL_LOG_H

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "wayland/server.h"

enum c_log_level {
	C_LOG_ERROR   = 1,
	C_LOG_INFO 	  = 1 << 1,
	C_LOG_DEBUG   = 1 << 2,
	C_LOG_WARNING = 1 << 3,
};


#define c_log_errno(level, format, ...) _c_log((level), (const char *)__FILE__ + 3, __LINE__, 1, format ": %s" __VA_OPT__(,) __VA_ARGS__, strerror(errno))
#define c_log(level, format, ...) _c_log((level), (const char *)__FILE__ + 3, __LINE__, 1, format __VA_OPT__(,) __VA_ARGS__)
#define c_log2(level, format, ...) _c_log((level), (const char *)__FILE__ + 3, __LINE__, 0, format __VA_OPT__(,) __VA_ARGS__)

void c_log_set_level(enum c_log_level n);
void c_log_wl_request(struct c_wl_connection *conn, struct c_wl_object *object, struct c_wl_request *request, union c_wl_arg *args);
void c_log_wl_event(struct c_wl_connection *conn, struct c_wl_object *object, const char *event_name, union c_wl_arg *args, size_t nargs, const char *signature);

void _c_log(enum c_log_level level, const char *file, int line, int insert_nl, const char *format, ...);


#endif
