#include <xf86drm.h>
#include <xf86drmMode.h>

const char *connector_str(uint32_t conn_type) {
	switch (conn_type) {
	case DRM_MODE_CONNECTOR_Unknown:     return "Unknown";
	case DRM_MODE_CONNECTOR_VGA:         return "VGA";
	case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
	case DRM_MODE_CONNECTOR_Composite:   return "Composite";
	case DRM_MODE_CONNECTOR_SVIDEO:      return "SVIDEO";
	case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
	case DRM_MODE_CONNECTOR_Component:   return "Component";
	case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
	case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
	case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
	case DRM_MODE_CONNECTOR_TV:          return "TV";
	case DRM_MODE_CONNECTOR_eDP:         return "eDP";
	case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
	case DRM_MODE_CONNECTOR_DSI:         return "DSI";
	default:                             return "Unknown";
	}
}

int calc_refresh_rate(drmModeModeInfo *mode) {
	int res = (mode->clock * 1000000LL / mode->htotal + mode->vtotal / 2) / mode->vtotal;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		res *= 2;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		res /= 2;

	if (mode->vscan > 1)
		res /= mode->vscan;

	return res;
}

drmModeModeInfoPtr get_preferred_mode(drmModeConnector *connector) {
  drmModeModeInfoPtr mode = NULL;
  for (int i = 0; i < connector->count_modes; i++) {
    mode = &connector->modes[i];
    if (mode->type & DRM_MODE_TYPE_PREFERRED) break;
  }
  return mode;
}

uint32_t get_crtc_id(int fd, drmModeResPtr res, drmModeConnectorPtr conn, uint32_t *taken_crtcs) {
  for (int enc_n = 0; enc_n < conn->count_encoders; enc_n++) {
    drmModeEncoderPtr encoder = drmModeGetEncoder(fd, res->encoders[enc_n]);
    if (!encoder) continue;

    for (int crtc_n = 0; crtc_n < res->count_crtcs; crtc_n++) {
      uint32_t bit = 1 << crtc_n;
      if ((encoder->possible_crtcs & bit) == 0) continue;
      if (*taken_crtcs & bit) continue;

      drmModeFreeEncoder(encoder);
      *taken_crtcs |= bit;

      return res->crtcs[crtc_n];
    }

    drmModeFreeEncoder(encoder);
  }

  return 0;
}
