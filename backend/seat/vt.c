#include <linux/vt.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

#include "util/log.h"

int vt_set_keyboard(int fd, int enable) {
  if (ioctl(fd, KDSKBMODE, enable ? K_UNICODE : K_OFF) < 0) 
    return -1;
  
  return 0;
}

int vt_set_graphics(int fd, int enable) {
  if (ioctl(fd, KDSETMODE, enable ? KD_GRAPHICS : KD_TEXT) < 0) 
    return -1;
  
  return 0;
}

int vt_open(int vt) {
  char path[16];
  snprintf(path, sizeof(path), "/dev/tty%d", vt);
  int fd = open(path, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    c_log_errno(C_LOG_ERROR, "failed to open %s", path);
    return -1;
  }

  return fd;
}

int vt_disable(int fd) {
  struct vt_mode mode = {0};
  mode.mode = VT_PROCESS;
  mode.relsig = SIGUSR1;
  mode.acqsig = SIGUSR2;

  if (ioctl(fd, VT_SETMODE, &mode) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to VT_SETMODE");
    return -1;
  }

  if (vt_set_graphics(fd, 1) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to enable graphics");
    return -1;
  }

  if (vt_set_keyboard(fd, 0) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to disable keyboard");
    return -1;
  }

  return 0;
}

int vt_enable(int fd) {
  if (vt_set_graphics(fd, 0) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to disable graphics");
    return -1;
  }

  if (vt_set_keyboard(fd, 1) < 0) {
    c_log_errno(C_LOG_ERROR, "failed to enable keyboard");
    return -1;
  }

  return 0;
}

int vt_get_current_num() {
  int fd = vt_open(0);
  if (fd < 0) return -1;

  struct vt_stat st;
  if (ioctl(fd, VT_GETSTATE, &st) < 0) {
    c_log_errno(C_LOG_ERROR, "ioctl(VT_GETSTATE) failed");
    close(fd);
    return -1;
  }

  close(fd);
  return st.v_active;
}

int vt_release(int fd) {
  if (ioctl(fd, VT_RELDISP, 1) < 0) {
    return -1;      
  }
  return 0;
}

int vt_acquire(int fd) {
  if (ioctl(fd, VT_RELDISP, VT_ACKACQ) < 0) {
    return -1;      
  }
  return 0;
}
