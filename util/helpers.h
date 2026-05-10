#ifndef CUTS_UTIL_HELPERS_H
#define CUTS_UTIL_HELPERS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STREQ(s1, s2) (strcmp((s1), (s2)) == 0)
#define LENGTH(s) (sizeof((s)) /  sizeof(*(s)))
#define MAX(v1, v2) ((v1) > (v2)) ? (v1) : (v2)
#define MIN(v1, v2) ((v1) > (v2)) ? (v2) : (v1)
#define SWITCH_STR(var) const char *__switch_var = (var);
#define SWITCH_STR_BREAK goto __switch_str_end
#define SWITCH_STR_END __switch_str_end:
#define CASE_STR(value)  if (STREQ(__switch_var, (value)))

void print_buffer(char *buffer, size_t buffer_len);
uint32_t drm_format_to_bpp(uint32_t format);
int set_nonblocking(int fd);
int starts_with(const char *string, const char *prefix);

#endif
