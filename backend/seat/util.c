#include <linux/vt.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

#include "util/log.h"

int get_active_vt() {
  int fd = open("/dev/tty", O_RDONLY);
  if (fd < 0) {
    c_log_errno(C_LOG_ERROR, "failed to open /dev/tty");
    return -1;
  }

  struct vt_stat st;
  if (ioctl(fd, VT_GETSTATE, &st) < 0) {
    c_log_errno(C_LOG_ERROR, "ioctl failed");
    return -1;
  }

  close(fd);
  return st.v_active;
}

int terminal_set_keyboard(int fd, int enable) {
  if (ioctl(fd, KDSKBMODE, enable ? K_UNICODE : K_OFF) < 0) 
    return -1;
  
  return 0;
}

int terminal_set_graphics(int fd, int enable) {
  if (ioctl(fd, KDSETMODE, enable ? KD_GRAPHICS : KD_TEXT) < 0) 
    return -1;
  
  return 0;
}

int terminal_open() {
  int fd = open("/dev/tty0", O_RDWR | O_NOCTTY);
  if (fd < 0) {
    c_log_errno(C_LOG_ERROR, "failed to open /dev/tty0");
    goto error;
  }

  struct vt_mode mode = {0};
  mode.mode = VT_PROCESS;
  mode.relsig = SIGUSR1;
  mode.acqsig = SIGUSR2;

  if (ioctl(fd, VT_SETMODE, &mode) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to VT_SETMODE");
    goto error;
  }

  if (terminal_set_graphics(fd, 1) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to enable graphics");
    goto error;
  }

  if (terminal_set_keyboard(fd, 0) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to disable keyboard");
    goto error;
  }

  return fd;

error:
  close(fd);
  return -1;
}


int drm_ioctl(int fd, int req) {
  int ret;
  do {
      ret = ioctl(fd, req, NULL);
  } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
  return ret;
}
