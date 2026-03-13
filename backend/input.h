#ifndef CUTS_BACKEND_INPUT_H
#define CUTS_BACKEND_INPUT_H

#include <libinput.h>

struct c_input {
	struct libinput *dev;
};


struct c_input *c_input_init();

#endif
