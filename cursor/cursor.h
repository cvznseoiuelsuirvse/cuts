#ifndef CUTS_CURSOR_H
#define CUTS_CURSOR_H

#include <stdint.h>
#include <stdio.h>

#include "output/output.h"
#include "util/event_loop.h"

#define _CUR_SYS_DIR   0
#define _CUR_LOCAL_DIR 1

enum c_cursor_shape {
  C_CURSOR_DEFAULT = 1,
  C_CURSOR_CONTEXT_MENU,
  C_CURSOR_HELP,
  C_CURSOR_POINTER,
  C_CURSOR_PROGRESS,
  C_CURSOR_WAIT,
  C_CURSOR_CELL,
  C_CURSOR_CROSSHAIR,
  C_CURSOR_TEXT,
  C_CURSOR_VERTICAL_TEXT,
  C_CURSOR_ALIAS,
  C_CURSOR_COPY,
  C_CURSOR_MOVE,
  C_CURSOR_NO_DROP,
  C_CURSOR_NOT_ALLOWED,
  C_CURSOR_GRAB,
  C_CURSOR_GRABBING,
  C_CURSOR_E_RESIZE,
  C_CURSOR_N_RESIZE,
  C_CURSOR_NE_RESIZE,
  C_CURSOR_NW_RESIZE,
  C_CURSOR_S_RESIZE,
  C_CURSOR_SE_RESIZE,
  C_CURSOR_SW_RESIZE,
  C_CURSOR_W_RESIZE,
  C_CURSOR_EW_RESIZE,
  C_CURSOR_NS_RESIZE,
  C_CURSOR_NESW_RESIZE,
  C_CURSOR_NWSE_RESIZE,
  C_CURSOR_COL_RESIZE,
  C_CURSOR_ROW_RESIZE,
  C_CURSOR_ALL_SCROLL,
  C_CURSOR_ZOOM_IN,
  C_CURSOR_ZOOM_OUT,
  C_CURSOR_DND_ASK,
  C_CURSOR_ALL_RESIZE,
};

struct c_cursor_frame {
  uint32_t hot_x, hot_y;
  uint32_t wait_ms;
  struct c_cursor *cur;
  struct c_cursor_frame *next;
  uint32_t *image;
};

struct c_cursor {
  uint32_t size;
  uint64_t max_size;

  char theme[64];
  uint32_t dir;

  int timer_fd;
  enum c_cursor_shape shape;
  uint32_t shapes;

  struct c_cursor_frame *frame;
  size_t frames_n;

  struct c_output *output;
  struct c_event_loop *loop;

  struct c_cursor_impl *impl;
};

struct c_cursor_impl {
  int  (*create)(struct c_cursor *cur, void **data, void *userdata);
  void (*free)(struct c_cursor *cur, void *data);
  int  (*move)(struct c_cursor *cur, double x, double y, void *data);
  int  (*write)(struct c_cursor *cur, void *buffer, size_t size, void *data);

  void  *data;
  void  *userdata;
};

struct c_cursor *c_cursor_init(struct c_output_manager *mgr, struct c_event_loop *loop);
int c_cursor_load(struct c_cursor *cur, const char *theme, size_t size);
int c_cursor_set_shape(struct c_cursor *cur, enum c_cursor_shape shape);
int c_cursor_move(struct c_cursor *cur, double x, double y);
void c_cursor_free(struct c_cursor *cur);

#endif
