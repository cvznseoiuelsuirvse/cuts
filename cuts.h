#ifndef CUTS_H
#define CUTS_H

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

struct border {
  uint32_t width;
  uint32_t c_focus;
  uint32_t c_default;
};

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
void sh(bind_args *args);
void kill_window(bind_args *args);
void move_focus(bind_args *args);
void switch_tag(bind_args *args);

// layout functions
void tile();

#endif
