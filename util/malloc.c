#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/log.h"

struct c_metadata {
  int ref_count;
};


void *c_malloc(size_t size) {
  struct c_metadata *m;
  size_t init_size = sizeof(*m) + size;

  m = malloc(init_size);
  if (!m) return NULL;

  memset(m, 0, init_size);
  m->ref_count = 1;

  return m + 1;
}

void c_free(void *data) {
  free((struct c_metadata *)data - 1);
}

void c_ref(void *data) {
  struct c_metadata *m = (struct c_metadata *)data - 1;
  c_log(C_LOG_DEBUG, "[C_REF] data: %p refcount: %d", data, m->ref_count);
  m->ref_count++;
}


void c_unref(void *data) {
  struct c_metadata *m = (struct c_metadata *)data - 1;
  c_log(C_LOG_DEBUG, "[C_UNREF] data: %p refcount: %d", data, m->ref_count);
  if (--m->ref_count == 0)
    c_free(data);
}

int c_get_refcount(void *data) {
  return data ? ((struct c_metadata *)data - 1)->ref_count : 0;
}
