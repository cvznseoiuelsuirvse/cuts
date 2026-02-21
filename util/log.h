#include <stdarg.h>


#define c_log_err(file, function,  format, ...) _c_log_err(file, function, format, ##__VA_ARGS__)

#ifdef C_LOGS
#define c_log_info(file, function, format, ...) _c_log_info((file), (function), (format), ##__VA_ARGS__)
#define c_log_warn(file, function, format, ...) _c_log_warn((file), (function), (format), ##__VA_ARGS__)
#else
#define c_log_info(file, function, format, ...)
#define c_log_warn(file, function, format, ...)
#endif

void _c_log_err(const char *file, const char *function, const char *format, ...);
void _c_log_info(const char *file, const char *function, const char *format, ...);
void _c_log_warn(const char *file, const char *function, const char *format, ...);


