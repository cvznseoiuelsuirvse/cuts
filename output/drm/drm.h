#ifndef CUTS_BACKEND_DRM_H
#define CUTS_BACKEND_DRM_H

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <gbm.h>

#include "util/list.h"

c_list *c_drm_get_connectors(int drm_fd);
void c_drm_page_flip(int drm_fd);

#endif
