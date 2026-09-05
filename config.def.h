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

static const uint32_t tags = 5;
static const char *tag_lables[] = {"1", "2", "3", "4", "5"};

static uint32_t gap = 5;
static uint32_t border_width = 1;

static const char *cursor_theme   = "default";
static const uint32_t cursor_size = 32;

#define BACKGROUND1    0x000000ff
#define BACKGROUND2    0x1a1a1aff
#define BACKGROUND3    0x2f2f2fff
#define BACKGROUND4    0xffffffff
#define BORDER_FOCUS   0x888888ff
#define BORDER_UNFOCUS 0x333333ff
#define BORDER_URGENT  BACKGROUND4
#define FONT_FOCUS     BACKGROUND4
#define FONT_UNFOCUS   0x777777ff
#define FONT_URGENT    BACKGROUND1

static const struct bar_config bar_cfg = {
  .font = {12, "Noto Sans Font Mono"},
  .pos = BAR_TOP,

  .tag = {
      .font_active         = COLOR_TO_BYTES(FONT_FOCUS),
      .font_inactive       = COLOR_TO_BYTES(FONT_UNFOCUS),
      .font_urgent         = COLOR_TO_BYTES(FONT_URGENT),
      .background_active   = COLOR_TO_BYTES(BACKGROUND3),
      .background_inactive = COLOR_TO_BYTES(BACKGROUND2),
      .background_urgent   = COLOR_TO_BYTES(BACKGROUND4),
  },

  .layout = {
      .font_color          = COLOR_TO_BYTES(FONT_FOCUS),
      .background_color    = COLOR_TO_BYTES(BACKGROUND2),
  },

  .title = {
      .font_color          = COLOR_TO_BYTES(FONT_FOCUS),
      .background_color    = COLOR_TO_BYTES(BACKGROUND1),
  },

  .text = {
      .font_color          = COLOR_TO_BYTES(FONT_UNFOCUS),
      .background_color    = COLOR_TO_BYTES(BACKGROUND2),
  },

};

static const uint32_t background_color[4] = COLOR_TO_BYTES(BACKGROUND1);
static const uint32_t border_active[4]    = COLOR_TO_BYTES(BORDER_FOCUS);
static const uint32_t border_inactive[4]  = COLOR_TO_BYTES(BORDER_UNFOCUS);
static const uint32_t border_urgent[4]    = COLOR_TO_BYTES(BORDER_URGENT);

static float    mfact   = 0.5f;
static uint32_t nmaster = 1;

static enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
static const uint32_t keyboard_repeat_rate  = 50;
static const uint32_t keyboard_repeat_delay = 300;

static struct xkb_rule_names xkb_rules = {
    .layout = "us",
    .options = "grp:toggle,caps:escape",
};

static struct monitor_config monitors[] = {};
static const char *autostart[] = {
  // "swaybg -i ~/img.png",
};

static struct layout layouts[] = {
	{tile, "//"},
	{zoom, "\\/"},
};

#define TERM "alacritty"
#define MENU "bemenu-run"

static struct key_bind keys[] = {
	{LEADER,                        XKB_KEY_q,          quit, 			       {}},
	{LEADER,                        XKB_KEY_Return, 	  spawn, 	           {.s = TERM}},
	{LEADER,                        XKB_KEY_p, 	        spawn, 	           {.s = MENU}},
	{LEADER,                        XKB_KEY_x, 	  	    window_kill,	     {}},
	{LEADER,                        XKB_KEY_j, 	  	    move_focus,        {.i = 1}},
	{LEADER,                        XKB_KEY_k, 	  	    move_focus,        {.i = -1}},
  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_F,          toggle_floating,   {}},
  {LEADER,                        XKB_KEY_f,          toggle_fullscreen, {}},
  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_I,          change_mfact,      {.d = -0.05f}},
  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_O,          change_mfact,      {.d = 0.05f}},

  {LEADER,                        XKB_KEY_o,          change_nmaster,    {.i = -1}},
  {LEADER,                        XKB_KEY_i,          change_nmaster,    {.i = 1}},

  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_M,          set_layout,        {.p = &layouts[1]}},
  {LEADER,                        XKB_KEY_m,          set_layout,        {.p = &layouts[0]}},

  {LEADER,                        XKB_KEY_minus,      change_gap,        {.i = -10}},
  {LEADER,                        XKB_KEY_equal,      change_gap,        {.i = 10}},

  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_underscore, change_border,     {.i = -2}},
  {LEADER | C_KEYBOARD_MOD_SHIFT, XKB_KEY_plus,       change_border,     {.i = 2}},

  TAG(1, XKB_KEY_exclam),
  TAG(2, XKB_KEY_at),
  TAG(3, XKB_KEY_numbersign),
  TAG(4, XKB_KEY_dollar),
  TAG(5, XKB_KEY_percent),
};

static struct mouse_bind mouse[] = {
	{LEADER, 0, BTN_LEFT,  MOUSE_DRAG(window_move),   {}},
	{LEADER, 0, BTN_RIGHT, MOUSE_DRAG(window_resize), {}},
};
