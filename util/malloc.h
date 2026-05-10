#ifndef C_UTIL_MALLOC_H
#define C_UTIL_MALLOC_H

#include <stdio.h>

void *c_malloc(size_t size);
void c_free(void *data);
void c_ref(void *data);
void c_unref(void *data);
int c_get_refcount(void *data);

#endif
