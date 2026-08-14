#include "cuts.h"
#include "seat/input.h"

#define LEADER C_KEYBOARD_MOD_LOGO

#define TAG(n, shift_key)                                                      \
  {LEADER, XKB_KEY_##n, switch_tag, {.u = 1 << (n - 1)}}, {                    \
    LEADER | C_KEYBOARD_MOD_SHIFT, shift_key, window_move_to_workspace, {      \
      .u = 1 << (n - 1)                                                        \
    }                                                                          \
  }

#define COLOR_TO_BYTES(n)                                                      \
  {                                                                            \
      ((n >> 24) & 0xff),                                                      \
      ((n >> 16) & 0xff),                                                      \
      ((n >> 8) & 0xff),                                                       \
      ((n >> 0) & 0xff),                                                       \
  }

#define MOUSE_DRAG(func) 1, .drag_handler=func
#define MOUSE_CLICK(func) 0, .handler=func

static enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;

static const uint32_t tags = 5;

static uint32_t gap          = 15;
static const uint32_t background_color[4] = COLOR_TO_BYTES(0x2D2D34FF);

static uint32_t border_width              = 3;
static const uint32_t border_focus[4]     = COLOR_TO_BYTES(0xCEB1BEFF);
static const uint32_t border_default[4]   = COLOR_TO_BYTES(0xB97375FF);

// bar
static const int font_size = 14;
static const uint32_t font_color[4]    = COLOR_TO_BYTES(0xFFFFFFFF);
static const enum bar_position bar_pos = BAR_TOP;

static float mfact      = 0.5f;
static uint32_t nmaster = 2;

static const uint32_t keyboard_repeat_rate =  50;
static const uint32_t keyboard_repeat_delay = 300;

static struct xkb_rule_names xkb_rules = {
    .layout = "us,ru",
    .options = "grp:toggle,caps:escape",
};

static struct monitor monitors[] = {};

static struct layout layouts[] = {
	{tile, "//"},
	{zoom, "\\/"},
};

static struct key_bind keys[] = {
	{LEADER,                        XKB_KEY_q,                        quit, 			       {}},
	{LEADER,                        XKB_KEY_Return, 	                spawn, 	           {.s = "alacritty"}},
	{LEADER,                        XKB_KEY_j, 	  	                  move_focus,        {.i = 1}},
	{LEADER,                        XKB_KEY_k, 	  	                  move_focus,        {.i = -1}},
  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_F,                        toggle_floating,   {}},
  {LEADER,                        XKB_KEY_f,                        toggle_fullscreen, {}},
  {LEADER,                        XKB_KEY_i,                        change_mfact,      {.d = -0.05f}},
  {LEADER,                        XKB_KEY_o,                        change_mfact,      {.d = 0.05f}},

  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_I,                        change_nmaster,    {.i = -1}},
  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_O,                        change_nmaster,    {.i = 1}},

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
