#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>

void print_buffer(char *buffer, size_t buffer_len, FILE *stream) {
  for (size_t i = 0; i < buffer_len; i++) {
    uint8_t c = buffer[i];
    if (32 <= c && c <= 126) {
      fprintf(stream, "%c", c);
    } else {
      fprintf(stream, "%02x", c);
    }
    if (i < buffer_len - 1) fprintf(stream, " ");
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

uint32_t hash_string(char *string) {
  unsigned long hash = 5381;
  int c;

  while ((c = *string++))
    hash = ((hash << 5) + hash) + c;

  return hash;
}

uint32_t swap_32(uint32_t value) {
  return ((value & 0xFF000000) >> 24) | ((value & 0x00FF0000) >> 8) |
         ((value & 0x0000FF00) << 8) | ((value & 0x000000FF) << 24);
}
uint16_t swap_16(uint32_t value) {
  return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

int read_at(void *ptr, size_t size, size_t n, size_t offset, FILE *f) {
  if (fseek(f, offset, SEEK_SET)) return 0;
  return fread(ptr, size, n, f);
}
