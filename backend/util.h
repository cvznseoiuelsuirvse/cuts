#ifndef CUTS_DRM_UTIL_H
#define CUTS_DRM_UTIL_H

#include <xf86drm.h>
#include <xf86drmMode.h>

const char *connector_str(uint32_t conn_type);
int calc_refresh_rate(drmModeModeInfo *mode);
drmModeModeInfoPtr get_preferred_mode(drmModeConnector *connector);
uint32_t get_crtc_id(int fd, drmModeResPtr res, drmModeConnectorPtr conn, uint32_t *taken_crtcs);

#endif
