#ifndef CUTS_UTIL_BITMAP_H
#define CUTS_UTIL_BITMAP_H

#include <stdio.h>
#include <stdint.h>

typedef struct c_bitmap {
  uint8_t *bytes;
  size_t size;
} c_bitmap;

c_bitmap *c_bitmap_new(size_t size);
void 	  c_bitmap_destroy(c_bitmap *bm);
int 	  c_bitmap_set(c_bitmap *bm, uint32_t n);
int 	  c_bitmap_get(c_bitmap *bm, uint32_t n);
uint32_t  c_bitmap_get_free(c_bitmap *bm);
int 	  c_bitmap_unset(c_bitmap *bm, uint32_t n);

#endif
