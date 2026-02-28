#ifndef CUTS_UTIL_LOG_H
#define CUTS_UTIL_LOG_H

#include "wayland/server.h"

#define C_LOG_ERROR   "ERROR"
#define C_LOG_INFO    "INFO"
#define C_LOG_WARNING "WARNING"

#ifdef CUTS_LOGS
#define c_log(level, format, ...) printf("[%s %s:%d] " format "\n", level, (const char *)__FILE__ + 3, __LINE__  __VA_OPT__(,) __VA_ARGS__)
#else
#define c_log(level, format, ...)
#endif /* CUTS_LOGS */

#ifdef CUTS_WL_LOGS
#define c_wl_print_request(fd, object, request, args) _c_wl_print_request((fd), (object), (request), (args))
#define c_wl_print_event(fd, object, event_name, args, nargs, signature) _c_wl_print_event((fd), (object), (event_name), (args), (nargs), (signature))
#else
#define c_wl_print_request(fd, object, request, args)
#define c_wl_print_event(fd, object, event_name, args, nargs, signature)
#endif /* CUTS_WL_LOGS */

void _c_wl_print_request(int fd, struct c_wl_object *object, struct c_wl_request *request, union c_wl_arg *args);
void _c_wl_print_event(int fd, struct c_wl_object *object, const char *event_name, union c_wl_arg *args, size_t nargs, const char *signature);


#endif
