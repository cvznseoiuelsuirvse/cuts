#ifndef CUTS_BACKEND_DRM_UTIL_H
#define CUTS_BACKEND_DRM_UTIL_H

#include <stdint.h>
#include <xf86drmMode.h>

int drm_format_num_planes(uint32_t format);
double drm_refresh_rate(drmModeModeInfo *mode);
const char *drm_connector_str(uint32_t connector_type);
enum wl_shm_format_enum drm_to_wl_shm_format(uint32_t format);
void drm_print_mode(drmModeModeInfo *mode);

#endif
