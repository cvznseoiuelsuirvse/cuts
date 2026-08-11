#ifndef CUTS_OUTPUT_DRM_SYNCOBJ_H
#define CUTS_OUTPUT_DRM_SYNCOBJ_H

#include <stdint.h>

struct c_drm_sync_object {
  int drm_fd;
  uint32_t handle;
  uint64_t point;
};

void c_drm_sync_object_free(struct c_drm_sync_object *syncobj);
struct c_drm_sync_object *c_drm_sync_object_init(int drm_fd);

int c_drm_import_sync_file(struct c_drm_sync_object *syncobj, int sync_file_fd);
int c_drm_export_sync_file(struct c_drm_sync_object *syncobj);

#endif
