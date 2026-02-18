#include "backend/drm.h"
#include "util/helpers.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *user_data) {
  struct c_drm_connector *conn = user_data;
  struct c_drm_dumb_framebuffer *fb = conn->back;

  draw_rect(fb, 0, 0, fb->width, fb->height, (uint32_t)(tv_usec & 0xffffffff));

  if (drmModePageFlip(fd, conn->crtc_id, fb->id, DRM_MODE_PAGE_FLIP_EVENT, conn) < 0) 
    perror("c_drm_backend_page_flip");

  conn->back = conn->front;
  conn->front = fb;
}

int c_drm_backend_page_flip(struct c_drm_backend *backend) {
  drmEventContext ctx = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler,
  };

  if (drmHandleEvent(backend->fd, &ctx) < 0) {
    perror("drmHandleEvent");
    return -1;
  }
  return 0;
}



static uint32_t get_crtc_id(int fd, drmModeResPtr res, drmModeConnectorPtr conn, uint32_t *taken_crtcs) {
  for (int enc_n = 0; enc_n < conn->count_encoders; enc_n++) {
    drmModeEncoderPtr encoder = drmModeGetEncoder(fd, res->encoders[enc_n]);
    if (!encoder) continue;

    for (int crtc_n = 0; crtc_n < res->count_crtcs; crtc_n++) {
      uint32_t bit = 1 << crtc_n;
      if ((encoder->possible_crtcs & bit) == 0) continue;
      if (*taken_crtcs & bit) continue;

      drmModeFreeEncoder(encoder);
      *taken_crtcs |= bit;

      return res->crtcs[crtc_n];
    }

    drmModeFreeEncoder(encoder);
  }

  return 0;
}

int c_drm_connector_set_crtc(int fd, struct c_drm_connector *conn, struct c_drm_dumb_framebuffer *fb) {
  if (drmModeSetCrtc(fd, conn->crtc_id, fb->id, 0, 0, &conn->id, 1, conn->mode) != 0) {
    perror("drmModeSetCrtc");
    return 1;
  }
  return 0;
}

static struct c_drm_dumb_framebuffer *c_drm_dumb_framebuffer_create(int fd, uint32_t width, uint32_t height, uint32_t format) {
  struct c_drm_dumb_framebuffer *fb = calloc(1, sizeof(struct c_drm_dumb_framebuffer));
  if (!fb) return NULL;

  fb->width = width;
  fb->height = height;

  if (drmModeCreateDumbBuffer(fd,
                          fb->width, fb->height, 32, 0, 
                          &fb->handle, &fb->stride,
                          &fb->size) != 0) {
    perror("drmModeCreateDumbBuffer");
    return NULL;
  }

  uint32_t handles[4] = {fb->handle};
  uint32_t strides[4] = {fb->stride};
  uint32_t offsets[4] = {0};

  if (drmModeAddFB2(fd, fb->width, fb->height,
    format, handles, strides, offsets, &fb->id, 0) != 0) {
    perror("drmModeAddFB2");
    return NULL;
  };

  if (drmModeMapDumbBuffer(fd , fb->handle, &fb->offset) != 0) {
    perror("drmModeMapDumbBuffer");
    return NULL;
  };

  uint8_t *buffer = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, fb->offset);
  if (buffer == MAP_FAILED) {
    perror("mmap");
    return NULL;
  }

  fb->buffer = buffer;
  memset(fb->buffer, 0, fb->size);
  return fb;

};

static void c_drm_dumb_framebuffer_free(int fd, struct c_drm_dumb_framebuffer *fb) {
    drmModeRmFB(fd, fb->id);
    drmModeDestroyDumbBuffer(fd, fb->handle);
    munmap(fb->buffer, fb->size);
    free(fb);
}

static int c_drm_backend_get_connectors(struct c_drm_backend *backend) {
  drmModeConnectorPtr connector;
  uint32_t taken_crtcs = 0;
  for (int i = 0; i < backend->res->count_connectors; i++) {
    connector = drmModeGetConnector(backend->fd, backend->res->connectors[i]);
    if (!connector) continue;
    if (connector->connection == DRM_MODE_CONNECTED) {
      size_t idx = backend->connectors_n;
      struct c_drm_connector *c_connector = &backend->connectors[idx];

      c_connector->conn = connector;
      c_connector->id = connector->connector_id;

      uint32_t crtc_id = get_crtc_id(backend->fd, backend->res, connector, &taken_crtcs);
      c_connector->crtc_id = crtc_id;
      c_connector->orig_crtc = drmModeGetCrtc(backend->fd, crtc_id);


      for (int m = 0; m < connector->count_modes; m++) {
        drmModeModeInfoPtr mode = &connector->modes[m];
        if (mode->type & DRM_MODE_TYPE_PREFERRED) {
          c_connector->mode = mode;
          break;
        }
      }

      if (!c_connector->mode) {
        c_connector->mode = &connector->modes[0];
      }

      c_connector->front = c_drm_dumb_framebuffer_create(backend->fd,
                               c_connector->mode->hdisplay, c_connector->mode->vdisplay, DRM_FORMAT_XRGB8888);
      if (!c_connector->front) return -1;

      c_connector->back = c_drm_dumb_framebuffer_create(backend->fd,
                               c_connector->mode->hdisplay, c_connector->mode->vdisplay, DRM_FORMAT_XRGB8888);
      if (!c_connector->back) return -1;

      c_drm_connector_set_crtc(backend->fd, c_connector, c_connector->front);

      if (drmModePageFlip(backend->fd, c_connector->crtc_id, c_connector->front->id, DRM_MODE_PAGE_FLIP_EVENT, c_connector) == -1) {
        perror("drmModePageFlip"); 
        return -1;
      };

      backend->connectors_n++;

    } else 
      drmModeFreeConnector(connector);
  }

  return backend->connectors_n == 0 ? -1 : 0;
}

void c_drm_backend_free(struct c_drm_backend *backend) {
  if (backend->res) drmModeFreeResources(backend->res);

  for (size_t i = 0; i < backend->connectors_n; i++) {
    struct c_drm_connector conn = backend->connectors[i];
    if (conn.back) c_drm_dumb_framebuffer_free(backend->fd, conn.back);
    if (conn.front) c_drm_dumb_framebuffer_free(backend->fd, conn.front);

    drmModeSetCrtc(backend->fd, 
                   conn.orig_crtc->crtc_id, conn.orig_crtc->buffer_id, 
                   0, 0, &conn.id, 1, &conn.orig_crtc->mode);
    drmModeFreeCrtc(conn.orig_crtc);
  }

  close(backend->fd);
  free(backend);
}

struct c_drm_backend *c_drm_backend_init() {
  struct c_drm_backend *backend = calloc(1, sizeof(struct c_drm_backend));
  if (!backend) return NULL;

  int fd = open("/dev/dri/card1", O_RDWR | O_NONBLOCK); // FIXME: pick card properly
  if (fd < 0) goto error;
  backend->fd = fd;

  drmModeResPtr res = drmModeGetResources(fd);
  if (!res) goto error;
  backend->res = res;

  if (c_drm_backend_get_connectors(backend) == -1) goto error;
  return backend;

error:
  c_drm_backend_free(backend);
  return NULL;
}
