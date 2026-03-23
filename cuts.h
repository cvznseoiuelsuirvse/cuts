#ifndef CUTS_H
#define CUTS_H

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

typedef union {
	char *const s;
	int32_t     i;
	uint32_t    u;
} bind_args;

typedef void(*bind_func)(bind_args *args);
struct bind {
  uint32_t 	   	modmask;
  xkb_keysym_t 	keysym;
  bind_func     func;
  bind_args     args;
};

struct layout {
	void (*func)();
	const char *name;
};

// bind functions
void quit(bind_args *args);
void spawn_client(bind_args *args);
void kill_client(bind_args *args);
void focus(bind_args *args);

// layout functions
void tile();

#endif
