#ifndef CUTS_UTIL_HELPERS_H
#define CUTS_UTIL_HELPERS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CONCAT(a, b) a##b
#define STREQ(s1, s2) (strcmp((s1), (s2)) == 0)
#define LENGTH(s) (sizeof((s)) /  sizeof(*(s)))
#define MAX(v1, v2) ((v1) > (v2)) ? (v1) : (v2)
#define MIN(v1, v2) ((v1) > (v2)) ? (v2) : (v1)

#define SWITCH_STR(var) { const char *__switch_var = (var); if (0) {
#define CASE_STR(value)  } else if (STREQ(__switch_var, (value))) {
#define DEFAULT_STR } else {
#define SWITCH_STR_END } }

#define HEX_TO_VEC4(n) \
{ \
  ((n >> 24) & 0xff) / 255.0f, \
  ((n >> 16) & 0xff) / 255.0f, \
  ((n >>  8) & 0xff) / 255.0f, \
  ((n >>  0) & 0xff) / 255.0f, \
}

void print_buffer(char *buffer, size_t buffer_len);
uint32_t drm_format_to_bpp(uint32_t format);
int set_nonblocking(int fd);
int starts_with(const char *string, const char *prefix);

#endif
