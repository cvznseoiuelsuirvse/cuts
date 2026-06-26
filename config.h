#include "cuts.h"
#include "backend/input.h"

static uint32_t gap = 15;
static uint32_t background = 0x777777ff;

struct border border = {
  .width = 5,
  .c_focus =   0xff0000ff,
  .c_default = 0xffffffff,
};

static struct xkb_rule_names xkb_rules = {
    .layout = "us,ru",
    .options = "grp:toggle,caps:escape",
};

#define LEADER C_KEYBOARD_MOD_ALT
#define SWITCH_TAG(n) {LEADER, XKB_KEY_##n, switch_tag, {.u = 1 << (n-1)}}

static struct bind binds[] = {
	{LEADER, XKB_KEY_q,                     quit, 			  {}},
	{LEADER, XKB_KEY_Return, 	              sh, 	        {.s = "alacritty"}},
	{LEADER, XKB_KEY_b, 	                  sh, 	        {.s = "firefox"}},
	{LEADER, XKB_KEY_x, 	  	              kill_window,	{}},
	{LEADER, XKB_KEY_j, 	  	              move_focus,   {.i = 1}},
	{LEADER, XKB_KEY_k, 	  	              move_focus,   {.i = -1}},
  {LEADER, XKB_KEY_XF86AudioRaiseVolume, 	sh, 				  {.s = "wpctl set-volume -l 1 @DEFAULT_SINK@ 2%+"}},
  {LEADER, XKB_KEY_XF86AudioLowerVolume, 	sh, 				  {.s = "wpctl set-volume -l 1 @DEFAULT_SINK@ 2%-"}},
  {LEADER, XKB_KEY_XF86AudioMute, 			  sh, 				  {.s = "wpctl set-mute @DEFAULT_SINK@ toggle"}},

  SWITCH_TAG(1),
  SWITCH_TAG(2),
  SWITCH_TAG(3),
  SWITCH_TAG(4),
  SWITCH_TAG(5),
};

static struct layout layouts[] = {
	{tile, "tile"},
};

static uint32_t keyboard_repeat_rate =  50;
static uint32_t keyboard_repeat_delay = 300;
