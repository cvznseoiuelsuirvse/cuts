#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <drm/drm_fourcc.h>

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

float read_f32(char *buffer, uint32_t *offset) {
  float val = *(float *)(buffer + *offset);
  *offset += sizeof(float);
  return val;
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


void write_f32(char *buffer, uint32_t *offset, float val) {
  *(float *)(buffer + *offset) = val;
  *offset += sizeof(float);
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
    uint32_t string_size = strlen(string);
    uint32_t padded_string_size = PADDED4(string_size);

    write_u32(buffer, offset, string_size + 1);
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

uint32_t drm_format_to_bpp(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_C8:
    case DRM_FORMAT_R8:
        return 1;

    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_ABGR4444:
        return 2;

    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
        return 3;

    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRX8888:
    case DRM_FORMAT_BGRA8888:
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_ABGR2101010:
        return 4;

    case DRM_FORMAT_XRGB16161616:
    case DRM_FORMAT_ARGB16161616:
    case DRM_FORMAT_XBGR16161616:
    case DRM_FORMAT_ABGR16161616:
        return 8;

    default:
        return 0;
    }
}
