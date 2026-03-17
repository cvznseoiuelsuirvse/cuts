#include <stdlib.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <string.h>
#include <errno.h>

#include "backend/drm/drm.h"
#include "backend/drm/cursor.h"

#include "util/log.h"

static int on_mouse_movement_cb(struct c_input_mouse_event *event, void *userdata) {
  c_log(C_LOG_DEBUG, "mouse x=%d y=%d", event->x, event->y);
  struct c_drm *drm = userdata;
  struct c_output *output = drm->output;
  int32_t new_x, new_y;
  if (!event->abs) {
    new_x = event->x + output->cursor->x;
    new_y = event->y + output->cursor->y;

    if (new_x < 0) new_x = 0;
    if (new_x > (int32_t)output->width) new_x = output->width;

    if (new_y < 0) new_y = 0;
    if (new_y > (int32_t)output->height) new_y = output->height;

    output->cursor->x = new_x;
    output->cursor->y = new_y;

    drmModeMoveCursor(drm->fd, drm->connector.crtc_id, new_x, new_y);
    
  }
  return 0;
}

int c_drm_cursor_write(struct c_drm *drm, struct c_drm_cursor *cursor, uint32_t *buffer, size_t buffer_size) {
  if (gbm_bo_write(cursor->gbm_bo, buffer, buffer_size) != 0) {
    c_log(C_LOG_ERROR, "gbm_bo_write failed: %s", strerror(errno));
    return -1;
  }

  size_t cursor_size = buffer_size / sizeof(uint32_t) / 2;
  uint32_t bo_handle = gbm_bo_get_handle(cursor->gbm_bo).u32;
  if (drmModeSetCursor(drm->fd, drm->connector.crtc_id, bo_handle, cursor_size, cursor_size) != 0) {
    c_log(C_LOG_ERROR, "drmModeSetCursor failed: %s", strerror(errno));
    return -1;
  }

  return 0;
}

void c_drm_cursor_free(struct c_drm_cursor *cursor) {
  if (cursor->gbm_bo) gbm_bo_destroy(cursor->gbm_bo);
  if (cursor->gbm_bo_next) gbm_bo_destroy(cursor->gbm_bo_next);
  free(cursor);
}

struct c_drm_cursor *c_drm_cursor_init(struct c_drm *drm, struct c_input *input) {
  struct c_drm_cursor *cursor = calloc(1, sizeof(*cursor));
  if (!cursor) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  uint64_t w, h;
  if (drmGetCap(drm->fd, DRM_CAP_CURSOR_WIDTH, &w) != 0) w = 32;
  if (drmGetCap(drm->fd, DRM_CAP_CURSOR_HEIGHT, &h) != 0) h = 32;

  cursor->width = w;
  cursor->width = h;

  cursor->gbm_bo = gbm_bo_create(drm->gbm_device, w, h, GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);
  if (!cursor->gbm_bo) {
    c_log(C_LOG_ERROR, "gbm_bo_create failed: %s", strerror(errno));
    goto error;
  }

  struct c_input_listener_mouse input_listener_mouse = {
    .on_mouse_movement = on_mouse_movement_cb,
  };
  c_input_add_listener_mouse(input, &input_listener_mouse, drm);

  return cursor;

error:
  c_drm_cursor_free(cursor);
  return NULL;
}
