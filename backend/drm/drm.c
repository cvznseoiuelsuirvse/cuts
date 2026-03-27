#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "backend/drm/drm.h"
#include "backend/drm/util.h"
#include "backend/drm/cursor.h"
#include "backend/input.h"

#include "util/log.h"


static void add_modes(drmModeConnectorPtr connector, struct c_output *output) {
  output->modes = c_list_new();

  for (int i = 0; i < connector->count_modes; i++) {
    drmModeModeInfoPtr mode = &connector->modes[i];
    struct c_output_mode c_mode = {
      .width = mode->hdisplay, 
      .height = mode->vdisplay, 
      .refresh = drm_refresh_rate_mhz(mode),
      .preferred = mode->type & DRM_MODE_TYPE_PREFERRED,
      .info = mode,
    };
    c_list_push(output->modes, &c_mode, sizeof(c_mode));
  }
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
      c_connector->orig_crtc = drmModeGetCrtc(drm->fd, crtc_id);

      drm->output->mm_width = connector->mmWidth;
      drm->output->mm_height = connector->mmHeight;
      drm->output->subpixel = connector->subpixel;

      add_modes(connector, drm->output);
      snprintf(drm->output->name, sizeof(drm->output->name), "%s-%d", 
               drm_connector_str(connector->connector_type), connector->connector_type_id);
      return 0;

    } else 
      drmModeFreeConnector(connector);
  }

  return -1;
}

int c_drm_dev_id(struct c_drm *drm, dev_t *dev_id) {
	struct stat stat;
	if (fstat(drm->fd, &stat) != 0)
		return -1;

	*dev_id = stat.st_rdev;
  return 0;
}

struct c_output_mode *c_drm_get_preferred_mode(struct c_drm *drm) {
  struct c_output_mode *mode;
  c_list_for_each(drm->output->modes, mode) {
    if (mode->preferred) return mode;
  }
  assert(0);
}

void c_drm_free(struct c_drm *drm) {
  struct c_drm_connector connector = drm->connector;
  if (drm->output) {
    if (drm->output->cursor) {
      memset(drm->output->cursor->image, 0, drm->output->cursor->image_size);
      c_cursor_update(drm, drm->output->cursor);
      c_cursor_free(drm->output->cursor);
    }
    c_list_destroy(drm->output->modes);
    free(drm->output);
  }

  if (connector.orig_crtc) {
    drmModeSetCrtc(drm->fd, 
                   connector.orig_crtc->crtc_id, connector.orig_crtc->buffer_id, 
                   0, 0, &connector.id, 1, &connector.orig_crtc->mode);
    drmModeFreeCrtc(connector.orig_crtc);
  }
  drmModeFreeConnector(connector.conn);

  if (drm->gbm_device) gbm_device_destroy(drm->gbm_device);

  free(drm);
}

struct c_drm *c_drm_init(int drm_fd, struct c_input *input) {
  struct c_drm *drm = calloc(1, sizeof(*drm));
  if (!drm) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  drm->fd = drm_fd;

  drmModeResPtr resource = drmModeGetResources(drm_fd);
  if (!resource) goto error;

  drm->output = calloc(1, sizeof(*drm->output));
  if (!drm->output) {
    c_log(C_LOG_ERROR, "calloc failed");
    goto error_resources;
  }

  if (c_drm_get_connector(drm, resource) == -1) goto error_resources;

  drm->gbm_device = gbm_create_device(drm->fd);
  if (!drm->gbm_device) {
    c_log_errno(C_LOG_ERROR, "gbm_create_device failed");
    goto error;
  }

  struct c_cursor *cursor = c_cursor_init(drm, input);
  if (!cursor) goto error_resources;
  drm->output->cursor = cursor;

  memset(cursor->image, 0, cursor->image_size);

  for (int y = 0; y < 10; y++) {
    for (int x = 0; x < 10; x++) {
      cursor->image[y * cursor->height + x] = 0xFFFFFFFF;
    }
  }
  c_cursor_update(drm, cursor);

  drmModeFreeResources(resource);

  return drm;

error_resources:
  drmModeFreeResources(resource);

error:
  c_drm_free(drm);
  return NULL;
}
