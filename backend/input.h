#ifndef CUTS_BACKEND_INPUT_H
#define CUTS_BACKEND_INPUT_H

#include <libinput.h>

#include "backend/backend.h"
#include "util/event_loop.h"


struct c_input_mouse_event {
	struct libinput_event_pointer *libinput_event;
	double x, y;
	int     abs;
};

struct c_input_listener_mouse {
	int (*on_mouse_movement)(struct c_input_mouse_event *event, void *userdata);
	int (*on_mouse_scroll)  (struct c_input_mouse_event *event, void *userdata);
	int (*on_mouse_button)  (struct c_input_mouse_event *event, void *userdata);
};

struct c_input_keyboard_event {
	struct libinput_event_keyboard *libinput_event;
	int32_t  key;
	int 	 pressed;
	uint64_t pressed_time;
};

struct c_input_listener_kbd {
	int (*on_kbd_key)     (struct c_input_keyboard_event *event, void *userdata);
};

struct c_input {
	struct libinput *libinput;

	uint8_t capabilities;
	c_list *listeners;
};

struct c_input *c_input_init(struct c_event_loop *loop, struct c_backend *backend);
void c_input_free(struct c_input *input);

void c_input_add_listener_mouse(struct c_input *input, struct c_input_listener_mouse *listener, void *userdata);
void c_input_add_listener_kbd(struct c_input *input, struct c_input_listener_kbd *listener, void *userdata);

#endif
