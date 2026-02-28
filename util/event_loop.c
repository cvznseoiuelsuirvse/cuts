#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>

#include "util/event_loop.h"
#include "../util/list.h"

#define CUTS_MAX_EPOLL_EVENTS 512

struct c_event_loop * c_event_loop_init() {
  struct c_event_loop *loop = calloc(1, sizeof(struct c_event_loop));

  loop->epfd = epoll_create1(0);
  if (loop->epfd == -1) {
    perror("epoll_create1");
    free(loop);
    return NULL;
  }

  loop->resources = c_list_new();
  loop->connections = c_list_new();
  return loop;
}

int c_event_loop_add(struct c_event_loop *loop, int fd, c_event_callback callback, void *data) {
  struct c_event_resource resource = {fd, data, callback};
  struct c_event_resource *resource_cpy = c_list_push(loop->resources, &resource, sizeof(struct c_event_resource));

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
  ev.data.ptr = resource_cpy;
  if (epoll_ctl(loop->epfd, EPOLL_CTL_ADD, resource.fd, &ev) == -1) {
    fprintf(stderr, "failed to add event %d\n", resource.fd);
    return -1;
  }
  return 0;
}

int c_event_loop_del(struct c_event_loop *loop, struct c_event_resource *resource) {
  if (epoll_ctl(loop->epfd, EPOLL_CTL_DEL, resource->fd, NULL) == -1) {
    fprintf(stderr, "failed to del event %d\n", resource->fd);
    return -1;
  }
  close(resource->fd);
  c_list_remove_ptr(&loop->resources, resource);
  return 0;
}


void c_event_loop_free(struct c_event_loop *loop) {
  struct c_event_resource *resource;
  struct c_list *l = loop->resources;
  c_list_for_each(l, resource) close(resource->fd);
  
  c_list_destroy(loop->resources);
  c_list_destroy(loop->connections);
  free(loop);
}


int c_event_loop_run(struct c_event_loop *loop) {
  if (!loop) { 
    return -1; 
  }

  int ret = 0;
  int n;
  struct epoll_event *events = loop->events;

  while (1) {
    n = epoll_wait(loop->epfd, loop->events, CUTS_MAX_EPOLL_EVENTS, -1);
    if (n == -1) {
      perror("epoll_wait");
      return -1;
    }
  
    for (int i = 0; i < n; i++) {
      struct c_event_resource *resource = events[i].data.ptr;
      if (events[i].events & EPOLLIN) {
        ret = resource->callback(loop, resource->fd, resource->data);
        switch (ret) {
          case -C_EVENT_ERROR_FATAL:
            goto out;

          case -C_EVENT_ERROR_WL_PROTO:
          case -C_EVENT_ERROR_WL_CLIENT_GONE:
            c_event_loop_del(loop, resource);
            break;
        }
          
      } else if (events[i].events & (EPOLLHUP | EPOLLERR))
        c_event_loop_del(loop, resource);
    }

  }

out:
  return -ret;
}

