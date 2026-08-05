#ifndef CUTS_BACKEND_DRM_UTIL_H
#define CUTS_BACKEND_DRM_UTIL_H

#include <stdint.h>
#include <xf86drmMode.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

int drm_format_num_planes(uint32_t format);
double drm_refresh_rate(drmModeModeInfo *mode);
const char *drm_connector_str(uint32_t connector_type);
void drm_print_mode(drmModeModeInfo *mode);

enum wl_shm_format_enum drm_fmt_to_wl_shm_fmt(uint32_t format);
uint32_t wl_shm_fmt_to_drm_fmt(enum wl_shm_format_enum format);
int drm_fmt_to_gl_fmt(uint32_t drm_format, GLenum *internal_format,
                        GLenum *format, GLenum *type);

#endif
