#ifndef CUTS_UTIL_MAP_H
#define CUTS_UTIL_MAP_H

#include <stdio.h>

struct c_map_pair {
  size_t key;
  void *value;
  struct c_map_pair *next;
  struct c_map_pair *prev;
};

typedef struct c_map {
  struct c_map_pair **pairs;
  size_t size;
} c_map;


#define c_map_for_each(m, p)                                     \
  for (size_t __i = 0; __i < (m)->size; __i++)                      \
    for ((p) = (m)->pairs[__i]; (p) != NULL; (p) = (p)->next)

c_map *c_map_new(size_t size);
void   c_map_destroy(c_map *m);
void  *c_map_set(c_map *m, size_t key, void *value, size_t value_size);
void   c_map_remove(c_map *m, size_t key);
void  *c_map_get(c_map *m, size_t key);

#endif
