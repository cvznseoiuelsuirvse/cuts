#ifndef CUTS_BACKEND_SEAT_VT_H
#define CUTS_BACKEND_SEAT_VT_H

int vt_open(int vt);
int vt_disable(int fd);
int vt_enable(int fd);
int vt_set_graphics(int fd, int enable);
int vt_set_keyboard(int fd, int enable);
int vt_get_current_num();
int vt_acquire(int fd);
int vt_release(int fd);

#endif
