#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <grp.h>
#include <sys/stat.h>

#include "util/log.h"
#include "util/helpers.h"
#include "util/signal.h"
#include "util/event_loop.h"
#include "util/list.h"

#include "sock.h"
#include "vt.h"

#define MAX_DEVICES 256
static uint32_t __dev_id = 1;

struct seat_device {
  int fd;
  uint32_t id;
  int is_drm;
};

struct seat_client {
  int fd;
  int vt;
  int vt_fd;
  size_t n_devices;
  c_list *devices;
};

struct {
  int fd;
  int cur_vt;
  struct c_event_loop *event_loop;
  c_list *clients;
  c_list *signals;
} serv = {0};

static void cleanup(int code) {
  if (serv.fd) unlink(C_SEAT_SOCKET_PATH);
  if (serv.clients) {
    struct seat_client *c;
    c_list_for_each(serv.clients, c) c_list_destroy(c->devices);
    c_list_destroy(serv.clients);
  }
  close(serv.fd);
  exit(code);
}

static void send_ack(int client_fd) {
  struct c_seat_msg_params params = {0};
  params.header.op = C_SEAT_MSG_ACK;
  assert(seat_send(client_fd, &params) >= 0);
}

static void seat_enable(int client_fd, int vt) {
  struct c_seat_msg_params send_params = {0};
  send_params.header.op = C_SEAT_MSG_ENABLE_SEAT;

  serv.cur_vt = vt;
  seat_send(client_fd, &send_params);

}

static void seat_disable(int client_fd) {
  struct c_seat_msg_params send_params = {0};
  send_params.header.op = C_SEAT_MSG_DISABLE_SEAT;

  seat_send(client_fd, &send_params);

}


#define __DRM_SET_MASTER  0x641e
#define __DRM_DROP_MASTER 0x641f

static int drm_ioctl(int fd, int req) {
  int ret;
  do {
      ret = ioctl(fd, req, NULL);
  } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
  return ret;
}

static inline int drm_set_master(int fd) {
  return drm_ioctl(fd, __DRM_SET_MASTER);
}

static inline int drm_drop_master(int fd) {
  return drm_ioctl(fd, __DRM_DROP_MASTER);
}

static int drm_activate_master(int vt) {
  struct seat_client *c;
  c_list_for_each(serv.clients, c) {
    struct seat_device *d;
    c_list_for_each(c->devices, d) {
      if (d->is_drm) {
        int req = c->vt == vt ? drm_set_master(d->fd) : drm_drop_master(d->fd);
        if (req < 0) return -1;
      }
    }
  }
  return 0;
}

static struct seat_client *get_client_from_fd(int fd) {
  struct seat_client *c;
  c_list_for_each(serv.clients, c) {
    if (fd == c->fd) return c;
  }
  return NULL;
}

static void seat_close_device(int client_fd, struct c_seat_msg_params *params) {
  struct seat_client *client = get_client_from_fd(client_fd);
  if (!client) {
      seat_send_error(client_fd, "no clients registered with fd %d", client_fd);
      return;
  }

  uint32_t id = *(uint32_t *)params->body;
  struct seat_device *d;
  c_list_for_each(client->devices, d) {
    if (d->id == id) {
      close(d->fd);
      client->n_devices--;
      c_list_remove_ptr(&client->devices, d);
      send_ack(client_fd);
      return;
    }
  }

  seat_send_error(client_fd, "invalid fd");

}

static void seat_open_device(int client_fd, struct c_seat_msg_params *params) {
  struct seat_client *client = get_client_from_fd(client_fd);
  if (!client) {
    seat_send_error(client_fd, "no clients registered with fd %d", client_fd);
    return;
  }

  if (client->n_devices > MAX_DEVICES) {
    seat_send_error(client_fd, "can't open more than %d devices", MAX_DEVICES);
    return;
  }

  const char *path = params->body;
  int is_drm = 0;
  if (starts_with(path, "/dev/dri/card")) {
    is_drm = 1;
  } else if (!starts_with(path, "/dev/input/event")) {
    seat_send_error(client_fd, "can't open %s. not allowed", path);
    return;
  }

  int fd = open(path, O_RDWR | O_NOCTTY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0) {
    seat_send_error(client_fd, "failed to open %s: %s", path, strerror(errno));
    return;
  }

  if (drm_activate_master(client->vt) < 0) {
    seat_send_error(client_fd, "drm_activate_master(%d) failed: %s", client->vt, strerror(errno));
    return;
  }

  struct c_seat_msg_params send_params = {0};
  send_params.fd = fd;
  send_params.header.op = C_SEAT_MSG_ACK;
  *(uint32_t *)send_params.body = __dev_id;
  send_params.header.body_size = sizeof(__dev_id);

  struct seat_device d = {
    .fd = fd,
    .id = __dev_id++,
    .is_drm = is_drm,
  };

  c_list_push(client->devices, &d, sizeof(d));
  client->n_devices++;
  seat_send(client_fd, &send_params);
}

static void vt_ack(struct seat_client *client, int acquire) {
  if (acquire) {
    seat_enable(client->fd, client->vt);
    drm_activate_master(client->vt);
  } else 
    seat_disable(client->fd); 

  int ret = acquire ? vt_acquire(client->vt_fd) : vt_release(client->vt_fd);
  if (ret < 0) {
    c_log_errno(C_LOG_ERROR, "failed to %s vt %d", acquire ? "acquire" : "release", client->vt);
    seat_send_error(client->fd, "failed to %s vt %d", acquire ? "acquire" : "release", client->vt);
    return;
  }

}

static void on_vt_release(int sig, void *userdata) {
  int cur_vt = vt_get_current_num();
  c_log(C_LOG_DEBUG, "on_vt_release(%d)", cur_vt);

  struct seat_client *client;
  c_list_for_each(serv.clients, client) {
    if (client->vt == cur_vt) {
      seat_disable(client->fd);

      if (vt_release(client->vt_fd) < 0) {
        c_log_errno(C_LOG_ERROR, "failed to release vt %d",  client->vt);
        seat_send_error(client->fd, "failed to release vt %d", client->vt);
      }

    }
  }
}

static void on_vt_activate(int sig, void *userdata) {
  int cur_vt = vt_get_current_num();
  c_log(C_LOG_DEBUG, "on_vt_activate(%d)", cur_vt);

  struct seat_client *client;
  c_list_for_each(serv.clients, client) {
    if (client->vt == cur_vt) {
      seat_enable(client->fd, client->vt);
      drm_activate_master(client->vt);

      if (vt_acquire(client->vt_fd) < 0) {
        c_log_errno(C_LOG_ERROR, "failed to acquire vt %d",  client->vt);
        seat_send_error(client->fd, "failed to acquire vt %d", client->vt);
      }

    }
  }
}

static void seat_open_seat(int client_fd, struct c_seat_msg_params *params) {
  int vt = vt_get_current_num();
  c_log(C_LOG_DEBUG, "current vt number: %d", vt);

  struct seat_client *c;
  c_list_for_each(serv.clients, c) {
    if (c->vt == vt) {
      seat_send_error(client_fd, "there is already a client that uses vt %d", vt);
      return;
    }
  }


  int vt_fd = vt_open(vt);
  if (!vt_fd) {
    seat_send_error(client_fd, "failed to open vt%d", vt);
    return;
  }

  if (vt_disable(vt_fd) < 0) {
    seat_send_error(client_fd, "failed to disable vt%d", vt);
    return;
  }

  struct seat_client new_client = {
    .fd = client_fd,
    .vt = vt,
    .vt_fd = vt_fd,
    .devices = c_list_new(),
  };

  // close(vt_fd);

  c_list_push(serv.clients, &new_client, sizeof(new_client));

  seat_enable(client_fd, vt);
};

static void seat_close_seat(int client_fd) {
  struct seat_client *client = get_client_from_fd(client_fd);
  if (!client) {
    seat_send_error(client_fd, "no clients registered with fd %d", client_fd);
    return;
  }

  vt_enable(client->vt_fd);
  close(client->vt_fd);

  struct seat_device *d;
  c_list_for_each(client->devices, d)
    if (d->is_drm) drm_drop_master(d->fd);

  c_list_destroy(client->devices);
  c_list_remove_ptr(&serv.clients, client);

}

C_EVENT_CALLBACK seat_client_callback(struct c_event_loop *loop, int fd, void *userdata) {
  struct c_seat_msg_params params;
  int n = seat_recv(fd, &params);
  if (n <= 0) {
    seat_close_seat(fd);
    return C_EVENT_ERROR_FD_GONE;
  }

  switch (params.header.op) {
    case C_SEAT_MSG_OPEN_SEAT:
      c_log(C_LOG_DEBUG, "client#%d seat.open_seat", fd);
      seat_open_seat(fd, &params);
      break;

    case C_SEAT_MSG_CLOSE_SEAT:
      c_log(C_LOG_DEBUG, "client#%d seat.close_seat", fd);
      seat_close_seat(fd);
      break;

    case C_SEAT_MSG_OPEN_DEVICE:
      c_log(C_LOG_DEBUG, "client#%d seat.open_device", fd);
      seat_open_device(fd, &params);
      break;

    case C_SEAT_MSG_CLOSE_DEVICE:
      c_log(C_LOG_DEBUG, "client#%d seat.close_device", fd);
      seat_close_device(fd, &params);
      break;
  }

  return C_EVENT_OK;

}


C_EVENT_CALLBACK seat_server_callback(struct c_event_loop *loop, int fd, void *userdata) {
  int client_fd = accept(fd, NULL, NULL);

  if (set_nonblocking(client_fd) == -1) {
    c_log(C_LOG_WARNING, "failed to set client fd to non-blocking");
  }

  if (c_event_loop_add(loop, client_fd, seat_client_callback, NULL) == -1) {
    c_log(C_LOG_ERROR, "c_event_loop_add failed");
    return C_EVENT_ERROR_FATAL;
  }

  return C_EVENT_OK;
} 

int main() {
  signal(SIGINT, cleanup);

  struct sockaddr_un sock_addr = {0};
  c_log_set_level(C_LOG_ERROR | C_LOG_DEBUG | C_LOG_INFO | C_LOG_WARNING);
                                                                  
  serv.clients = c_list_new();

  serv.fd = socket(AF_UNIX, C_SEAT_SOCKET_FLAGS, 0);
  if (serv.fd == -1) {
    c_log_errno(C_LOG_ERROR, "socket failed");
    return 1;
  }

  sock_addr.sun_family = AF_UNIX;
  snprintf(sock_addr.sun_path, sizeof(sock_addr.sun_path), C_SEAT_SOCKET_PATH);

  if (bind(serv.fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) == -1) {
    c_log_errno(C_LOG_ERROR, "bind failed");
    goto error;
  }
  
  if (listen(serv.fd, 16) == -1) {
    c_log_errno(C_LOG_ERROR, "listen failed");
    goto error_unlink;
  }

  serv.event_loop = c_event_loop_init();
  if (!serv.event_loop) {
    c_log(C_LOG_ERROR, "c_event_loop_init failed");
    goto error_unlink;
  }

  struct group *grp = getgrnam("cuts");
  if (!grp) {
    c_log(C_LOG_ERROR, "'cuts' groups doesn't exist");
    goto error_unlink;
  }

  if (chmod(C_SEAT_SOCKET_PATH, 0770) == -1) {
    c_log_errno(C_LOG_ERROR, "socket chmod failed");
    goto error_unlink;
  }
  if (chown(C_SEAT_SOCKET_PATH, getuid(), grp->gr_gid) == -1) {
    c_log_errno(C_LOG_ERROR, "socket chown failed");
    goto error_unlink;
  }

  c_signal_handler_add(SIGUSR1, on_vt_release, NULL);
  c_signal_handler_add(SIGUSR2, on_vt_activate, NULL);

  c_event_loop_add(serv.event_loop, serv.fd, seat_server_callback, NULL);
  c_event_loop_run(serv.event_loop);

  cleanup(0);

error_unlink:
  cleanup(1);

error:
  close(serv.fd);
  return 1;
}
