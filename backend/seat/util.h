#ifndef CUTS_BACKEND_SEAT_UTIL_H
#define CUTS_BACKEND_SEAT_UTIL_H

#include <stdint.h>

#define __DRM_SET_MASTER  0x641e
#define __DRM_DROP_MASTER 0x641f

int get_active_vt();
int drm_ioctl(int fd, int req);

int terminal_open();
int terminal_set_graphics(int fd, int enable);
int terminal_set_keyboard(int fd, int enable);

#endif
