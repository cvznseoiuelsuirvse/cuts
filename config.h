#include "cuts.h"
#include "seat/input.h"

#define LEADER C_KEYBOARD_MOD_ALT

#define TAG(n, shift_key)                                                      \
  {LEADER, XKB_KEY_##n, switch_tag, {.u = 1 << (n - 1)}}, {                    \
    LEADER | C_KEYBOARD_MOD_SHIFT, shift_key, window_move_to_workspace, {      \
      .u = 1 << (n - 1)                                                        \
    }                                                                          \
  }

#define MOUSE_DRAG(func) 1, .drag_handler=func
#define MOUSE_CLICK(func) 0, .handler=func

static enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;

static const uint32_t gap = 15;
static const uint32_t background = 0x00000000;

static const float mfact =      0.5f;
static const uint32_t nmaster = 2;

static struct border border = {
  .width = 1,
  .c_focus = 0x777777ff,
  .c_default = 0x444444ff,
};

static struct xkb_rule_names xkb_rules = {
    .layout = "us,ru",
    .options = "grp:toggle,caps:escape",
};

static struct monitor monitors[] = {
  {"eDP-1", 0, 0, 2560, 1600, 165},
};

static struct key_bind keys[] = {
	{LEADER,                        XKB_KEY_q,                        quit, 			     {}},
	{LEADER,                        XKB_KEY_Return, 	                spawn, 	         {.s = "alacritty"}},
	{LEADER,                        XKB_KEY_b, 	                      spawn, 	         {.s = "firefox"}},
	{LEADER,                        XKB_KEY_c, 	                      spawn, 	         {.s = "chromium --enable-features=UseOzonePlatform --ozone-platform=wayland"}},
	{LEADER,                        XKB_KEY_x, 	  	                  window_kill,	   {}},
	{LEADER,                        XKB_KEY_j, 	  	                  move_focus,      {.i = -1}},
	{LEADER,                        XKB_KEY_k, 	  	                  move_focus,      {.i = 1}},
  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_F,                        toggle_floating, {}},
  {0,                             XKB_KEY_XF86AudioRaiseVolume, 	  spawn, 				   {.s = "wpctl set-volume -l 1 @DEFAULT_SINK@ 10%+"}},
  {0,                             XKB_KEY_XF86AudioLowerVolume, 	  spawn, 				   {.s = "wpctl set-volume -l 1 @DEFAULT_SINK@ 10%-"}},
  {0,                             XKB_KEY_XF86AudioMute, 			      spawn, 				   {.s = "wpctl set-mute @DEFAULT_SINK@ toggle"}},

  TAG(1, XKB_KEY_exclam),
  TAG(2, XKB_KEY_at),
  TAG(3, XKB_KEY_numbersign),
  TAG(4, XKB_KEY_dollar),
  TAG(5, XKB_KEY_percent),
};

static struct mouse_bind mouse[] = {
	{LEADER, 0, BTN_LEFT, MOUSE_DRAG(window_move), {}},
	{LEADER, 0, BTN_RIGHT, MOUSE_DRAG(window_resize), {}},
};

static struct layout layouts[] = {
	{tile, "tile"},
};

static const uint32_t keyboard_repeat_rate =  50;
static const uint32_t keyboard_repeat_delay = 300;
