#ifndef CUTS_RENDER_CURSOR_H
#define CUTS_RENDER_CURSOR_H

#include "backend/drm/cursor.h"

enum c_cursor_state {
	C_CURSOR_NORMAL,
};

struct c_cursor {
	char 				theme[256];
	enum c_cursor_state state;

	struct c_drm_cursor *backend_cursor;
};


struct c_cursor *c_cursor_init(struct c_drm *drm, const char *theme);

#endif
