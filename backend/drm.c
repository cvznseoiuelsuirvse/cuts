#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <gbm.h>

#include "backend/drm.h"
#include "util/log.h"

#define c_unused __attribute__((unused))

c_unused static const char *drm_connector_str(uint32_t conn_type) {
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

int drm_format_num_planes(uint32_t format) {
    switch (format) {
    case DRM_FORMAT_C8:
    case DRM_FORMAT_RGB332:
    case DRM_FORMAT_BGR233:
    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_ARGB2101010:
        return 1;
    
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_NV24:
    case DRM_FORMAT_NV42:
    case DRM_FORMAT_P010:
    case DRM_FORMAT_P012:
    case DRM_FORMAT_P016:
        return 2;

    case DRM_FORMAT_YUV410:
    case DRM_FORMAT_YVU410:
    case DRM_FORMAT_YUV411:
    case DRM_FORMAT_YVU411:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_YVU422:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_YVU444:
        return 3;

    default:
        return 0; 
    }
}

c_unused static int drm_calc_refresh_rate(drmModeModeInfo *mode) {
	int res = (mode->clock * 1000000LL / mode->htotal + mode->vtotal / 2) / mode->vtotal;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		res *= 2;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		res /= 2;

	if (mode->vscan > 1)
		res /= mode->vscan;

	return res;
}

static drmModeModeInfoPtr get_preferred_mode(drmModeConnector *connector) {
  drmModeModeInfoPtr mode = NULL;
  for (int i = 0; i < connector->count_modes; i++) {
    mode = &connector->modes[i];
    if (mode->type & DRM_MODE_TYPE_PREFERRED) break;
  }
  return mode;
}

static uint32_t get_crtc_id(int fd, drmModeResPtr res, drmModeConnectorPtr conn, uint32_t *taken_crtcs) {
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



// int c_drm_page_flip(struct c_drm *drm) {
//   struct c_drm_connector *connector = backend->connector;
//
//   if (!connector->waiting_for_flip) {
//     if (!drmModePageFlip(backend->fd, connector->crtc_id, backend->fb_id, DRM_MODE_PAGE_FLIP_EVENT, connector))
//       connector->waiting_for_flip = 1;
//   }
//   return 0;
// }
//
// void c_drm_page_flip2(struct c_drm *drm, int *needs_redraw) {
//   struct c_drm_connector *connector = backend->connector;
//
//   if (!connector->waiting_for_flip && *needs_redraw) {
//     if (!drmModePageFlip(backend->fd, connector->crtc_id, backend->fb_id, DRM_MODE_PAGE_FLIP_EVENT, connector))
//       connector->waiting_for_flip = 1;
//   }
//
//   *needs_redraw = 0;
// }



static int c_drm_get_connector(struct c_drm *drm, drmModeResPtr resource) {
  drmModeConnectorPtr connector;
  uint32_t taken_crtcs = 0;
  for (int i = 0; i < resource->count_connectors; i++) {
    connector = drmModeGetConnector(drm->fd, resource->connectors[i]);
    if (!connector) continue;
    if (connector->connection == DRM_MODE_CONNECTED) {
      struct c_drm_connector *c_connector = &drm->connector;

      c_connector->waiting_for_flip = 0;
      c_connector->conn = connector;
      c_connector->id = connector->connector_id;

      uint32_t crtc_id = get_crtc_id(drm->fd, resource, connector, &taken_crtcs);
      c_connector->crtc_id = crtc_id;

      c_connector->mode.info = get_preferred_mode(connector);
      if (!c_connector->mode.info) {
        c_connector->mode.info = &connector->modes[0];
      }

      c_connector->mode.width = c_connector->mode.info->hdisplay;
      c_connector->mode.height = c_connector->mode.info->vdisplay;
      c_connector->orig_crtc = drmModeGetCrtc(drm->fd, crtc_id);

      return 0;

    } else 
      drmModeFreeConnector(connector);
  }

  return -1;
}

void c_drm_free(struct c_drm *drm) {
  struct c_drm_connector connector = drm->connector;
  if (connector.orig_crtc) {
    drmModeSetCrtc(drm->fd, 
                   connector.orig_crtc->crtc_id, connector.orig_crtc->buffer_id, 
                   0, 0, &connector.id, 1, &connector.orig_crtc->mode);
    drmModeFreeCrtc(connector.orig_crtc);
  }
  drmModeFreeConnector(connector.conn);

  free(drm);
}

int c_drm_dev_id(struct c_drm *drm, dev_t *dev_id) {
	struct stat stat;
	if (fstat(drm->fd, &stat) != 0) {
		c_log(C_LOG_ERROR, "fstat failed");
		return -1;
	}

	*dev_id = stat.st_rdev;
  return 0;
}

struct c_drm *c_drm_init(int drm_fd) {
  struct c_drm *drm = calloc(1, sizeof(struct c_drm));
  if (!drm) return NULL;

  drm->fd = drm_fd;

  drmModeResPtr resource = drmModeGetResources(drm_fd);
  if (!resource) goto error;

  if (c_drm_get_connector(drm, resource) == -1) goto error;


  drmModeFreeResources(resource);
  return drm;

error:
  c_drm_free(drm);
  return NULL;
}
