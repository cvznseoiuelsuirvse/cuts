#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/log.h"

struct metadata {
  size_t ref_count;
  char loc[128];
  void (*defer)(void *);
};


void *_c_malloc(size_t size, char *filepath, int line) {
  struct metadata *m;
  size_t init_size = sizeof(*m) + size;

  double nmemb_f = (double)init_size / sizeof(*m); 
  size_t nmemb = nmemb_f;
  if (nmemb_f != nmemb) nmemb++;

  m = calloc(nmemb, sizeof(*m));
  if (!m) return NULL;

  m->ref_count = 1;

  snprintf(m->loc, sizeof(m->loc), "%s:%d", filepath, line);
  c_log(C_LOG_DEBUG, "new memory allocated (%zu bytes) at %p from %s", size, m + 1, m->loc);

  return m + 1; // == return (void *)m + sizeof(struct metadata)
}

void c_defer(void *data, void (*func)(void *)) {
  struct metadata *m = (struct metadata *)data - 1;
  c_log(C_LOG_DEBUG, "defer function for %p is set to %p", data, func);
  m->defer = func;
}

void c_ref(void *data) {
  struct metadata *m = (struct metadata *)data - 1;
  m->ref_count++;
  c_log(C_LOG_DEBUG, "ref=%d %p (allocated from %s)", m->ref_count, data, m->loc);
}


void c_unref(void *data) {
  struct metadata *m = (struct metadata *)data - 1;
  m->ref_count--;
  c_log(C_LOG_DEBUG, "unref=%d %p (allocated from %s)", m->ref_count, data, m->loc);
  if (m->ref_count == 0) {
    if (m->defer) {
      c_log(C_LOG_DEBUG, "deferring %p", m);
      m->defer(data);
    }
    free((struct metadata *)data - 1);
  }
}

int c_get_refcount(void *data) {
  return data ? ((struct metadata *)data - 1)->ref_count : 0;
}
