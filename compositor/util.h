#ifndef CUTS_COMPOSITOR_UTIL_H
#define CUTS_COMPOSITOR_UTIL_H

#include <stdint.h>

struct c_wl_surface;

void get_surface_raw_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height);
void get_surface_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height);
void get_surface_size(struct c_wl_surface *surface, double *width, double *height);

#endif
