#ifndef CUTS_UTIL_CALLBACK_H
#define CUTS_UTIL_CALLBACK_H

#include "util/list.h"


#define c_callback_notify(cb, st, func, ...)                                     \
  if (((struct st *)cb->listeners)->func)                                        \
    ((struct st *)cb->listeners)->func(__VA_ARGS__ __VA_OPT__(, ) cb->userdata);

#define c_callback_notify_all(cb_list, st, func, ...)                          \
  {                                                                            \
    c_list *l = cb_list;                                                       \
    while (l && l->next) {                                                     \
      c_list *next = l->next;                                                  \
      struct c_callback *cb = l->data;                                         \
      if (((struct st *)cb->listeners)->func)                                  \
        ((struct st *)cb->listeners)                                           \
            ->func(__VA_ARGS__ __VA_OPT__(, ) cb->userdata);                   \
      l = next;                                                                \
    }                                                                          \
  }

struct c_callback {
  void *listeners;
  void *userdata;
};

struct c_callback *c_callback_add(c_list *cb_list, void *listeners, void *userdata);
void c_callback_remove(c_list **cb_list, struct c_callback *cb);

#endif
