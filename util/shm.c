#include <sys/mman.h>
#include <stdio.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "util/log.h"

static int n = 1;

static inline void gen_name(char *buf) {
  snprintf(buf, PATH_MAX, "/cuts-%d", n++);
}

int new_shm(int size, int *rfd, int *rwfd) {
  char name[PATH_MAX] = {0};
  gen_name(name);

  if ((*rwfd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0600)) < 0) {
    c_log_errno(C_LOG_ERROR, "shm_open(O_RDWR) failed");
    return -1;
  }

  if ((*rfd = shm_open(name, O_RDONLY, 0)) < 0) {
    c_log_errno(C_LOG_ERROR, "shm_open(O_RDONLY) failed");
    goto error_wfd;
  }

  shm_unlink(name);
  
  if (fchmod(*rwfd, 0) < 0) {
    c_log_errno(C_LOG_ERROR, "fchmod failed");
    goto error_fds;
  }

  if (ftruncate(*rwfd, size) < 0) {
    c_log_errno(C_LOG_ERROR, "ftruncate failed");
    goto error_fds;
  }

  return 0;

error_fds:
  close(*rfd);
  close(*rwfd);
  return -1;

error_wfd:
  shm_unlink(name);
  close(*rwfd);
  return -1;

}

void *new_shm_mmap(int size, int *rfd, int *rwfd, int flags) {
  if (new_shm(size, rfd, rwfd) < 0) return NULL;

  void *ptr = mmap(0, size, PROT_READ | PROT_WRITE, flags, *rwfd, 0);
  if (ptr == MAP_FAILED) {
    c_log_errno(C_LOG_ERROR, "mmap failed");
    close(*rfd);
    close(*rfd);
    return NULL;
  }

  return ptr;
}
