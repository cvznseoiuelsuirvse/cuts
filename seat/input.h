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

enum c_input_mouse_axis_source {
	C_MOUSE_AXIS_SOURCE_WHEEL,
	C_MOUSE_AXIS_SOURCE_FINGER,
	C_MOUSE_AXIS_SOURCE_CONTINUOUS,
	C_MOUSE_AXIS_SOURCE_WHEEL_TILT,
};

struct c_input_mouse_event {
	struct libinput_event_pointer *libinput_event;
	double x, y;
  int abs;

	uint32_t button;
	int      is_pressed;

	double axis;
	double axis120;
	double axis_discrete;
	enum c_input_mouse_axis_source axis_source;
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

struct c_input_events {
	void (*mouse_movement)(struct c_input_mouse_event *event, void *userdata);
	void (*mouse_scroll)  (struct c_input_mouse_event *event, void *userdata);
	void (*mouse_button)  (struct c_input_mouse_event *event, void *userdata);
	void (*keyboard_key)  (struct c_input_keyboard_event *event, void *userdata);
};

struct c_input_libinput_interface {
	void *userdata;
	int (*open_restricted)(const char *path, int flags, void *userdata);
	void (*close_restricted)(int fd, void *userdata);
};

struct c_input_config {
  enum libinput_config_accel_profile accel_profile;
};

struct c_input {
	struct libinput *libinput;
  struct c_input_config *config;

	struct {
		struct xkb_context    *ctx;
		struct xkb_keymap 	  *keymap;
		struct xkb_state	    *state;
		struct xkb_rule_names *rule_names;
	} xkb;

	uint8_t capabilities;

	c_list *event_cb;
	c_list *combo_cb;

  struct {
    uint32_t mod_mask;
    uint32_t button;

    void    (*drag_handler)(int, void *);
    void     *drag_handler_userdata;
  } state;
};

struct c_input_combo {
  struct {
  } keyboard;

  struct {
    uint32_t btn;
    int      drag;
  } mouse;
};

struct c_input *
c_input_init(struct c_event_loop *loop,
             struct c_input_libinput_interface *libinput_interface,
             struct c_input_config *config);
void c_input_free(struct c_input *input);

int c_input_init_xkb_state(struct c_input *input, struct xkb_rule_names *rule_names);
int c_input_get_xkb_keymap_fd(struct c_input *input, int *fd);

void c_input_add_event_listener(struct c_input *input, struct c_input_events *subs, void *userdata);
void c_input_add_combo(struct c_input *input, uint32_t mod_mask,
                       xkb_keysym_t keysym, uint32_t btn,
                       void (*handler)(void *userdata), void *userdata);
void c_input_add_drag_combo(struct c_input *input, uint32_t mod_mask,
                            xkb_keysym_t keysym, uint32_t btn,
                            void (*handler)(int done, void *userdata),
                            void *userdata);
#endif
