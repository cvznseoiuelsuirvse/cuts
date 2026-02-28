#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <gbm.h>

#include "backend/drm.h"
#include "util/log.h"

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



// int c_drm_backend_page_flip(struct c_drm_backend *backend) {
//   struct c_drm_connector *connector = backend->connector;
//
//   if (!connector->waiting_for_flip) {
//     if (!drmModePageFlip(backend->fd, connector->crtc_id, backend->fb_id, DRM_MODE_PAGE_FLIP_EVENT, connector))
//       connector->waiting_for_flip = 1;
//   }
//   return 0;
// }
//
// void c_drm_backend_page_flip2(struct c_drm_backend *backend, int *needs_redraw) {
//   struct c_drm_connector *connector = backend->connector;
//
//   if (!connector->waiting_for_flip && *needs_redraw) {
//     if (!drmModePageFlip(backend->fd, connector->crtc_id, backend->fb_id, DRM_MODE_PAGE_FLIP_EVENT, connector))
//       connector->waiting_for_flip = 1;
//   }
//
//   *needs_redraw = 0;
// }



static int c_drm_backend_get_connector(struct c_drm_backend *backend, drmModeResPtr resource) {
  drmModeConnectorPtr connector;
  uint32_t taken_crtcs = 0;
  for (int i = 0; i < resource->count_connectors; i++) {
    connector = drmModeGetConnector(backend->fd, resource->connectors[i]);
    if (!connector) continue;
    if (connector->connection == DRM_MODE_CONNECTED) {
      struct c_drm_connector *c_connector = calloc(1, sizeof(struct c_drm_connector));

      c_connector->waiting_for_flip = 0;
      c_connector->drmModeConn = connector;
      c_connector->id = connector->connector_id;

      uint32_t crtc_id = get_crtc_id(backend->fd, resource, connector, &taken_crtcs);
      c_connector->crtc_id = crtc_id;

      c_connector->mode = get_preferred_mode(connector);
      if (!c_connector->mode) {
        c_connector->mode = &connector->modes[0];
      }

      c_connector->orig_crtc = drmModeGetCrtc(backend->fd, crtc_id);

      backend->connector = c_connector;
      return 0;

    } else 
      drmModeFreeConnector(connector);
  }

  return -1;
}

void c_drm_backend_free(struct c_drm_backend *drm) {
  struct c_drm_connector *connector = drm->connector;
  if (connector) {
    if (connector->orig_crtc) {
      drmModeSetCrtc(drm->fd, 
                     connector->orig_crtc->crtc_id, connector->orig_crtc->buffer_id, 
                     0, 0, &connector->id, 1, &connector->orig_crtc->mode);
      drmModeFreeCrtc(connector->orig_crtc);
    }
    drmModeFreeConnector(connector->drmModeConn);
    free(connector);
  }

  close(drm->fd);
  free(drm);
}

int c_drm_backend_dev_id(struct c_drm_backend *drm, dev_t *dev_id) {
	struct stat stat;
	if (fstat(drm->fd, &stat) != 0) {
		c_log(C_LOG_ERROR, "fstat failed");
		return -1;
	}

	*dev_id = stat.st_rdev;
  return 0;
}

struct c_drm_backend *c_drm_backend_init() {
  struct c_drm_backend *backend = calloc(1, sizeof(struct c_drm_backend));
  if (!backend) return NULL;

  int fd = -1;
  char path[128];
  for (int i = 0; i < 10; i++) {
    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/dev/dri/card%d", i);

    fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd > 0) break;
  }

  if (fd < 0) {
    c_log(C_LOG_ERROR, "No cards found");
    goto error;
  }

  backend->fd = fd;
  c_log(C_LOG_INFO, "Using card %s", path);

  drmModeResPtr resource = drmModeGetResources(fd);
  if (!resource) goto error;

  if (c_drm_backend_get_connector(backend, resource) == -1) goto error;

  drmModeFreeResources(resource);
  return backend;

error:
  c_drm_backend_free(backend);
  return NULL;
}
