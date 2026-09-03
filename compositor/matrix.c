#include <string.h>
#include "compositor/matrix.h"

void matrix3_new(mat3 m) {
  m[0][0] = 1.0f;
  m[1][1] = 1.0f;
  m[2][2] = 1.0f;
}

void matrix3_multiply(mat3 m1, mat3 m2, mat3 out) {
  int s = 3;
  for (int x = 0; x < s; x++) {
    for (int y = 0; y < s; y++) {
      out[x][y] = 0;
      for (int k = 0; k < s; k++) {
        out[x][y] += m1[x][k] * m2[k][y];
      }
    }
  }
}
 
void matrix3_translate(mat3 m, double x, double y) {
  m[0][2] += m[0][0] * x + m[0][1] * y;
  m[1][2] += m[1][0] * x + m[1][1] * y;
}

void matrix3_scale(mat3 m, double x, double y) {
  m[0][0] *= x;
  m[1][0] *= x;

  m[0][1] *= y;
  m[1][1] *= y;
}

void matrix3_rotate(mat3 m, enum wl_output_transform_enum transform) {

  mat3 f = {0};
  f[0][0] = 1.0f;
  f[1][1] = 1.0f;
  f[2][2] = 1.0f;

  if (transform >= WL_OUTPUT_TRANSFORM_FLIPPED) {
    f[0][0] = -1.0f;
  }

  mat3 rot = {0};
  rot[2][2] = 1.0f;

  switch (transform) {
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_90:
      rot[0][1] = -1.0f;
      rot[1][0] = 1.0f;
      break;

    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
    case WL_OUTPUT_TRANSFORM_180:
      rot[0][0] = -1.0f;
      rot[1][1] = -1.0f;
      break;

    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
    case WL_OUTPUT_TRANSFORM_270:
      rot[0][1] = 1.0f;
      rot[1][0] = -1.0f;
      break;

    default:
      rot[0][0] = 1.0f;
      rot[1][1] = 1.0f;
      break;
  }

  mat3 tmp;
  mat3 result;
  matrix3_multiply(f, rot, tmp);
  matrix3_multiply(m, tmp, result);
  memcpy(m, result, sizeof(mat3));
}
