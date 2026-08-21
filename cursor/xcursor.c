#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "cursor/cursor.h"
#include "util/log.h"
#include "util/helpers.h"

struct xc_header {
  char magic[4];
  uint32_t size;
  uint32_t version;
  uint32_t ntoc;
};

struct xc_toc_header {
  uint32_t type;
  uint32_t subtype;
  uint32_t position;
};

struct xc_chunk_header {
  uint32_t header;
  uint32_t type;
  uint32_t subtype;
  uint32_t version;
};

struct xc_chunk_image {
  struct xc_chunk_header header;
  uint32_t width;
  uint32_t height;
  uint32_t xhot;
  uint32_t yhot;
  uint32_t delay;
};

struct xc_chunk_text {
  struct xc_chunk_header header;
  uint32_t length;
};

const char *str_cursor_shape(enum c_cursor_shape shape) {
  switch (shape) {
    case C_CURSOR_DEFAULT:        return "default";
    case C_CURSOR_CONTEXT_MENU:   return "context-menu";
    case C_CURSOR_HELP:           return "help";
    case C_CURSOR_POINTER:        return "pointer";
    case C_CURSOR_PROGRESS:       return "progress";
    case C_CURSOR_WAIT:           return "wait";
    case C_CURSOR_CELL:           return "cell";
    case C_CURSOR_CROSSHAIR:      return "crosshair";
    case C_CURSOR_TEXT:           return "text";
    case C_CURSOR_VERTICAL_TEXT:  return "vertical-text";
    case C_CURSOR_ALIAS:          return "alias";
    case C_CURSOR_COPY:           return "copy";
    case C_CURSOR_MOVE:           return "move";
    case C_CURSOR_NO_DROP:        return "no-drop";
    case C_CURSOR_NOT_ALLOWED:    return "not-allowed";
    case C_CURSOR_GRAB:           return "grab";
    case C_CURSOR_GRABBING:       return "grabbing";
    case C_CURSOR_E_RESIZE:       return "e-resize";
    case C_CURSOR_N_RESIZE:       return "n-resize";
    case C_CURSOR_NE_RESIZE:      return "ne-resize";
    case C_CURSOR_NW_RESIZE:      return "nw-resize";
    case C_CURSOR_S_RESIZE:       return "s-resize";
    case C_CURSOR_SE_RESIZE:      return "se-resize";
    case C_CURSOR_SW_RESIZE:      return "sw-resize";
    case C_CURSOR_W_RESIZE:       return "w-resize";
    case C_CURSOR_EW_RESIZE:      return "ew-resize";
    case C_CURSOR_NS_RESIZE:      return "ns-resize";
    case C_CURSOR_NESW_RESIZE:    return "nesw-resize";
    case C_CURSOR_NWSE_RESIZE:    return "nwse-resize";
    case C_CURSOR_COL_RESIZE:     return "col-resize";
    case C_CURSOR_ROW_RESIZE:     return "row-resize";
    case C_CURSOR_ALL_SCROLL:     return "all-scroll";
    case C_CURSOR_ZOOM_IN:        return "zoom-in";
    case C_CURSOR_ZOOM_OUT:       return "zoom-out";
    case C_CURSOR_DND_ASK:        return "dnd-ask";
    case C_CURSOR_ALL_RESIZE:     return "all-resize";
  }

  return "unknown";
}

void c_xcursor_unload_image(struct c_cursor *cur) {
  struct c_cursor_frame *frame = cur->frame; 
  for (size_t i = 0; i < cur->frames_n; i++) {
    struct c_cursor_frame *next = frame->next;
    free(frame->image);
    free(frame);
    frame = next;
  }

  cur->frames_n = 0;
  cur->frame = NULL;
}

struct c_cursor_frame *load_frame(struct c_cursor *cur, FILE *f, size_t offset, struct c_cursor_frame *prev) {
  struct xc_chunk_image img;
  if (!read_at(&img, sizeof(img), 1, offset, f)) {
    c_log_errno(C_LOG_ERROR, "failed to read image chunk");
    return NULL;
  }

  struct c_cursor_frame *frame = calloc(1, sizeof(*frame));
  if (!frame) {
    c_log_errno(C_LOG_ERROR, "failed to allocate frame");
    return NULL;
  }

  if (prev)
    prev->next = frame;


  frame->hot_x = img.xhot;
  frame->hot_y = img.yhot;
  frame->wait_ms = img.delay;
  frame->cur = cur;

  size_t image_buf_size = cur->max_size * cur->max_size * 4;
  size_t image_size = img.width * img.height * 4;

  frame->image = calloc(1, image_buf_size);
  if (!frame->image) {
    c_log_errno(C_LOG_ERROR, "failed to allocate frame image");
    goto error;
  }

  uint32_t *temp = malloc(image_size);
  if (!frame->image) {
    c_log_errno(C_LOG_ERROR, "failed to allocate frame image (temp)");
    goto error_temp;
  }

  if (read_at(temp, 1, image_size, offset + sizeof(img), f) != (int)image_size) {
    c_log_errno(C_LOG_ERROR, "failed to copy frame image");
    goto error_copy;
  }

  for (uint32_t y = 0; y < cur->size; y++) {
    for (uint32_t x = 0; x < cur->size; x++) {
      frame->image[y * cur->max_size + x] = temp[y * cur->size + x];
    }
  }

  free(temp);
  cur->frames_n++;

  return frame;

error_copy:
  free(temp);

error_temp:
  free(frame->image);

error:
  free(frame);
  return NULL;
}

int c_xcursor_load_image(struct c_cursor *cur) {
  char local_icons_dir[256];
  char cursor_file[512];

  const char *homepath = getenv("HOME");
  if (!homepath) {
    c_log(C_LOG_ERROR, "couldn't get 'HOME' env var");
    return 1;
  }
  snprintf(local_icons_dir, sizeof(local_icons_dir), "%s/.local/share/icons", homepath);

  if (cur->dir == _CUR_LOCAL_DIR)
    snprintf(cursor_file, sizeof(cursor_file), "%s/%s/cursors/%s", local_icons_dir, cur->theme, str_cursor_shape(cur->shape));
  else if (cur->dir == _CUR_SYS_DIR)
    snprintf(cursor_file, sizeof(cursor_file), "/usr/share/icons/%s/cursors/%s", cur->theme, str_cursor_shape(cur->shape));

  FILE *f = fopen(cursor_file, "r");
  if (!f) {
    c_log(C_LOG_ERROR, "failed to open %s", cursor_file);
    return 1;
  }

  int ret = 0;
  int read_items;

  struct xc_header header;
  if ((read_items = fread(&header, sizeof(header), 1, f)) < 1) {
    c_log(C_LOG_ERROR, "failed to read cursor header");
    ret = 1;
    goto out;
  }

  uint32_t size_diff = 1024; // uint instead of int so it won't go negative
  uint32_t lowest_subtype = 0;
  uint32_t fit_subtype = 0;

  struct xc_toc_header toc_head;
  for (size_t i = 0; i < header.ntoc; i++) {
    if (!read_at(&toc_head, sizeof(toc_head), 1, header.size + i * sizeof(toc_head), f)) {
      c_log_errno(C_LOG_ERROR, "failed to read toc header %d (enumerating sizes)", i);
      ret = 1;
      goto out;
    }
    if (toc_head.subtype == lowest_subtype)
      break;

    if (toc_head.type == 0xFFFD0002) {
      uint32_t d = cur->size - toc_head.subtype;

      if (!lowest_subtype)
        lowest_subtype = toc_head.subtype;

      if (d < size_diff) {
        size_diff = d;
        fit_subtype = toc_head.subtype;
      }
    }
  }
  cur->size = fit_subtype;

  struct c_cursor_frame *prev = NULL;
  for (size_t i = 0; i < header.ntoc; i++) {
    if (!read_at(&toc_head, sizeof(toc_head), 1, header.size + i * sizeof(toc_head), f)) {
      c_log_errno(C_LOG_ERROR, "failed to read toc header %d (loading frames)", i);
      ret = 1;
      goto out;
    }

    if (toc_head.type == 0xFFFD0002 && toc_head.subtype == fit_subtype) {
      prev = load_frame(cur, f, toc_head.position, prev);
      if (!prev) {
        ret = 1;
        goto frames_error;
      }

      if (!cur->frame) cur->frame = prev;
    }
  }

  // loop
  prev->next = cur->frame;
  goto out;

frames_error:
  c_xcursor_unload_image(cur); 

out:
  fclose(f);
  return ret;
}
