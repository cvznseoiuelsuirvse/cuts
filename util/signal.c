#include <signal.h>
#include <assert.h>
#include <stdio.h>

#include "util/signal.h"

struct __signal_handler {
  int signal;
  c_signal_handler handler;
  void *userdata;
};

static unsigned long n_signal_handlers = 0;
static struct __signal_handler signal_handlers[64]; // should be enough

static void signal_handler(int signal) {
  for(unsigned long i = 0; i < n_signal_handlers; i++) {
    struct __signal_handler h = signal_handlers[i];
    if (h.signal == signal)
      h.handler(signal, h.userdata);

  }
}

void c_signal_handler_add(int signal, c_signal_handler handler, void *userdata) {
  assert(n_signal_handlers < sizeof(signal_handlers) / sizeof(*signal_handlers));

  struct __signal_handler *h = &signal_handlers[n_signal_handlers++];
  h->signal = signal;
  h->handler = handler;
  h->userdata = userdata;

  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  assert(sigaction(signal, &sa, NULL) == 0);
     
}
