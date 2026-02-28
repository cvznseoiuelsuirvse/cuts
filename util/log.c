#include <stdio.h>

#include "wayland/server.h"
#include "util/helpers.h"

void _c_wl_print_request(int client_fd, struct c_wl_object *object, struct c_wl_request *request, union c_wl_arg *args) {
  const struct c_wl_interface *iface = object->iface;
  printf("CLIENT %d -> %s#%lu.%s(", client_fd, iface->name, object->id, request->name);

  c_wl_array *arr;
  for (size_t i = 1; i <= request->nargs; i++) {
    uint8_t c = request->signature[i-1];

    switch (c) {
      case 'u': 
        printf("%lu", args[i].u);
        break;

      case 'i': 
        printf("%li", args[i].i);
        break;

      case 'f': 
        printf("%f", args[i].f);
        break;

      case 'o': 
        printf("%lu", args[i].o);
        break;

      case 'n': 
        printf("%lu", args[i].n);
        break;

      case 's': 
        printf("\"%s\"", args[i].s);
        break;

      case 'e': 
        printf("%d", args[i].e);
        break;

      case 'F':
        printf("%d", args[i].F);
        break;

      case 'a':
      arr = args[i].a;
      printf("[%lu]", arr->size);
      print_buffer(arr->data, arr->size);
      break;

    }
    if (i < request->nargs)
      printf(", ");
  }

  printf(")\n");

}


void _c_wl_print_event(int client_fd, struct c_wl_object *object, const char *event_name, 
					   union c_wl_arg *args, size_t nargs, const char *signature) {

  const struct c_wl_interface *iface = object->iface;
  printf("CLIENT %d <- %s#%lu.%s(", client_fd, iface->name, object->id, event_name);

  c_wl_array *arr;
  for (size_t i = 0; i < nargs; i++) {
    uint8_t c = signature[i];

    switch (c) {
      case 'u': 
        printf("%lu", args[i].u);
        break;

      case 'i': 
        printf("%li", args[i].i);
        break;

      case 'f': 
        printf("%f", args[i].f);
        break;

      case 'o': 
        printf("%lu", args[i].o);
        break;

      case 'n': 
        printf("%lu", args[i].n);
        break;

      case 's': 
        printf("\"%s\"", args[i].s);
        break;

      case 'e': 
        printf("%d", args[i].e);
        break;

      case 'F':
        printf("%d", args[i].F);
        break;

      case 'a':
        arr = args[i].a;
        printf("[%lu]", arr->size);
        print_buffer(arr->data, arr->size);
        break;
    }
    if (i < nargs - 1)
      printf(", ");
  }

  printf(")\n");

}

