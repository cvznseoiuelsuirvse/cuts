#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "util/log.h"
#include "wayland/display.h"
#include "util/helpers.h"

static int      __log_mask = 0;
static int64_t  __start = 0;
static int      __color = 0;

#define LOG_STREAM    stderr
#define LOG_STREAM_FD STDERR_FILENO

#define _printf(...) fprintf(LOG_STREAM, __VA_ARGS__)
#define _vprintf(...) vfprintf(LOG_STREAM, __VA_ARGS__)

static const char *log_level_string(enum c_log_level level) {
	switch (level) {
		case C_LOG_ERROR: 	return "ERROR";
		case C_LOG_INFO: 	  return "INFO";
		case C_LOG_DEBUG: 	return "DEBUG";
		case C_LOG_WARNING: return "WARNING";
		case C_LOG_WAYLAND: return "WAYLAND";
    default:            return "LOG";
	}
}

void format_ms(int ms, char *buffer, size_t buffer_size) {
  long long total_seconds = ms / 1000;
  int msec = ms % 1000;
  int sec = total_seconds % 60;
  long long total_minutes = total_seconds / 60;
  int min = total_minutes % 60;
  long long hours = total_minutes % 60;
  long long days = hours / 24;
  snprintf(buffer, buffer_size, "%02lld:%02lld:%02d:%02d.%03d", days, hours, min, sec, msec);
}

void _c_log(enum c_log_level level, const char *file, int line, int insert_nl, const char *format, ...) {
  if (!(level & __log_mask)) return;

  char time_format[64];
  format_ms(c_since_start_ms(), time_format, sizeof(time_format));

  va_list args;
  va_start(args, format);

  if (__color && isatty(LOG_STREAM_FD)) {
    switch (level) {
      case C_LOG_ERROR:
        _printf("\033[31m[%s] [%s %s:%d] ", time_format, log_level_string(level), file, line);
        break;

      case C_LOG_WARNING:
        _printf("\033[33m[%s] [%s %s:%d] ", time_format, log_level_string(level), file, line);
        break;

      case C_LOG_INFO:
        _printf("\033[34m[%s] [%s %s:%d] ", time_format, log_level_string(level), file, line);
        break;

      case C_LOG_DEBUG:
        _printf("\033[37;2m[%s] [%s %s:%d] ", time_format, log_level_string(level), file, line);
        break;

      default:
        _printf("[%s] [%s %s:%d] ", time_format, log_level_string(level), file, line);
        break;
    }

    _vprintf(format, args);
    _printf("\033[0m");

  } else {
    _printf("[%s] [%s %s:%d] ", time_format, log_level_string(level), file, line);
    _vprintf(format, args);
  }

  if (insert_nl)
    _printf("\n");

  fflush(LOG_STREAM);
  va_end(args);
}

void c_log_wl_request(struct c_wl_connection *conn, struct c_wl_object *object, struct c_wl_request *request, union c_wl_arg *args) {
  if (!(C_LOG_WAYLAND & __log_mask)) return;
  const struct c_wl_interface *iface = object->iface;
  c_log2(C_LOG_WAYLAND, " REQ (%p) %s#%lu.%s(", conn, iface->name, object->id, request->name);

  c_wl_array *arr;
  for (size_t i = 1; i <= request->nargs; i++) {
    uint8_t c = request->signature[i-1];

    switch (c) {
      case 'u': 
        _printf("%u", args[i].u);
        break;

      case 'i': 
        _printf("%i", args[i].i);
        break;

      case 'f': 
        _printf("%f", c_wl_fixed_to_double(args[i].f));
        break;

      case 'o': 
        _printf("%u", args[i].o);
        break;

      case 'n': 
        _printf("%u", args[i].n);
        break;

      case 's': 
        if (*args[i].s)
          _printf("\"%s\"", args[i].s);
        else
          _printf("NULL");
        break;

      case 'e': 
        _printf("%d", args[i].e);
        break;

      case 'F':
        _printf("%d", args[i].F);
        break;

      case 'a':
        arr = args[i].a;
        _printf("[%u]", arr->size);
        print_buffer(arr->data, arr->size, LOG_STREAM);
      break;

    }
    if (i < request->nargs)
      _printf(", ");
  }

  _printf(")\n");

}


void c_log_wl_event(struct c_wl_connection *conn, struct c_wl_object *object, const char *event_name, 
					   union c_wl_arg *args, size_t nargs, const char *signature) {
  if (!(C_LOG_WAYLAND & __log_mask)) return;
  const struct c_wl_interface *iface = object->iface;
  c_log2(C_LOG_WAYLAND, "EVT (%p) %s#%lu.%s(", conn, iface->name, object->id, event_name);

  c_wl_array *arr;
  for (size_t i = 0; i < nargs; i++) {
    uint8_t c = signature[i];

    switch (c) {
      case 'u': 
        _printf("%u", args[i].u);
        break;

      case 'i': 
        _printf("%i", args[i].i);
        break;

      case 'f': 
        _printf("%f", c_wl_fixed_to_double(args[i].f));
        break;

      case 'o': 
        _printf("%u", args[i].o);
        break;

      case 'n': 
        _printf("%u", args[i].n);
        break;

      case 's': 
        _printf("\"%s\"", args[i].s);
        break;

      case 'e': 
        _printf("%d", args[i].e);
        break;

      case 'F':
        _printf("%d", args[i].F);
        break;

      case 'a':
        arr = args[i].a;
        _printf("[%u]", arr->size);
        print_buffer(arr->data, arr->size, LOG_STREAM);
        break;
    }
    if (i < nargs - 1)
      _printf(", ");
  }

  _printf(")\n");

}

void c_log_setup(struct c_log_config *cfg) {
  __log_mask |= cfg->level_mask;
  __color = cfg->color;
}

int c_since_start_ms() {
  if (__start == 0)
    __start = now_ms();

  int64_t diff = now_ms() - __start;
  return diff;
}
