#ifndef CUTS_COMPOSITOR_MATRIX_H
#define CUTS_COMPOSITOR_MATRIX_H

#include "wayland/proto/wayland.h"

typedef float mat3[3][3];
void matrix3_new(mat3 m);
void matrix3_multiply(mat3 m1, mat3 m2, mat3 result);
void matrix3_translate(mat3 m, double x, double y);
void matrix3_scale(mat3 m, double x, double y);
void matrix3_rotate(mat3 m, enum wl_output_transform_enum transform);

#endif
