#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "wayland/types.h"
#include "wayland/proto/xdg-shell.h"

#define PADDED4(n) ((n + 3) & ~3)

void calc_popup_coords(struct c_xdg_surface *surface, int32_t *x, int32_t *y) {
  struct c_xdg_positioner positioner = surface->popup.positioner;

  int32_t anchor_x, anchor_y;
  int32_t dx, dy;

  switch (positioner.anchor) {
    case XDG_POSITIONER_ANCHOR_RIGHT:
    case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      anchor_x = positioner.anchor_rect.width;
      break;

    case XDG_POSITIONER_ANCHOR_TOP:
    case XDG_POSITIONER_ANCHOR_BOTTOM:
      anchor_x = positioner.anchor_rect.width / 2;
      break;

    default:
      anchor_x = 0;
      break;
  }

  switch (positioner.anchor) {
    case XDG_POSITIONER_ANCHOR_BOTTOM:
    case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      anchor_y = positioner.anchor_rect.height;
      break;

    case XDG_POSITIONER_ANCHOR_LEFT:
    case XDG_POSITIONER_ANCHOR_RIGHT:
      anchor_y = positioner.anchor_rect.height / 2;
      break;

    default:
      anchor_y = 0;
      break;
  }

  switch (positioner.gravity) {
    case XDG_POSITIONER_GRAVITY_LEFT:
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
    case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
      dx = -positioner.width;
      break;

    case XDG_POSITIONER_GRAVITY_TOP:
    case XDG_POSITIONER_GRAVITY_BOTTOM:
      dx = -positioner.width / 2;
      break;

    default:
      dx = 0;
      break;
  }

  switch (positioner.gravity) {
    case XDG_POSITIONER_GRAVITY_TOP:
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
    case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
      dy = -positioner.height;
      break;

    case XDG_POSITIONER_GRAVITY_LEFT:
    case XDG_POSITIONER_GRAVITY_RIGHT:
      dy = -positioner.height / 2;
      break;

    default:
      dy = 0;
      break;
  }

  *x = positioner.anchor_rect.x + positioner.x + anchor_x + dx;
  *y = positioner.anchor_rect.y + positioner.y + anchor_y + dy;

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
    if (array_size > 0) {
      memset(buffer + *offset, 0, padded_array_size);
      memcpy(buffer + *offset, array, array_size);
    }

    *offset += padded_array_size;
}
