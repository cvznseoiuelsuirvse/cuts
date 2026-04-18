#ifndef CUTS_UTIL_LIST_H
#define CUTS_UTIL_LIST_H

#include <stdio.h>
#include <stdint.h>

typedef struct c_list {
  void *data;
  int copied;
  struct c_list *prev;
  struct c_list *next;

  size_t size;
} c_list;

#define c_list_for_each(pos, member)                             \
  for (c_list *__pos = pos; __pos->next && ((member = __pos->data), 1); __pos = __pos->next) 	\

c_list *c_list_new();
void    c_list_destroy(c_list *l);
void   *c_list_push(c_list *l, void *data, size_t data_size);
void 	c_list_remove_ptr(c_list **head, void *data);
void   *c_list_get(c_list *l, uint32_t i);
size_t  c_list_len(c_list *l);
int     c_list_idx(c_list *l, void *data);
void   *c_list_insert(c_list **head, uint32_t i, void *data, size_t data_size);

#endif
