#include <drm.h>
#include <xf86drm.h>
#include <stdlib.h>

#include "util/log.h"
#include "output/drm/syncobj.h"

#define drm_call(func, err_label, ...)                                         \
  if (func(__VA_ARGS__)) {                                                     \
    c_log_errno(C_LOG_ERROR, #func " failed");                                 \
    goto err_label;                                                            \
  }

void c_drm_sync_object_free(struct c_drm_sync_object *syncobj) {
  if (syncobj->handle)
    drm_call(drmSyncobjDestroy, out, syncobj->drm_fd, syncobj->handle);

out:
  free(syncobj);
}

struct c_drm_sync_object *c_drm_sync_object_init(int drm_fd) {
  struct c_drm_sync_object *syncobj = calloc(1, sizeof(*syncobj));
  if (!syncobj) {
    c_log_errno(C_LOG_ERROR, "failed to allocate syncobj");
    return NULL;
  }

  syncobj->drm_fd = drm_fd;
  syncobj->point = 0;
  drm_call(drmSyncobjCreate, error, syncobj->drm_fd, 0, &syncobj->handle);

  return syncobj;

error:
  c_drm_sync_object_free(syncobj);
  return NULL;
}

int c_drm_import_sync_file(struct c_drm_sync_object *syncobj, int sync_file_fd) {
  uint32_t handle;
  drm_call(drmSyncobjCreate,error, syncobj->drm_fd, 0, &handle);
  drm_call(drmSyncobjImportSyncFile, error_after_create, syncobj->drm_fd,
           handle, sync_file_fd);
  drm_call(drmSyncobjTransfer, error_after_create, syncobj->drm_fd,
           syncobj->handle, syncobj->point, handle, 0, 0);
  drmSyncobjDestroy(syncobj->drm_fd, handle);
  return 0;

error_after_create:
  drmSyncobjDestroy(syncobj->drm_fd, handle);
error:
  return -1;
}

int c_drm_export_sync_file(struct c_drm_sync_object *syncobj) {
  int sync_file_fd;
  uint32_t handle;

  drm_call(drmSyncobjCreate, error, syncobj->drm_fd, 0, &handle);
  drm_call(drmSyncobjTransfer, error_after_create, syncobj->drm_fd, handle, 0,
           syncobj->handle, syncobj->point, 0);
  drm_call(drmSyncobjExportSyncFile, error_after_create, syncobj->drm_fd,
           handle, &sync_file_fd);

  drmSyncobjDestroy(syncobj->drm_fd, handle);
  return sync_file_fd;

error_after_create:
  drmSyncobjDestroy(syncobj->drm_fd, handle);
error:
  return -1;
}
