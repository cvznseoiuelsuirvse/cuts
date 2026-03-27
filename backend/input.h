#ifndef CUTS_BACKEND_INPUT_H
#define CUTS_BACKEND_INPUT_H

#include <libinput.h>
#include <xkbcommon/xkbcommon.h>

#include "util/event_loop.h"

enum c_input_keyboard_mods {
	C_KEYBOARD_MOD_SHIFT = 1 << 0,
	C_KEYBOARD_MOD_CTRL =  1 << 2,
	C_KEYBOARD_MOD_ALT  =  1 << 3,
	C_KEYBOARD_MOD_LOGO =  1 << 6,
};

enum c_input_mouse_buttons {
	C_MOUSE_BUTTON_RIGHT = 1,
};

enum c_input_mouse_axis_source {
	C_MOUSE_AXIS_SOURCE_WHEEL = 1,
	C_MOUSE_AXIS_SOURCE_FINGER,
	C_MOUSE_AXIS_SOURCE_CONTINUOUS,
	C_MOUSE_AXIS_SOURCE_WHEEL_TILT,
};

struct c_input_mouse_event {
	struct libinput_event_pointer *libinput_event;
	double x, y;
	int     abs;

	enum c_input_mouse_buttons button;
	int      button_pressed;

	double axis;
	double axis_discrete;
	enum c_input_mouse_axis_source axis_source;
};

struct c_input_event_listener_mouse {
	int (*on_mouse_movement)(struct c_input_mouse_event *event, void *userdata);
	int (*on_mouse_scroll)  (struct c_input_mouse_event *event, void *userdata);
	int (*on_mouse_button)  (struct c_input_mouse_event *event, void *userdata);
};

struct c_input_keyboard_event {
	struct libinput_event_keyboard *libinput_event;
	int32_t  key;
	int 	 pressed;
	int 	 changed;

	xkb_mod_mask_t 	   mods_depressed;
	xkb_mod_mask_t 	   mods_latched;
	xkb_mod_mask_t 	   mods_locked;
	xkb_layout_index_t group;
};

struct c_input_event_listener_keyboard {
	int (*on_keyboard_key)     (struct c_input_keyboard_event *event, void *userdata);
};

struct c_input_libinput_interface {
	void *userdata;
	int (*open_restricted)(const char *path, int flags, void *user_data);
	void (*close_restricted)(int fd, void *user_data);
};

struct c_input {
	struct libinput *libinput;

	struct {
		struct xkb_context    *ctx;
		struct xkb_keymap 	  *keymap;
		struct xkb_state	  *state;
		struct xkb_rule_names *rule_names;
	} xkb;

	uint8_t capabilities;

	c_list *event_listeners;
	c_list *shortcut_listeners;
};

struct c_input *c_input_init(struct c_event_loop *loop, struct c_input_libinput_interface *libinput_interface);
void c_input_free(struct c_input *input);

int c_input_init_xkb_state(struct c_input *input, struct xkb_rule_names *rule_names);
int c_input_get_xkb_keymap_fd(struct c_input *input, int *fd);

void c_input_add_event_listener_mouse(struct c_input *input, 
									  struct c_input_event_listener_mouse *listener, void *userdata);

void c_input_add_event_listener_keyboard(struct c_input *input, 
									struct c_input_event_listener_keyboard *listener, void *userdata);

void c_input_add_shortcut_handler(struct c_input *input, uint32_t mod_mask, xkb_keysym_t keysym, 
                                  void(*handler)(void *userdata), void *userdata);

#endif
