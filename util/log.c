#include <stdio.h>
#include <stdarg.h>

#include "util/log.h"
#include "wayland/server.h"
#include "util/helpers.h"

static int __log_mask = 0;

static const char *log_level_string(enum c_log_level level) {
	switch (level) {
		case C_LOG_ERROR: 	return "ERROR";
		case C_LOG_INFO: 	  return "INFO";
		case C_LOG_DEBUG: 	return "DEBUG";
		case C_LOG_WARNING: return "WARNING";
    default:            return "LOG";
	}
}

void _c_log(enum c_log_level level, const char *file, int line, int insert_nl, const char *format, ...) {
  if (!(level & __log_mask)) return;

  va_list args;
  va_start(args, format);

  switch (level) {
    case C_LOG_ERROR:
      printf("\033[31m[%s %s:%d] ", log_level_string(level), file, line);
      break;
    case C_LOG_WARNING:
      printf("\033[33m[%s %s:%d] ", log_level_string(level), file, line);
      break;
    default:
      printf("[%s %s:%d] ", log_level_string(level), file, line);
      break;
  }

  vprintf(format, args);
  if (level & (C_LOG_ERROR | C_LOG_WARNING))
    printf("\033[0m");
  if (insert_nl)
    printf("\n");

  va_end(args);
}

void c_log_wl_request(struct c_wl_connection *conn, struct c_wl_object *object, struct c_wl_request *request, union c_wl_arg *args) {
  if (!(C_LOG_DEBUG & __log_mask)) return;

  const struct c_wl_interface *iface = object->iface;
  c_log2(C_LOG_DEBUG, "[wayland] client (%p) %s#%lu.%s(", conn, iface->name, object->id, request->name);

  c_wl_array *arr;
  for (size_t i = 1; i <= request->nargs; i++) {
    uint8_t c = request->signature[i-1];

    switch (c) {
      case 'u': 
        printf("%u", args[i].u);
        break;

      case 'i': 
        printf("%i", args[i].i);
        break;

      case 'f': 
        printf("%f", args[i].f);
        break;

      case 'o': 
        printf("%u", args[i].o);
        break;

      case 'n': 
        printf("%u", args[i].n);
        break;

      case 's': 
        printf("\"%s\"", args[i].s);
        break;

      case 'e': 
        printf("%d", args[i].e);
        break;

      case 'F':
        printf("%d", args[i].F);
        break;

      case 'a':
      arr = args[i].a;
      printf("[%u]", arr->size);
      print_buffer(arr->data, arr->size);
      break;

    }
    if (i < request->nargs)
      printf(", ");
  }

  printf(")\n");

}


void c_log_wl_event(struct c_wl_connection *conn, struct c_wl_object *object, const char *event_name, 
					   union c_wl_arg *args, size_t nargs, const char *signature) {
  if (!(C_LOG_DEBUG & __log_mask)) return;
  const struct c_wl_interface *iface = object->iface;
  c_log2(C_LOG_DEBUG, "[wayland] server (%p) %s#%lu.%s(", conn, iface->name, object->id, event_name);

  c_wl_array *arr;
  for (size_t i = 0; i < nargs; i++) {
    uint8_t c = signature[i];

    switch (c) {
      case 'u': 
        printf("%u", args[i].u);
        break;

      case 'i': 
        printf("%i", args[i].i);
        break;

      case 'f': 
        printf("%f", args[i].f);
        break;

      case 'o': 
        printf("%u", args[i].o);
        break;

      case 'n': 
        printf("%u", args[i].n);
        break;

      case 's': 
        printf("\"%s\"", args[i].s);
        break;

      case 'e': 
        printf("%d", args[i].e);
        break;

      case 'F':
        printf("%d", args[i].F);
        break;

      case 'a':
        arr = args[i].a;
        printf("[%u]", arr->size);
        print_buffer(arr->data, arr->size);
        break;
    }
    if (i < nargs - 1)
      printf(", ");
  }

  printf(")\n");

}

inline void c_log_set_level(enum c_log_level n) {
  __log_mask |= n;
}

