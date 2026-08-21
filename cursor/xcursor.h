#ifndef CUTS_CURSOR_XCURSOR_H
#define CUTS_CURSOR_XCURSOR_H

#include "cursor/cursor.h"

const char *str_cursor_shape(enum c_cursor_shape shape);
void c_xcursor_unload_image(struct c_cursor *cur);
int c_xcursor_load_image(struct c_cursor *cur);

#endif
