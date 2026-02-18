#include "list.h"

#include <stdlib.h>
#include <string.h>

c_list *c_list_new() {
  c_list *l = malloc(sizeof(c_list));
  if (l == NULL) {
    perror("malloc");
    return NULL;
  }

  l->data = NULL;
  l->next = NULL;
  l->prev = NULL;

  return l;
}

void c_list_destroy(c_list *l) {
  while (l) {
    c_list *next = l->next;
    free(l->data);
    free(l);
    l = next;
  }
}

void *c_list_push(c_list *l, void *data, size_t data_size) {
  for (; l->next; l = l->next)
    ;

  l->next = c_list_new();
  l->next->prev = l;

  if (data_size > 0) {
    l->data = malloc(data_size);
    if (!l->data) {
      perror("malloc");
      return NULL;
    }
    memcpy(l->data, data, data_size);
  } else 
    l->data = data;

  return l->data;
}

void c_list_remove(c_list **head, size_t n) {
  size_t i;
  c_list *l = *head;

  for (i = 0; l; l = l->next, i++) {
    if (n == i) {
      if (l->prev) {
        l->prev->next = l->next;
      } else {
        *head = l->next;
      }

      if (l->next)
        l->next->prev = l->prev;

      free(l->data);
      free(l);
      break;
    }
  }
}

void c_list_remove_ptr(c_list **head, void *ptr) {
  size_t i;
  c_list *l = *head;

  for (i = 0; l; l = l->next, i++) {
    if (l->data == ptr) {
      if (l->prev) {
        l->prev->next = l->next;
      } else {
        *head = l->next;
      }

      if (l->next)
        l->next->prev = l->prev;

      free(l->data);
      free(l);
      break;
    }
  }
}

void *c_list_get(c_list *l, size_t n) {
  size_t i;
  for (i = 0; l->next; l = l->next, i++) {
    if (n == i) {
      return l->data;
    }
  }

  return NULL;
}

size_t c_list_len(c_list *l) {
  size_t i;
  if (!l)
    return 0;

  for (i = 0; l->next; l = l->next, i++)
    ;
  return i;
}
