#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "wayland/util.h"

#define PADDED4(n) ((n + 3) & ~3)

int32_t read_i32(uint8_t *buffer, size_t *offset) {
  int32_t val = *(int32_t *)(buffer + *offset);
  *offset += sizeof(int32_t);
  return val;
}

uint32_t read_u32(uint8_t *buffer, size_t *offset) {
  uint32_t val = *(uint32_t *)(buffer + *offset);
  *offset += sizeof(uint32_t);
  return val;
}

uint16_t read_u16(uint8_t *buffer, size_t *offset) {
  uint16_t val = *(uint16_t *)(buffer + *offset);
  *offset += sizeof(uint16_t);
  return val;
}

void read_string(uint8_t *buffer, size_t *offset, char *out, size_t out_size) {
  uint32_t string_size = read_u32(buffer, offset);
  assert(string_size < out_size);
  if (!string_size) {
    out = NULL;
  } else {
    memcpy(out, buffer + *offset, string_size);
    *offset += PADDED4(string_size);
  }
}

void *read_array(uint8_t *buffer, size_t *offset, size_t size) {
  uint32_t array_size = read_u32(buffer, offset);
  assert(array_size < size);
  void *out = malloc(size);
  if (!out) return NULL;
  memcpy(out, buffer + *offset, array_size);
  *offset += PADDED4(array_size);
  return out;
}

void write_i32(uint8_t *buffer, size_t *offset, int32_t val) {
  *(int32_t *)(buffer + *offset) = val;
  *offset += sizeof(int32_t);
}

void write_u32(uint8_t *buffer, size_t *offset, uint32_t val) {
  *(uint32_t *)(buffer + *offset) = val;
  *offset += sizeof(uint32_t);
}

void write_u16(uint8_t *buffer, size_t *offset, uint16_t val) {
  *(uint16_t *)(buffer + *offset) = val;
  *offset += sizeof(uint16_t);
}

void write_string(uint8_t *buffer, size_t *offset, const char *string) {
    uint32_t string_size = strlen(string) + 1;
    uint32_t padded_string_size = PADDED4(string_size);

    write_u32(buffer, offset, string_size);
    memset(buffer + *offset, 0, padded_string_size);
    snprintf((char *)buffer + *offset, padded_string_size, "%s", string);

    *offset += padded_string_size ? padded_string_size : sizeof(uint32_t);
}

void write_array(uint8_t *buffer, size_t *offset, const uint8_t *array, size_t array_size) {
    uint32_t padded_array_size = PADDED4(array_size);

    write_u32(buffer, offset, array_size);
    if (array_size > 0) {
      memset(buffer + *offset, 0, padded_array_size);
      memcpy(buffer + *offset, array, array_size);
    }

    *offset += padded_array_size;
}
