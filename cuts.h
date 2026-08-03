#ifndef CUTS_H
#define CUTS_H

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

struct monitor {
  const char *name;
  uint32_t x, y;
  uint32_t width, height;
  uint32_t refresh_rate;
};

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

typedef void(*bind_handler)(bind_args *);
typedef void(*bind_drag_handler)(int, bind_args *);

struct key_bind {
  uint32_t 	   	modmask;
  xkb_keysym_t 	keysym;

  bind_handler  handler;
  bind_args     args;
};

struct mouse_bind {
  uint32_t 	   	modmask;
  xkb_keysym_t 	keysym;
  uint32_t      btn;
  int           drag;

  union {
    bind_handler      handler;
    bind_drag_handler drag_handler;
  };

  bind_args         args;
};

struct layout {
	void (*func)();
	const char *name;
};

// bind functions
void quit(bind_args *);
void spawn(bind_args *);
void window_kill(bind_args *);
void move_focus(bind_args *);
void switch_tag(bind_args *);
void toggle_floating(bind_args *);
void window_move_to_workspace(bind_args *);

// bind drag functions
void window_move(int, bind_args *);
void window_resize(int, bind_args *);

// layout functions
void tile();

#endif
