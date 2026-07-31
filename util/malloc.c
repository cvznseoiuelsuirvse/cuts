#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  return m + 1; // == return (void *)m + sizeof(struct c_metadata)
}

void c_free(void *data) {
  free((struct c_metadata *)data - 1);
}

void c_ref(void *data) {
  struct c_metadata *m = (struct c_metadata *)data - 1;
  m->ref_count++;
}


void c_unref(void *data) {
  struct c_metadata *m = (struct c_metadata *)data - 1;
  if (--m->ref_count == 0)
    c_free(data);
}

int c_get_refcount(void *data) {
  return data ? ((struct c_metadata *)data - 1)->ref_count : 0;
}
