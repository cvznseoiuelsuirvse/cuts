#ifndef CUTS_UTIL_SIGNAL_H
#define CUTS_UTIL_SIGNAL_H

typedef void (*c_signal_handler)(int signal, void *userdata);
void c_signal_handler_add(int signal, c_signal_handler handler, void *userdata);

#endif
