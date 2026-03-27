#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <drm/drm_fourcc.h>
#include <fcntl.h>

#define PADDED4(n) ((n + 3) & ~3)

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

int32_t read_i32(char *buffer, uint32_t *offset) {
  int32_t val = *(int32_t *)(buffer + *offset);
  *offset += sizeof(int32_t);
  return val;
}

uint32_t read_u32(char *buffer, uint32_t *offset) {
  uint32_t val = *(uint32_t *)(buffer + *offset);
  *offset += sizeof(uint32_t);
  return val;
}

uint16_t read_u16(char *buffer, uint32_t *offset) {
  uint16_t val = *(uint16_t *)(buffer + *offset);
  *offset += sizeof(uint16_t);
  return val;
}

void read_string(char *buffer, uint32_t *offset, char *out, size_t out_size) {
  uint32_t string_size = read_u32(buffer, offset);
  assert(string_size < out_size);
  memcpy(out, buffer + *offset, string_size);
  *offset += PADDED4(string_size);
}

void *read_array(char *buffer, uint32_t *offset, size_t size) {
  uint32_t array_size = read_u32(buffer, offset);
  assert(array_size < size);
  void *out = malloc(size);
  if (!out) return NULL;
  memcpy(out, buffer + *offset, array_size);
  *offset += PADDED4(array_size);
  return out;
}

void write_i32(char *buffer, uint32_t *offset, int32_t val) {
  *(int32_t *)(buffer + *offset) = val;
  *offset += sizeof(int32_t);
}

void write_u32(char *buffer, uint32_t *offset, uint32_t val) {
  *(uint32_t *)(buffer + *offset) = val;
  *offset += sizeof(uint32_t);
}

void write_u16(char *buffer, uint32_t *offset, uint16_t val) {
  *(uint16_t *)(buffer + *offset) = val;
  *offset += sizeof(uint16_t);
}

void write_string(char *buffer, uint32_t *offset, const char *string) {
    uint32_t string_size = strlen(string) + 1;
    uint32_t padded_string_size = PADDED4(string_size);

    write_u32(buffer, offset, string_size);
    memset(buffer + *offset, 0, padded_string_size);
    snprintf(buffer + *offset, padded_string_size, "%s", string);

    *offset += padded_string_size ? padded_string_size : sizeof(uint32_t);
}

void write_array(char *buffer, uint32_t *offset, const void *array, size_t array_size) {
    uint32_t padded_array_size = PADDED4(array_size);

    write_u32(buffer, offset, array_size);
    memset(buffer + *offset, 0, padded_array_size);
    memcpy(buffer + *offset, array, array_size);

    *offset += padded_array_size ? padded_array_size : sizeof(uint32_t);
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
