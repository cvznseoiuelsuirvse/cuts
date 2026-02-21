#include "map.h"
            
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

c_map *c_map_new(size_t size) {
  c_map *m = malloc(sizeof(c_map));
  if (m == NULL) {
    perror("malloc");
    return NULL;
  }

  m->size = size;
  m->pairs = calloc(size, sizeof(void *));
  if (m->pairs == NULL) {
    perror("calloc");
    free(m);
    return NULL;
  }

  return m;
}

void c_map_destroy(c_map *m) {
  if (m) {
    for (size_t i = 0; i < m->size; i++) {
      struct c_map_pair *pair = m->pairs[i];
      while (pair) {
        struct c_map_pair *next = pair->next;
        free(pair->value);
        free(pair);
        pair = next;
      }
    }
  }

  free(m->pairs);
  free(m);
}

void *c_map_set(c_map *m, size_t key, void *value, size_t value_size) {
  size_t n = key % m->size;

  struct c_map_pair *new_pair = calloc(1, sizeof(struct c_map_pair));
  struct c_map_pair *current_pair = m->pairs[n];

  new_pair->prev = NULL;
  new_pair->next = current_pair;
  new_pair->key = key;

  if (value_size > 0) {
    new_pair->value = malloc(value_size);
    if (!new_pair) {
      perror("malloc");
      return NULL;
    }
    memcpy(new_pair->value, value, value_size);
  } else 
    new_pair->value = value;
  
  
  if (current_pair) {
    current_pair->prev = new_pair;
  }

  m->pairs[n] = new_pair;

  return new_pair->value;
};

void c_map_remove(c_map *m, size_t key) {
  size_t n = key % m->size;
  struct c_map_pair *pair = m->pairs[n];

  for (; pair && pair->key != key; pair = pair->next)
    ;

  if (pair) {
    if (!pair->prev && !pair->next) { // first and only
      m->pairs[n] = NULL;

    } else if (!pair->prev) { // first
      pair->next->prev = NULL;
      m->pairs[n] = pair->next;

    } else if (!pair->next) { // last
      pair->prev->next = NULL;

    } else { // middle
      pair->prev->next = pair->next;
      pair->next->prev = pair->prev;
    }

    free(pair->value);
    free(pair);
  }
}

void *c_map_get(c_map *m, size_t key) {
  size_t n = key % m->size;
  struct c_map_pair *pair = m->pairs[n];

  for (; pair && pair->key != key; pair = pair->next)
    ;

  if (pair)
    return pair->value;

  return NULL;
};
