#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/log.h"

struct metadata {
  size_t ref_count;
  void (*defer)(void *);
};


void *c_malloc(size_t size) {
  struct metadata *m;
  size_t init_size = sizeof(*m) + size;

  double nmemb_f = (double)init_size / sizeof(*m); 
  size_t nmemb = nmemb_f;
  if (nmemb_f != nmemb) nmemb++;

  m = calloc(nmemb, sizeof(*m));
  if (!m) return NULL;

  m->ref_count = 1;

  return m + 1; // == return (void *)m + sizeof(struct metadata)
}

void c_defer(void *data, void (*func)(void *)) {
  struct metadata *m = (struct metadata *)data - 1;
  c_log(C_LOG_DEBUG, "defer function for %p is set to %p", m, func);
  m->defer = func;
}

void c_ref(void *data) {
  struct metadata *m = (struct metadata *)data - 1;
  c_log(C_LOG_DEBUG, "ref %p (%d)", data, m->ref_count);
  m->ref_count++;
}


void c_unref(void *data) {
  struct metadata *m = (struct metadata *)data - 1;
  c_log(C_LOG_DEBUG, "unref %p (%d)", data, m->ref_count);

  if (--m->ref_count == 0) {
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
