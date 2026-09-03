#ifndef CUTS_WAYLAND_IMPL_WLR_DATA_CONTROL_H
#define CUTS_WAYLAND_IMPL_WLR_DATA_CONTROL_H

#include <stdio.h>
#include "wayland/types.h"

struct c_ext_data_control_source {
  C_WL_BASE;
  const char *mimetypes[64];
  size_t mimes;

  struct c_ext_data_control_device *device;

};

struct c_ext_data_control_device {
  C_WL_BASE;
  struct c_ext_data_control_source *selection;
  struct c_ext_data_control_source *primary_selection;
  struct c_ext_data_control_offer  *offer;
};

#endif
