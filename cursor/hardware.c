#include <stdlib.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "cursor/cursor.h"
#include "util/log.h"

struct hw_data {
  int drm_fd;
	struct gbm_bo *gbm_bo;
};

int hw_move(struct c_cursor *cur, double x, double y, void *data) {
  struct hw_data *d = data;
  int ret = drmModeMoveCursor(d->drm_fd, cur->output->crtc.id, x, y);
  if (ret)
    c_log_errno(C_LOG_ERROR, "drmModeMoveCursor failed");

  return ret;
}

int hw_write(struct c_cursor *cur, void *buffer, size_t size, void *data) {
  int ret = 0;
  struct hw_data *d = data;


  if (gbm_bo_write(d->gbm_bo, buffer, size) != 0) {
    c_log_errno(C_LOG_ERROR, "gbm_bo_write failed");
    ret = 1;
    goto out;
 }

  uint32_t bo_handle = gbm_bo_get_handle(d->gbm_bo).u32;
  if (drmModeSetCursor(d->drm_fd, cur->output->crtc.id, bo_handle, cur->max_size, cur->max_size) != 0) {
    c_log_errno(C_LOG_ERROR, "drmModeSetCursor failed");
    ret = 1;
    goto out;
  }

out:
  return ret;
}

void hw_free(struct c_cursor *cursor, void *data) {
  struct hw_data *d = data;
  if (d->gbm_bo) gbm_bo_destroy(d->gbm_bo);
  free(d);
}

int hw_create(struct c_cursor *cur, void **data, void *userdata) {
  struct c_output_manager *mgr = userdata;
  struct hw_data *d = *data;
  d = calloc(1, sizeof(*d));
  if (!d) {
    c_log_errno(C_LOG_ERROR, "failed to allocate data for hardware cursor");
    return 1;
  }

  d->drm_fd = mgr->drm_fd;

  if (drmGetCap(mgr->drm_fd, DRM_CAP_CURSOR_WIDTH, &cur->max_size) != 0) cur->max_size = cur->size;
  if (drmGetCap(mgr->drm_fd, DRM_CAP_CURSOR_HEIGHT, &cur->max_size) != 0) cur->max_size = cur->size;
  
  if (cur->max_size < cur->size) {
    c_log(C_LOG_WARNING, "requested cursor size is too big. using %d", cur->max_size);
    cur->size = cur->max_size;
  }

  d->gbm_bo = gbm_bo_create(mgr->gbm_device, cur->max_size, cur->max_size,
      GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

  if (!d->gbm_bo) {
    c_log_errno(C_LOG_ERROR, "gbm_bo_create failed");
    goto error;
  }

  *data = d;
  return 0;

error:
  hw_free(cur, d);
  *data = NULL;
  return 1;
}

struct c_cursor_impl hw_impl = {
  .create = hw_create,
  .free   = hw_free,
  .move   = hw_move,
  .write  = hw_write,
};
