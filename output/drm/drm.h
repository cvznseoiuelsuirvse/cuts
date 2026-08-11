#ifndef CUTS_BACKEND_DRM_H
#define CUTS_BACKEND_DRM_H

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <gbm.h>

#include "util/list.h"
#include "output/output.h"

void c_drm_free_output(int drm_fd, struct c_output *output);
c_list *c_drm_get_outputs(int drm_fd);
int c_drm_atomic_commit(int drm_fd, struct c_output *output,
                        struct c_output_mode *mode, int flags, void *userdata,
                        int in_fence_fd);

#endif
