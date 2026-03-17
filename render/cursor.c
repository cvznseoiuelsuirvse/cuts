#include <stdlib.h>

#include "render/cursor.h"
#include "util/log.h"

struct c_cursor *c_cursor_init(struct c_drm *drm, const char *theme) {
  struct c_cursor *cursor = calloc(1, sizeof(*cursor));
  if (!cursor) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  return cursor;
}
