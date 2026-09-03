#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <dirent.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "util/helpers.h"
#include "cursor/cursor.h"
#include "cursor/xcursor.h"
#include "util/log.h"
#include "util/helpers.h"

extern struct c_cursor_impl hw_impl;

static int cursor_write(struct c_cursor *cur, void *buffer, size_t size) {
  return cur->impl->write(cur, buffer, size, cur->impl->data);
}

static int readline(FILE *f, char *buf, size_t size) {
  size_t i = 0;
  while ((buf[i++] = getc(f)) != '\n' && i < size)
    ;
  buf[i - 1] = 0;
  return i - 1;
}

static uint32_t read_cursor_dir(struct c_cursor *cur, const char *path, const char *name) {
  c_log(C_LOG_DEBUG, "reading %s/%s", path, name);

  uint32_t shapes = 0;
  char cursors_dir[256];

  char index_path[256];
  snprintf(index_path, sizeof(index_path), "%s/%s/index.theme", path, name);

  FILE *f = fopen(index_path, "r");
  if (!f) {
    c_log_errno(C_LOG_ERROR, "failed to open %s", index_path);
    return 0;
  }

  char line[1024];
  int len = readline(f, line, sizeof(line));

  if (STREQ(line, "[Icon Theme]")) {
    len = readline(f, line, sizeof(line));
    if (len > 9) {
      line[8] = 0;
      char *key = line;
      char *value = line + 9;

      if (STREQ(key, "Inherits")) {
        c_log(C_LOG_DEBUG, "%s/%s inherits from %s/%s", path, name, path, value);
        shapes = read_cursor_dir(cur, path, value);
        if (!shapes) goto get_shapes;

        fclose(f);
        return shapes;
      }
    }
  }

  c_log(C_LOG_DEBUG, "%s/%s doesn't inherit", path, name);

 
get_shapes:
  fclose(f);

  snprintf(cursors_dir, sizeof(cursors_dir), "%s/%s/cursors", path, name);

  DIR *dir = opendir(cursors_dir);
  if (!dir) return 0;

  struct dirent *d;
  while ((d = readdir(dir))) {
    for (int i = 0; i < C_CURSOR_ALL_RESIZE; i++) {
      if (STREQ(d->d_name, str_cursor_shape(i)))
        shapes |= ENUM_FLAG(i);
    }
  }

  snprintf(cur->theme, sizeof(cur->theme), "%s", name);
  closedir(dir);
  return shapes;
}

static uint32_t read_dir(struct c_cursor *cur, const char *path, const char *target) {
  c_log(C_LOG_DEBUG, "reading %s to find '%s'", path, target);
  uint32_t shapes = 0;

  DIR *dir = opendir(path);
  if (!dir) return 0;

  struct dirent *d;
  while ((d = readdir(dir))) {
    if (STREQ(d->d_name, target)) {
      shapes = read_cursor_dir(cur, path, target);
      if (shapes) break;
    }
  }

  closedir(dir);
  return shapes;
}

static int load_cursor_theme(struct c_cursor *cur, const char *theme) {
  const char *sys_icons_dir = "/usr/share/icons";
  char local_icons_dir[256];

  const char *homepath = getenv("HOME");
  if (!homepath) {
    c_log(C_LOG_ERROR, "couldn't get 'HOME' env var");
    return 1;
  }
  snprintf(local_icons_dir, sizeof(local_icons_dir), "%s/.local/share/icons", homepath);

  uint32_t shapes;
  if ((shapes = read_dir(cur, local_icons_dir, theme))) { 
    cur->dir = _CUR_LOCAL_DIR;
    goto out; 
  }
  c_log(C_LOG_DEBUG, "'%s' not found in %s", theme, local_icons_dir);

  if ((shapes = read_dir(cur, sys_icons_dir, theme))) {
    cur->dir = _CUR_SYS_DIR;
    goto out;
  }
  c_log(C_LOG_DEBUG, "'%s' not found in %s", theme, sys_icons_dir);

  c_log(C_LOG_INFO, "couldn't find '%s' cursor theme. loading default");
  shapes = read_cursor_dir(cur, sys_icons_dir, "default");
  if (!shapes)
    return 1;
  cur->dir = _CUR_SYS_DIR;

out:
  cur->shapes = shapes;
  return 0;
}

int c_cursor_load(struct c_cursor *cur, const char *theme, size_t size) {
  cur->size = size;

  if (cur->impl->create(cur, &cur->impl->data, cur->impl->userdata)) {
    c_log(C_LOG_ERROR, "failed to create cursor");
    goto error;
  }

  if (load_cursor_theme(cur, theme)) {
    c_log(C_LOG_ERROR, "failed to load cursor theme");
    goto load_error;
  }

  c_log(C_LOG_INFO, "cursor: %s. size: %d", cur->theme, cur->size);
  for (int i = 0; i < C_CURSOR_ALL_RESIZE; i++) {
    if (cur->shapes & ENUM_FLAG(i))
      c_log(C_LOG_INFO, "  shape %s", str_cursor_shape(i));
  }

  return 0;

load_error:
  cur->impl->free(cur, cur->impl->userdata);
  cur->impl->data = NULL;

error:
  return 1;
}

static int set_timer(int fd, uint32_t wait_ms) {
  struct itimerspec t;
  t.it_value.tv_sec    = wait_ms / 1000;
  t.it_value.tv_nsec    = (wait_ms % 1000) * 1000000LL;
  t.it_interval.tv_sec = t.it_value.tv_sec;
  t.it_interval.tv_nsec = t.it_value.tv_nsec;
  return timerfd_settime(fd, 0, &t, NULL);
}

static void unset_timer(int fd) {
  struct itimerspec t;
  t.it_value.tv_sec     = 0;
  t.it_value.tv_nsec    = 0;
  t.it_interval.tv_sec  = 0;
  t.it_interval.tv_nsec = 0;
  timerfd_settime(fd, 0, &t, NULL);
}

C_EVENT_CALLBACK on_frame_swap(struct c_event_loop *loop, int fd, void *userdata) {
  struct c_cursor *cur = userdata;
  cur->frame = cur->frame->next;
  
  uint64_t expiry_c;
  if (read(fd, &expiry_c, sizeof(expiry_c)) == -1) {
    c_log_errno(C_LOG_ERROR, "failed to read");
  }

  if (cursor_write(cur, cur->frame->image, cur->max_size * cur->max_size * 4)) {
    c_log(C_LOG_ERROR, "failed to write frame");
  }
  return C_EVENT_OK;
}


int c_cursor_set_shape(struct c_cursor *cur, enum c_cursor_shape shape) {
  if (!(ENUM_FLAG(shape) & cur->shapes)) return 0;

  cur->shape = shape;

  if (cur->frame) {
    unset_timer(cur->timer_fd);
    c_xcursor_unload_image(cur);
  }

  if (c_xcursor_load_image(cur)) return 1;

  if (cursor_write(cur, cur->frame->image, cur->max_size * cur->max_size * 4)) goto error;

  if (cur->frames_n > 1) {
    if (!cur->timer_fd) {
      int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
      if (timer_fd == -1) {
        c_log_errno(C_LOG_ERROR, "failed to create timerfd");
        goto error;
      }
      cur->timer_fd = timer_fd;
      c_event_loop_add(cur->loop, cur->timer_fd, on_frame_swap, cur);
    }

    if (set_timer(cur->timer_fd, cur->frame->wait_ms)) {
      c_log_errno(C_LOG_ERROR, "failed to set timer");
      goto error;
    }
  }

  return 0;

error:
  c_xcursor_unload_image(cur);
  return 1;
}

int c_cursor_move(struct c_cursor *cur, double x, double y) {
  return cur->impl->move(cur, x, y, cur->impl->data);
}

struct c_cursor *c_cursor_init(struct c_output_manager *mgr, struct c_event_loop *loop) {
  struct c_cursor *cur = calloc(1, sizeof(*cur));
  if (!cur) {
    c_log_errno(C_LOG_ERROR, "failed to allocate c_cursor");
    return NULL;
  }

  cur->output = c_list_get(mgr->outputs, 0);
  assert(cur->output);

  cur->loop = loop;

	char *cursor_type = getenv("CUTS_HARDWARE_CURSOR");
  if (cursor_type) {
    if (*cursor_type == '1') goto hw;
    else {
      c_log(C_LOG_WARNING, "software cursor isn't implemented yet. using hardware cursor");
      goto hw;
    }

  } else {
hw:
    cur->impl = &hw_impl;
    cur->impl->userdata = mgr;
  }

  return cur;
}

void c_cursor_free(struct c_cursor *cur) {
  if (cur->frame) c_xcursor_unload_image(cur);
  if (cur->impl->data) cur->impl->free(cur, cur->impl->data);
  free(cur);
}
