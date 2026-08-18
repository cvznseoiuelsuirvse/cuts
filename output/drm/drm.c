#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "output/drm/drm.h"
#include "output/drm/syncobj.h"
#include "output/drm/util.h"

#include "output/output.h"

#include "render/framebuffer.h"

#include "util/log.h"
#include "util/helpers.h"

static void get_output_make_model(int drm_fd, struct c_output *output) {
  struct c_output_drm_object *obj = &output->connector;

  uint32_t blob_id;
  drmModePropertyRes *prop_res = NULL;

  for (size_t i = 0; i < obj->props->count_props; i++) {
    if (STREQ(obj->props_info[i]->name, "EDID")) {
      prop_res = obj->props_info[i];
      blob_id = obj->props->prop_values[i];
      break;
    }
  }

  if (!prop_res) {
    c_log(C_LOG_INFO, "no EDID found in output %s", output->name);
    return;
  }


  uint32_t prop_type = drmModeGetPropertyType(prop_res);

  if (prop_type == DRM_MODE_PROP_BLOB) {
    drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(drm_fd, blob_id);

    if (blob && blob->data) {
      print_buffer(blob->data, blob->length, stderr);
      fprintf(stderr, "\n");

      uint16_t manufacturer_bytes =  *(uint8_t *)(blob->data + 8) << 8 | *(uint8_t *)(blob->data + 9);
      output->model =  *(uint16_t *)(blob->data + 10);
      output->serial =  *(uint32_t *)(blob->data + 12);

      output->make[0] = ((manufacturer_bytes & 0x7C00) >> 10) + 64;
      output->make[1] = ((manufacturer_bytes & 0x3E0) >> 5) + 64;
      output->make[2] = (manufacturer_bytes & 0x1F) + 64;
      output->make[3] = 0;

      for (size_t i = 0; i < 3; i++) {
        char *b = blob->data + 72 + (i * 18);
        if (*(uint32_t *)b == 0xfc000000 || *(uint32_t *)b == 0xfe000000) {
          size_t j;
          for (j = 0; j < 14; ++j) {
            if (*(uint8_t *)(b + 4 + j) == 0xA) break;
          }
          memcpy(output->manufacturer_name, b + 5, j - 1);
          break;
        }
      }
    } else {
      c_log(C_LOG_INFO, "EDID blob is empty");
    }

    drmModeFreePropertyBlob(blob);
  }
}

static int set_object_property_value(drmModeAtomicReq *req,
                              struct c_output_drm_object *obj, const char *name,
                              uint64_t value) {
  uint32_t prop_id = 0;

  for (size_t i = 0; i < obj->props->count_props; i++) {
    if (STREQ(obj->props_info[i]->name, name)) {
      prop_id = obj->props_info[i]->prop_id;
      break;
    }
  }

  return drmModeAtomicAddProperty(req, obj->id, prop_id, value);
}

static c_list *get_modes(int fd, drmModeConnectorPtr connector) {
  c_list *modes = c_list_new();

  for (int i = 0; i < connector->count_modes; i++) {
    drmModeModeInfo mode = connector->modes[i];
    double refresh_rate = drm_refresh_rate(&mode);

    struct c_output_mode c_mode = {0};
    c_mode.width = mode.hdisplay; 
    c_mode.height = mode.vdisplay; 
    c_mode.preferred = mode.type & DRM_MODE_TYPE_PREFERRED;
    c_mode.refresh_rate = refresh_rate;
    c_mode.drm.info = mode;

    if (drmModeCreatePropertyBlob(fd, &mode, sizeof(mode), &c_mode.drm.blob_id)) {
      c_log_errno(C_LOG_ERROR, "failed to create mode propery blob");
      goto error;
    }

    c_list_push(modes, &c_mode, sizeof(c_mode));
  }

  return modes;

  struct c_output_mode *mode;

error:
  c_list_for_each(modes, mode)
    drmModeDestroyPropertyBlob(fd, mode->drm.blob_id);
  c_list_destroy(modes);
  return NULL;
}

static int64_t get_property_value(int fd, drmModeObjectPropertiesPtr props, const char *name) {
  uint64_t value = -1;
	int found = 0;
	for (size_t i = 0; i < props->count_props && !found; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
		if (STREQ(prop->name, name))
			value = props->prop_values[i];
		drmModeFreeProperty(prop);
	}

	return value;
}

static void free_properties(struct c_output_drm_object *obj) {
	for (size_t i = 0; i < obj->props->count_props; i++)
		drmModeFreeProperty(obj->props_info[i]);
	free(obj->props_info);
	drmModeFreeObjectProperties(obj->props);
}


static void get_object_properties(int fd, struct c_output_drm_object *obj, uint32_t type) {
	obj->props = drmModeObjectGetProperties(fd, obj->id, type);
	obj->props_info = calloc(obj->props->count_props, sizeof(obj->props_info));

	for (size_t i = 0; i < obj->props->count_props; i++)
		obj->props_info[i] = drmModeGetProperty(fd, obj->props->props[i]);
}

static int get_crtc(int fd, drmModeResPtr res, drmModeConnectorPtr conn,
                         uint32_t *taken_crtcs,
                         struct c_output_drm_object *obj) {
  int ret = -1;
  for (int enc_n = 0; enc_n < conn->count_encoders; enc_n++) {
    drmModeEncoderPtr encoder = drmModeGetEncoder(fd, res->encoders[enc_n]);
    if (!encoder) continue;

    for (int i = 0; i < res->count_crtcs; i++) {
      uint32_t bit = 1 << i;

      if (!(encoder->possible_crtcs & bit)) continue;
      if (*taken_crtcs & bit) continue;

      drmModeFreeEncoder(encoder);
      *taken_crtcs |= bit;

      obj->id = res->crtcs[i];
      ret = i;
      goto get_props;
    }
    drmModeFreeEncoder(encoder);
  }

get_props:
  if (ret >= 0) {
    get_object_properties(fd, obj, DRM_MODE_OBJECT_CRTC);
    if (!obj->props)
      return -1;
  }
  return ret;
}

static int get_plane(int fd, uint32_t crtc, struct c_output_drm_object *obj) {
  int ret = -1;
  drmModePlaneResPtr plane_res = drmModeGetPlaneResources(fd);

  for (size_t i = 0; i < plane_res->count_planes; i++) {
    drmModePlanePtr plane = drmModeGetPlane(fd, plane_res->planes[i]);
    if (plane->possible_crtcs & (1 << crtc)) {
      drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
      if (get_property_value(fd, props, "type") == DRM_PLANE_TYPE_PRIMARY) {
        obj->id = plane->plane_id;
        drmModeFreeObjectProperties(props);
        drmModeFreePlane(plane);
        ret = 0;
        goto get_props;

      }
      drmModeFreeObjectProperties(props);
    }
    drmModeFreePlane(plane);
  }

get_props:
  drmModeFreePlaneResources(plane_res);

  if (!ret) {
    get_object_properties(fd, obj, DRM_MODE_OBJECT_PLANE);
    if (!obj->props)
      return -1;
  }

  return ret;

}

static int get_output_drm_objects(int drm_fd, struct c_output *output,
                              drmModeConnectorPtr connector,
                              drmModeResPtr resource, uint32_t *taken_crtcs) {

  output->connector.id = connector->connector_id;
  get_object_properties(drm_fd, &output->connector, DRM_MODE_OBJECT_CONNECTOR);
  if (!output->connector.props) {
    c_log(C_LOG_ERROR, "failed to get connector props");
    goto error_connector;
  }

  int crtc_idx;
  if ((crtc_idx = get_crtc(drm_fd, resource, connector, taken_crtcs, &output->crtc)) == -1) {
    c_log(C_LOG_ERROR, "failed to get CRTC");
    goto error_crtc;
  }

  if (get_plane(drm_fd, crtc_idx, &output->plane) == -1) {
    c_log(C_LOG_ERROR, "failed to get primary plane");
    goto error_plane;
  }

  return 0;

error_plane:
  free_properties(&output->crtc);
error_crtc:
  free_properties(&output->connector);
error_connector:
  return -1;
}

void c_drm_free_output(int drm_fd, struct c_output* output) {
  drmModeSetCrtc(drm_fd, output->orig_crtc->crtc_id, output->orig_crtc->buffer_id,
                 0, 0, &output->connector.id, 1, &output->orig_crtc->mode);
  drmModeFreeCrtc(output->orig_crtc);


  if (output->plane.props_info) {
    free_properties(&output->plane);
  }

  if (output->crtc.props_info) {
    free_properties(&output->crtc);
  }

  if (output->connector.props_info) {
    free_properties(&output->connector);
  }

  if (output->timeline) {
    c_drm_sync_object_free(output->timeline);
  }

  c_list_destroy(output->modes);
}

c_list *c_drm_get_outputs(int drm_fd) {
  drmModeResPtr resources = drmModeGetResources(drm_fd);
  if (!resources) {
    c_log_errno(C_LOG_ERROR, "failed to get DRM resources");
    return NULL;
  }

  c_list *outputs = c_list_new();

  drmModeConnectorPtr connector;
  uint32_t taken_crtcs = 0;

  for (int i = 0; i < resources->count_connectors; i++) {
    connector = drmModeGetConnector(drm_fd, resources->connectors[i]);
    if (!connector) continue;
    if (connector->connection != DRM_MODE_CONNECTED) goto iter_end;

    struct c_output output = {0};

    if (get_output_drm_objects(drm_fd, &output, connector, resources, &taken_crtcs)) goto iter_end_error;


    output.orig_crtc = drmModeGetCrtc(drm_fd, output.crtc.id);

    output.mm_width = connector->mmWidth;
    output.mm_height = connector->mmHeight;
    output.subpixel = connector->subpixel;

    output.modes = get_modes(drm_fd, connector);
    snprintf(output.name, sizeof(output.name), "%s-%d",
             drm_connector_str(connector->connector_type),
             connector->connector_type_id);

    get_output_make_model(drm_fd, &output);

    output.timeline = c_drm_sync_object_init(drm_fd);
    if (!output.timeline) {
      c_log(C_LOG_WARNING, "failed to create timeline sync object for output %s", output.name);
    }

    c_list_push(outputs, &output, sizeof(output));

iter_end:
    drmModeFreeConnector(connector);
    continue;

iter_end_error:
    drmModeFreeConnector(connector);
    c_list_destroy(outputs);
    outputs = NULL;
    goto out;
  }

out:
  drmModeFreeResources(resources);
  if (!taken_crtcs && outputs)
    c_list_destroy(outputs);
  return outputs;
}

int c_drm_atomic_commit(int drm_fd, struct c_output *output,
                        struct c_output_mode *mode, int flags, void *userdata,
                        int in_fence_fd) {
  int ret = -1;

  drmModeAtomicReqPtr req = drmModeAtomicAlloc();
  struct c_output_drm_object *plane = &output->plane;

  if (set_object_property_value(req, &output->connector, "CRTC_ID", output->crtc.id) < 0)         goto set_prop_error;
	if (set_object_property_value(req, &output->crtc, "MODE_ID", mode->drm.blob_id) < 0)            goto set_prop_error;
	if (set_object_property_value(req, &output->crtc, "ACTIVE", 1) < 0)                             goto set_prop_error;
	if (set_object_property_value(req, plane, "FB_ID", c_output_backbuffer(output)->drm_fb_id) < 0) goto set_prop_error;
	if (set_object_property_value(req, plane, "CRTC_ID", output->crtc.id) < 0)                      goto set_prop_error;
	if (set_object_property_value(req, plane, "SRC_X", 0) < 0)                                      goto set_prop_error;
	if (set_object_property_value(req, plane, "SRC_Y", 0) < 0)                                      goto set_prop_error;
	if (set_object_property_value(req, plane, "SRC_W", mode->width << 16) < 0)                      goto set_prop_error;
	if (set_object_property_value(req, plane, "SRC_H", mode->height << 16) < 0)                     goto set_prop_error;
	if (set_object_property_value(req, plane, "CRTC_X", 0) < 0)                                     goto set_prop_error;
	if (set_object_property_value(req, plane, "CRTC_Y", 0) < 0)                                     goto set_prop_error;
	if (set_object_property_value(req, plane, "CRTC_W", mode->width) < 0)                           goto set_prop_error;
	if (set_object_property_value(req, plane, "CRTC_H", mode->height) < 0)                          goto set_prop_error;
  if (in_fence_fd > -1 && set_object_property_value(req, plane, "IN_FENCE_FD", in_fence_fd) < 0)  goto set_prop_error;

  ret = 0;

  if (!req) {
    c_log_errno(C_LOG_ERROR, "failed to prepare atomic request");
    ret = -1;
    goto out;
  }

  ret = drmModeAtomicCommit(drm_fd, req, flags, userdata);
  if (ret) {
    c_log_errno(C_LOG_ERROR, "drmModeAtomicCommit failed");
    ret = -1;
    goto out;
  }

out:
  drmModeAtomicFree(req);
  return ret;

set_prop_error:
  c_log_errno(C_LOG_ERROR, "failed to set property");
  drmModeAtomicFree(req);
  return -1;
}
