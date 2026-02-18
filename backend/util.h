#ifndef CUTS_DRM_UTIL_H
#define CUTS_DRM_UTIL_H

#include <xf86drm.h>
#include <xf86drmMode.h>

const char *c_drm_connector_str(uint32_t conn_type);
int c_drm_calc_refresh_rate(drmModeModeInfo *mode);
drmModeModeInfoPtr c_drm_get_preferred_mode(drmModeConnector *connector);

#endif
