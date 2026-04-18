#include "cuts.h"
#include "backend/input.h"

static uint32_t gap = 15;
static struct xkb_rule_names xkb_rules = {
    .layout = "us,ru",
    .options = "grp:toggle,caps:escape",
};

#define LEADER_KEY C_KEYBOARD_MOD_ALT

static struct bind binds[] = {
	{LEADER_KEY, XKB_KEY_q,                     quit, 			    {}},
	{LEADER_KEY, XKB_KEY_Return, 	              sh_cmd, 	      {.s = "alacritty"}},
	{LEADER_KEY, XKB_KEY_b, 	                  sh_cmd, 	      {.s = "firefox"}},
	{LEADER_KEY, XKB_KEY_x, 	  	              kill_window,  	{}},
	{LEADER_KEY, XKB_KEY_j, 	  	              focus,  		    {.i = 1}},
	{LEADER_KEY, XKB_KEY_k, 	  	              focus,  		    {.i = -1}},
  {LEADER_KEY, XKB_KEY_XF86AudioRaiseVolume, 	sh_cmd, 				{.s = "wpctl set-volume -l 1 @DEFAULT_SINK@ 2%+"}},
  {LEADER_KEY, XKB_KEY_XF86AudioLowerVolume, 	sh_cmd, 				{.s = "wpctl set-volume -l 1 @DEFAULT_SINK@ 2%-"}},
  {LEADER_KEY, XKB_KEY_XF86AudioMute, 			  sh_cmd, 				{.s = "wpctl set-mute @DEFAULT_SINK@ toggle"}},

};

static struct layout layouts[] = {
	{tile, "tile"},
};

static uint32_t keyboard_repeat_rate =  50;
static uint32_t keyboard_repeat_delay = 300;
