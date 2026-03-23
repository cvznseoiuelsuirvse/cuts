#ifndef CUTS_BACKEND_DRM_UTIL_H
#define CUTS_BACKEND_DRM_UTIL_H

#include <stdint.h>
#include <xf86drmMode.h>

int drm_format_num_planes(uint32_t format);
int drm_refresh_rate_mhz(drmModeModeInfo *mode);
const char *drm_connector_str(uint32_t connector_type);

#endif
