#include <stdint.h>
#include "wayland/types.h"

double c_wl_fixed_to_double(c_wl_fixed f) {
    union di {
      double d;
      long i;
    } u;

    u.i = ((1023L + 44L) << 52) + (1L << 51) + f;

    return u.d - (3L << 43);
}


c_wl_fixed c_wl_fixed_from_double(double d) {
  union di {
    double d;
    long i;
  } u;
  u.d = d + (3L << (51 - 8));
  return (c_wl_fixed)u.i;
}
