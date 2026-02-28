#ifndef CUTS_UTIL_LIST_H
#define CUTS_UTIL_LIST_H

#include <stdio.h>

typedef struct c_list {
  void *data;
  struct c_list *prev;
  struct c_list *next;
} c_list;

#define c_list_for_each(pos, member)                             \
  for (; pos->next && ((member = pos->data), 1); pos = pos->next) 	\

c_list *c_list_new();
void    c_list_destroy(c_list *l);
void   *c_list_push(c_list *l, void *data, size_t data_size);
void    c_list_remove(c_list **head, size_t n);
void 	c_list_remove_ptr(c_list **head, void *ptr);
void   *c_list_get(c_list *l, size_t n);
void   *c_list_get_last(c_list *l);
size_t  c_list_len(c_list *l);

#endif
