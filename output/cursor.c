#include <stdlib.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "output/cursor.h"
#include "seat/input.h"
#include "util/log.h"

struct __pointer_area {
  uint32_t x, y, width, height;
}; 

static void on_mouse_movement_cb(struct c_input_mouse_event *event, void *userdata) {
  struct c_output_manager *mgr = userdata;

  struct c_output *output = mgr->cursor_output;
  struct c_output_mode *mode = output->current_mode;

  uint32_t width = mode->width;
  uint32_t height = mode->height;

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


  struct __pointer_area current_area = {output->x, output->y, mode->width, mode->height};

  if (!CURSOR_INSIDE(new_x, new_y, &current_area)) {
    struct c_output *output;
    c_list_for_each(mgr->outputs, output) {
      struct c_output_mode *n_mode = output->current_mode;
      struct __pointer_area new_area = {output->x, output->y, n_mode->width, n_mode->height};
      if (CURSOR_INSIDE(new_x, new_y, &new_area)) {
        mgr->cursor_output = output;
      }
    }
  }

  drmModeMoveCursor(mgr->drm_fd, output->crtc_id, new_x, new_y);
}

int c_cursor_update(struct c_output_manager *mgr, struct c_output *output, uint32_t *buffer,
                    size_t buffer_size) {
  struct c_cursor *cursor = output->cursor;
  if (gbm_bo_write(cursor->gbm_bo, buffer, buffer_size * sizeof(uint32_t)) != 0) {
    c_log_errno(C_LOG_ERROR, "gbm_bo_write failed");
    return -1;
  }

  uint32_t bo_handle = gbm_bo_get_handle(cursor->gbm_bo).u32;
  if (drmModeSetCursor(mgr->drm_fd, output->crtc_id, bo_handle, cursor->width, cursor->height) != 0) {
    c_log_errno(C_LOG_ERROR, "drmModeSetCursor failed");
    return -1;
  }

  return 0;
}

void c_cursor_free(struct c_cursor *cursor) {
  if (cursor->gbm_bo) gbm_bo_destroy(cursor->gbm_bo);
  free(cursor);
}

struct c_cursor *c_cursor_init(struct c_output_manager *mgr, struct c_input *input) {
  struct c_cursor *cursor = calloc(1, sizeof(*cursor));
  if (!cursor) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  uint64_t w, h;
  if (drmGetCap(mgr->drm_fd, DRM_CAP_CURSOR_WIDTH, &w) != 0) w = 32;
  if (drmGetCap(mgr->drm_fd, DRM_CAP_CURSOR_HEIGHT, &h) != 0) h = 32;

  cursor->width = w;
  cursor->height = h;

  cursor->gbm_bo = gbm_bo_create(mgr->gbm_device, w, h, GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);
  if (!cursor->gbm_bo) {
    c_log_errno(C_LOG_ERROR, "gbm_bo_create failed");
    goto error;
  }

  struct c_input_event_listener_mouse input_listener_mouse = {
    .on_mouse_movement = on_mouse_movement_cb,
  };
  c_input_add_event_listener_mouse(input, &input_listener_mouse, mgr);

  return cursor;

error:
  c_cursor_free(cursor);
  return NULL;
}
