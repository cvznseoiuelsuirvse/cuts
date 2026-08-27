#ifndef CUTS_H
#define CUTS_H

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

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
	char  const *s;
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

// bind functions
void quit(bind_args *);
void spawn(bind_args *);
void window_kill(bind_args *);
void move_focus(bind_args *);
void switch_tag(bind_args *);
void toggle_floating(bind_args *);
void window_move_to_workspace(bind_args *);
void change_mfact(bind_args *);
void change_nmaster(bind_args *);
void set_layout(bind_args *);
void toggle_fullscreen(bind_args *);
void change_border(bind_args *args);
void change_gap(bind_args *args);

// bind drag functions
void window_move(int, bind_args *);
void window_resize(int, bind_args *);

// layout functions
void tile();
void zoom();

#endif
