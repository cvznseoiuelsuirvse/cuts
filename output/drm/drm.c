#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "output/drm/drm.h"
#include "output/drm/util.h"
#include "output/output.h"

#include "util/log.h"

static c_list *get_modes(drmModeConnectorPtr connector) {
  c_list *modes = c_list_new();

  for (int i = 0; i < connector->count_modes; i++) {
    drmModeModeInfo mode = connector->modes[i];
    double refresh_rate = drm_refresh_rate(&mode);

    struct c_output_mode c_mode = {0};
    c_mode.width = mode.hdisplay; 
    c_mode.height = mode.vdisplay; 
    c_mode.preferred = mode.type & DRM_MODE_TYPE_PREFERRED;
    c_mode.drm_info = mode;
    c_mode.refresh_rate = refresh_rate;

    c_list_push(modes, &c_mode, sizeof(c_mode));
  }

  return modes;
}

static uint32_t get_crtc_id(int fd, drmModeResPtr res, drmModeConnectorPtr conn, uint32_t *taken_crtcs) {
  for (int enc_n = 0; enc_n < conn->count_encoders; enc_n++) {
    drmModeEncoderPtr encoder = drmModeGetEncoder(fd, res->encoders[enc_n]);
    if (!encoder) continue;

    for (int i = 0; i < res->count_crtcs; i++) {
      uint32_t bit = 1 << i;

      if (!(encoder->possible_crtcs & bit)) continue;
      if (*taken_crtcs & bit) continue;

      drmModeFreeEncoder(encoder);
      *taken_crtcs |= bit;

      return res->crtcs[i];
    }
    drmModeFreeEncoder(encoder);
  }

  return 0;
}

c_list *c_drm_get_connectors(int drm_fd) {
  drmModeResPtr resource = drmModeGetResources(drm_fd);
  if (!resource) {
    c_log_errno(C_LOG_ERROR, "failed to get DRM resources");
    return NULL;
  }

  c_list *outputs = c_list_new();

  drmModeConnectorPtr connector;
  uint32_t taken_crtcs = 0;
  for (int i = 0; i < resource->count_connectors; i++) {
    connector = drmModeGetConnector(drm_fd, resource->connectors[i]);
    if (!connector) continue;
    if (connector->connection != DRM_MODE_CONNECTED) goto iter_end;

    struct c_output output = {0};

    output.connector_id = connector->connector_id;
    output.crtc_id = get_crtc_id(drm_fd, resource, connector, &taken_crtcs);
    output.orig_crtc = drmModeGetCrtc(drm_fd, output.crtc_id);

    output.mm_width = connector->mmWidth;
    output.mm_height = connector->mmHeight;
    output.subpixel = connector->subpixel;

    output.modes = get_modes(connector);
    snprintf(output.name, sizeof(output.name), "%s-%d",
             drm_connector_str(connector->connector_type),
             connector->connector_type_id);

    c_list_push(outputs, &output, sizeof(output));

iter_end:
    drmModeFreeConnector(connector);
  }

  drmModeFreeResources(resource);
  return outputs;
}
