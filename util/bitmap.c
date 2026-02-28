#include "bitmap.h"

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#define byte_index(n) ((n) / 8)
#define bit_index(n) ((n) % 8)


inline int c_bitmap_set(c_bitmap *bm, uint32_t n) {
  if (n > bm->size) return -1;
  bm->bytes[byte_index(n)] |= (1 << bit_index(n));
  return 0;
}

inline int c_bitmap_get(c_bitmap *bm, uint32_t n) {
  if (n > bm->size) return -1;
  return (bm->bytes[byte_index(n)] & (1 << bit_index(n))) != 0;
}

uint32_t c_bitmap_get_free(c_bitmap *bm) {
  for (size_t byte_i = 0; byte_i < bm->size / 8; byte_i++) {
    uint8_t byte = bm->bytes[byte_i];
    if ((byte & 0xff) != 0xff) {
      for (size_t bit_i = 0; bit_i < 8; bit_i++)
        if (!(byte & (1 << bit_i))) return byte_i * 8 + bit_i;
    }
  }

  return 0;
}

inline int c_bitmap_unset(c_bitmap *bm, uint32_t n) {
  if (n > bm->size) return -1;
  bm->bytes[byte_index(n)] &= ~(1 << bit_index(n));
  return 0;
}

c_bitmap *c_bitmap_new(size_t size) {
  c_bitmap *bm = malloc(sizeof(c_bitmap));
  if (bm == NULL) {
    perror("malloc");
    return NULL;
  }

  bm->bytes = calloc(1, size / 8);
  if (!bm->bytes) {
    perror("c_bitmap_new: calloc(): ");
    return NULL;
  }
  bm->size = size;

  return bm;
}

inline void c_bitmap_destroy(c_bitmap *bm) {
  if (!bm) return;
  free(bm->bytes);
  free(bm);
}
