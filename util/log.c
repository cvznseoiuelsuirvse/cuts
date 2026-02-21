#include <stdio.h>
#include <stdarg.h>

#include "util/log.h"

void _c_log_err(const char *file, const char *function, const char *format, ...) {
  va_list list;
  va_start(list, format);

  fprintf(stderr, "[ERROR %s] %s: ", file, function);
  vfprintf(stderr, format, list);

  va_end(list);
}

void _c_log_info(const char *file, const char *function, const char *format, ...) {
  va_list list;
  va_start(list, format);

  printf("[INFO %s] %s: ", file, function);
  vprintf(format, list);

  va_end(list);
}

void _c_log_warn(const char *file, const char *function, const char *format, ...) {
  va_list list;
  va_start(list, format);

  printf("[WARNING %s] %s: ", file, function);
  vprintf(format, list);

  va_end(list);
}

