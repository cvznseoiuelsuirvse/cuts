#ifndef CUTS_OUTPUT_H
#define CUTS_OUTPUT_H

#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <gbm.h>

#include "util/list.h"
#include "util/event_loop.h"
#include "wayland/display.h"
#include "seat/session/session.h"

#define c_output_backbuffer(output) (output)->swapchain.buffers[(output)->swapchain.front ^ 1]
#define c_output_frontbuffer(output) (output)->swapchain.buffers[(output)->swapchain.front]

struct c_output_mode {
	uint32_t width, height;
	double refresh_rate;
	int preferred;

  struct {
    drmModeModeInfo info;
    uint32_t blob_id;
  } drm;
};

struct c_output_drm_object {
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;
	uint32_t id;
};

struct c_output {
	char name[16];
	char manufacturer_name[13];

  char make[4];
  uint16_t model;
  uint32_t serial;

	uint32_t mm_width, mm_height;
	int subpixel;

  uint32_t x, y;

	c_list *modes;
  struct c_output_mode *current_mode;

  struct c_output_drm_object connector;
  struct c_output_drm_object crtc;
  struct c_output_drm_object plane;

	drmModeCrtcPtr orig_crtc;

	int waiting_for_flip;
	int need_redraw;

  c_list *active_surfaces;

	struct {
		int front;
		struct c_framebuffer *buffers[2];
	} swapchain;

  struct c_drm_sync_object *timeline;

};

struct c_output_manager;
typedef int (*on_redraw_handler)(struct c_output_manager *mgr, struct c_output *output, void *userdata);

struct c_output_manager {
  int drm_fd;
  struct gbm_device *gbm_device;
  struct c_renderer *renderer;

  void *on_redraw_userdata;
  on_redraw_handler on_redraw;

  c_list *outputs;
};

struct c_output_manager *c_output_manager_init(struct c_session *session, struct c_event_loop *loop, struct c_wl_display *display);
void c_output_manager_free(struct c_output_manager *mgr);
void c_output_set_mode(struct c_output_manager *mgr, struct c_output *output, struct c_output_mode *mode);
int c_output_damage(struct c_output_manager *mgr, struct c_output *output);
void c_output_register_on_redraw(struct c_output_manager *mgr, on_redraw_handler handler, void *userdata);

#endif
