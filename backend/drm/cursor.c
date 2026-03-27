#include <stdlib.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "backend/drm/drm.h"
#include "backend/drm/cursor.h"

#include "util/log.h"

static int on_mouse_movement_cb(struct c_input_mouse_event *event, void *userdata) {
  struct c_drm *drm = userdata;
  struct c_output *output = drm->output;
  struct c_output_mode *preferred_mode = c_drm_get_preferred_mode(drm);
  uint32_t width = preferred_mode->width;
  uint32_t height = preferred_mode->height;

  double new_x, new_y;
  if (!event->abs) {
    new_x = event->x + output->cursor->x;
    new_y = event->y + output->cursor->y;
  } else {
    new_x = libinput_event_pointer_get_absolute_x_transformed(event->libinput_event, width);
    new_y = libinput_event_pointer_get_absolute_y_transformed(event->libinput_event, height);
  }

  if (new_x < 0) new_x = 0;
  if (new_x > width) new_x = width;

  if (new_y < 0) new_y = 0;
  if (new_y > height) new_y = height;

  output->cursor->x = new_x;
  output->cursor->y = new_y;
  
  event->x = new_x;
  event->y = new_y;

  drmModeMoveCursor(drm->fd, drm->connector.crtc_id, new_x, new_y);
  
  return 0;
}

int c_cursor_update(struct c_drm *drm, struct c_cursor *cursor) {
  if (gbm_bo_write(cursor->gbm_bo, cursor->image, cursor->image_size) != 0) {
    c_log_errno(C_LOG_ERROR, "gbm_bo_write failed");
    return -1;
  }

  uint32_t bo_handle = gbm_bo_get_handle(cursor->gbm_bo).u32;
  if (drmModeSetCursor(drm->fd, drm->connector.crtc_id, bo_handle, cursor->width, cursor->height) != 0) {
    c_log_errno(C_LOG_ERROR, "drmModeSetCursor failed");
    return -1;
  }

  return 0;
}

void c_cursor_free(struct c_cursor *cursor) {
  if (cursor->gbm_bo) gbm_bo_destroy(cursor->gbm_bo);
  if (cursor->image) free(cursor->image);
  free(cursor);
}

struct c_cursor *c_cursor_init(struct c_drm *drm, struct c_input *input) {
  struct c_cursor *cursor = calloc(1, sizeof(*cursor));
  if (!cursor) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  uint64_t w, h;
  if (drmGetCap(drm->fd, DRM_CAP_CURSOR_WIDTH, &w) != 0) w = 32;
  if (drmGetCap(drm->fd, DRM_CAP_CURSOR_HEIGHT, &h) != 0) h = 32;

  cursor->width = w;
  cursor->height = h;

  cursor->gbm_bo = gbm_bo_create(drm->gbm_device, w, h, GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);
  if (!cursor->gbm_bo) {
    c_log_errno(C_LOG_ERROR, "gbm_bo_create failed");
    goto error;
  }

  cursor->image_size = w * h * sizeof(uint32_t);
  cursor->image = calloc(cursor->image_size, 1);
  if (!cursor->image) {
    c_log(C_LOG_ERROR, "calloc failed");
    goto error;
  }

  struct c_input_event_listener_mouse input_listener_mouse = {
    .on_mouse_movement = on_mouse_movement_cb,
  };
  c_input_add_event_listener_mouse(input, &input_listener_mouse, drm);

  return cursor;

error:
  c_cursor_free(cursor);
  return NULL;
}
