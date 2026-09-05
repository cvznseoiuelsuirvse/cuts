#ifndef CUTS_COMPOSITOR_UTIL_H
#define CUTS_COMPOSITOR_UTIL_H

#include <stdint.h>

struct c_wl_surface;

double get_surface_scale(struct c_wl_surface *surface);
void get_surface_raw_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height);
void get_surface_logical_buf_size(struct c_wl_surface *surface, double *width, double *height);
void get_surface_size(struct c_wl_surface *surface, double *width, double *height);

#endif
