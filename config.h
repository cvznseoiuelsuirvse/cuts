#include "cuts.h"
#include "backend/input.h"

static uint32_t gap = 15;
static struct xkb_rule_names xkb_rules = {
    .layout = "us,ru",
    .options = "grp:toggle,caps:escape",
};

#define MOD_KEY C_MOD_ALT

static struct bind binds[] = {
	{MOD_KEY, XKB_KEY_q,		quit, 			{}},
	{MOD_KEY, XKB_KEY_Return, 	spawn_client, 	{.s = "alacritty"}},
	{MOD_KEY, XKB_KEY_b, 	    spawn_client, 	{.s = "firefox"}},
	{MOD_KEY, XKB_KEY_x, 	  	kill_client,  	{}},
	{MOD_KEY, XKB_KEY_j, 	  	focus,  		{.i = 1}},
	{MOD_KEY, XKB_KEY_k, 	  	focus,  		{.i = -1}},
};

static struct layout layouts[] = {
	{tile, "tile"},
};
