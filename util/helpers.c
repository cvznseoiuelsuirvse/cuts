#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <drm/drm_fourcc.h>
#include <fcntl.h>

void print_buffer(char *buffer, size_t buffer_len) {
  for (size_t i = 0; i < buffer_len; i++) {
    uint8_t c = buffer[i];
    if (32 <= c && c <= 126) {
      printf("%c", c);
    } else {
      printf("%02x", c);
    }
    if (i < buffer_len - 1) printf(" ");
  }
}


int set_nonblocking(int fd) {
  int flags;

  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return flags;

  flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}

int starts_with(const char *string, const char *prefix) {
  size_t s_len = strlen(string);
  size_t p_len = strlen(prefix);

  if (s_len < p_len) return 0;
  return strncmp(string, prefix, p_len) == 0;
}
