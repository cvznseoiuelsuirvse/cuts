#include <stdlib.h>
#include <string.h>

#include "list.h"

c_list *c_list_new() {
  c_list *l = calloc(1, sizeof(*l));
  if (l == NULL) {
    perror("malloc");
    return NULL;
  }
  l->size = 0;
  return l;
}

static c_list *c_list_new2(void *data, size_t data_size) {
  c_list *l = c_list_new();
  if (data_size > 0) {
    l->data = malloc(data_size);
    if (!l->data) {
      perror("malloc");
      return NULL;
    }
    memcpy(l->data, data, data_size);
    l->copied = 1;
  } else 
    l->data = data;

  return l->data;
}

void c_list_destroy(c_list *l) {
  while (l) {
    c_list *next = l->next;
    if (l->copied)
      free(l->data);
    free(l);
    l = next;
  }
}

void *c_list_push(c_list *l, void *data, size_t data_size) {
  l->size++;
  for (; l->next; l = l->next);

  l->next = c_list_new();
  l->next->prev = l;

  if (data_size > 0) {
    l->data = malloc(data_size);
    if (!l->data) {
      perror("malloc");
      return NULL;
    }
    memcpy(l->data, data, data_size);
    l->copied = 1;
  } else 
    l->data = data;

  return l->data;
}

void *c_list_insert(c_list **head, uint32_t i, void *data, size_t data_size) {
  c_list *l = *head;
  if (i > l->size) return NULL;

  l->size++;

  for (uint32_t ii = 0; l->next; l = l->next, ii++)
    if (ii == i) break;
    
  if (!l->prev && l->next) {  // first
    c_list *new = c_list_new2(data, data_size);
    new->next = l;
    l->prev = new;
    *head = new;
    return new->data;

  } else if (l->prev && l->next) {  // middle
    c_list *new = c_list_new2(data, data_size);
    new->prev = l->prev;
    new->next = l;
    l->prev->next = new;
    l->prev = new;
    return new->data;
  }

  // last
  return c_list_push(l, data, data_size);
}

void c_list_remove_ptr(c_list **head, void *data) {
  c_list *l = *head;
  l->size--;

  for (size_t i = 0; l; l = l->next, i++) {
    if (l->data == data) {
      if (!l->prev && l->next) {
        *head = l->next;
        l->next->size = l->size;
        l->next->prev = NULL;
      }
      else if (l->prev && l->next) {
        l->prev->next = l->next;
        l->next->prev = l->prev;
      }
      else { // l->prev && !l->next
        l->prev->next = NULL;
      }

      if (l->copied)
        free(l->data);

      free(l);

      break;
    }
  }

}

void *c_list_get(c_list *l, uint32_t i) {
  for (uint32_t ii = 0; l->next; l = l->next, ii++) {
    if (ii == i) {
      return l->data;
    }
  }

  return NULL;
}

int c_list_idx(c_list *l, void *data) {
  for (int i = 0; l->next; l = l->next, i++) {
    if (l->data == data) {
      return i;
    }
  }

  return -1;
}
