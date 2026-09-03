#ifndef CUTS_H
#define CUTS_H

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

#define BIND_KEY(func)        void func(bind_args *)
#define BIND_MOUSE(func)      void func(bind_args *)
#define BIND_MOUSE_DRAG(func) void func(int done, bind_args *)
#define LAYOUT_FUNC(func)     void func()

struct monitor_config {
  const char *name;
  double scale;
  uint32_t x, y;
  uint32_t width, height;
  uint32_t refresh_rate;
};


enum bar_position {
  BAR_TOP    ,
  BAR_BOTTOM ,
  BAR_RIGHT  ,
  BAR_LEFT   ,
};

struct bar_config {
  struct {
    int size;
    const char *name;
  } font;

  struct {
    uint32_t font_active[4];
    uint32_t font_inactive[4];
    uint32_t font_urgent[4];

    uint32_t background_active[4];
    uint32_t background_inactive[4];
    uint32_t background_urgent[4];
  } tag;

  struct {
    uint32_t font_color[4];
    uint32_t background_color[4];
  } layout;

  struct {
    uint32_t font_color[4];
    uint32_t background_color[4];
  } title;

  struct {
    uint32_t font_color[4];
    uint32_t background_color[4];
  } text;

  enum bar_position pos;

};

typedef union {
	const char  *s;
	int32_t      i;
	uint32_t     u;
  double       d;
  void        *p;
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
	const char *repr;
};

BIND_KEY(quit);
BIND_KEY(spawn);
BIND_KEY(window_kill);
BIND_KEY(move_focus);
BIND_KEY(switch_tag);
BIND_KEY(toggle_floating);
BIND_KEY(window_move_to_workspace);
BIND_KEY(change_mfact);
BIND_KEY(change_nmaster);
BIND_KEY(set_layout);
BIND_KEY(toggle_fullscreen);
BIND_KEY(change_border);
BIND_KEY(change_gap);
BIND_MOUSE_DRAG(window_move);
BIND_MOUSE_DRAG(window_resize);
LAYOUT_FUNC(tile);
LAYOUT_FUNC(zoom);

#endif
