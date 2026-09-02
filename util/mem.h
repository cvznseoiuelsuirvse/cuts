#ifndef C_UTIL_MALLOC_H
#define C_UTIL_MALLOC_H

#include <stdio.h>

#define c_malloc(size) _c_malloc(size, (const char *)__FILE__ + 3, __LINE__)

void *_c_malloc(size_t size, const char *file, int line);
void c_defer(void *data, void (*func)(void *));
void c_ref(void *data);
void c_unref(void *data);
int c_get_refcount(void *data);

#endif
