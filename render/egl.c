#define _GNU_SOURCE
#include <stdlib.h>
#include <drm_fourcc.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <drm_mode.h>
#include <xf86drm.h>
#include <GL/gl.h>

#include "render/egl.h"
#include "util/log.h"
#include "util/helpers.h"

#define load_proc(name) egl->proc.name = (void *)eglGetProcAddress(#name);

static int has_ext(const char *ext, const char *exts) {
  if (!exts) return 0;

  size_t exts_len = strlen(exts);
  char exts_cpy[exts_len + 1];
  snprintf(exts_cpy, sizeof(exts_cpy), "%s", exts);

  char *token = strtok(exts_cpy, " ");
  while (token != NULL) {
    if (STREQ(token, ext)) return 1;
    token = strtok(NULL, " ");
  }

  return 0;

}


static int load_gl_exts(struct c_egl *egl) {
  const GLubyte *exts = glGetString(GL_EXTENSIONS);

  if (has_ext("GL_OES_EGL_image_external", (const char *)exts)) {
    load_proc(glEGLImageTargetTexture2DOES);
  } else {
    c_log(C_LOG_ERROR, "GL_OES_EGL_image_external not supported");
    return -1;
  }

  return 0;

}

static int load_egl_exts(struct c_egl *egl) {
  const char *exts_display = eglQueryString(egl->display, EGL_EXTENSIONS);


  if (has_ext("EGL_KHR_image_base", exts_display)) {
    load_proc(eglCreateImageKHR);
    load_proc(eglDestroyImageKHR);
  } else {
    c_log(C_LOG_ERROR, "EGL_KHR_image_base not supported");
    return -1;
  }

  if (!has_ext("EGL_EXT_image_dma_buf_import", exts_display)) {
    c_log(C_LOG_ERROR, "EGL_EXT_image_dma_buf_import not supported");
    return -1;
  }

  if (has_ext("EGL_EXT_image_dma_buf_import_modifiers", exts_display)) {
    egl->ext_support.EXT_image_dma_buf_import_modifiers = 1;
    load_proc(eglQueryDmaBufFormatsEXT);
    load_proc(eglQueryDmaBufModifiersEXT);
  } else {    
    c_log(C_LOG_ERROR, "EGL_EXT_image_dma_buf_import_modifiers not supported");
    return -1;
  }

  return 0;

}

static int c_egl_query_modifiers(struct c_egl *egl, EGLint format, 
                                 EGLuint64KHR **modifiers, EGLBoolean **external_only) {
  EGLint num_modifiers;
  if (egl->proc.eglQueryDmaBufModifiersEXT(egl->display, format, 0, NULL, NULL, &num_modifiers) != EGL_TRUE) {
    c_log(C_LOG_ERROR, "eglQueryDmaBufModifiersEXT(NULL) failed");
    return -1;
  }

  *modifiers = calloc(num_modifiers, sizeof(**modifiers));
  if (!modifiers) {
    c_log(C_LOG_ERROR, "calloc modifiers failed");
    return -1;
  }
  *external_only = calloc(num_modifiers, sizeof(**external_only));
  if (!external_only) {
    free(modifiers);
    c_log(C_LOG_ERROR, "calloc external_only failed");
    return -1;
  }

  if (egl->proc.eglQueryDmaBufModifiersEXT(egl->display, format, num_modifiers, 
                                           *modifiers, *external_only, &num_modifiers) != EGL_TRUE) {
    free(modifiers);
    free(external_only);
    c_log(C_LOG_ERROR, "eglQueryDmaBufModifiersEXT failed");
    return -1;
  }

  return num_modifiers;
}

static int c_egl_query_formats(struct c_egl *egl, EGLint **formats) {
  EGLint num_formats;
  if (egl->proc.eglQueryDmaBufFormatsEXT(egl->display, 0, NULL, &num_formats) != EGL_TRUE) {
    c_log(C_LOG_ERROR, "eglQueryDmaBufFormatsEXT(NULL) failed");
    return -1;
  }

  *formats = calloc(num_formats, sizeof(**formats));
  if (!formats) {
    c_log(C_LOG_ERROR, "calloc failed");
    return -1;
  }

  if (egl->proc.eglQueryDmaBufFormatsEXT(egl->display, num_formats, *formats, &num_formats) != EGL_TRUE) {
    free(formats);
    c_log(C_LOG_ERROR, "eglQueryDmaBufFormatsEXT failed");
    return -1;
  }

  return num_formats;
}

int c_egl_get_format_table_fd(struct c_egl *egl) {
  int fd = memfd_create("format_table", 0);
  if (!fd) {
    c_log(C_LOG_ERROR, "memfd_create failed");
    return -1;
  }

  for (size_t i = 0; i < egl->format_table.pairs_n; i++) {
    struct c_format_table_pair pair = egl->format_table.pairs[i];
    write(fd, &pair, sizeof(pair));
  }

  return fd;
}

static int get_format_table(struct c_egl *egl) {
  EGLint *formats = NULL;
  EGLint num_formats = c_egl_query_formats(egl, &formats);
  if (num_formats == -1) return -1;


  for (int f = 0; f < num_formats; f++) {
    EGLint format = formats[f];    
    EGLuint64KHR *modifiers = NULL;
    EGLBoolean *external_only = NULL;

    EGLint num_modifiers = c_egl_query_modifiers(egl, format, &modifiers, &external_only);
    if (num_modifiers < 0) continue;
    for (int m = 0; m < num_modifiers; m++, egl->format_table.pairs_n++);

    free(modifiers);
    free(external_only);
  }

  egl->format_table.pairs = calloc(egl->format_table.pairs_n, sizeof(struct c_format_table_pair));
  if (!egl->format_table.pairs) {
    c_log(C_LOG_ERROR, "calloc failed");
    return -1;
  }

  int i = 0;
  for (int f = 0; f < num_formats; f++) {
    EGLint format = formats[f];    

    EGLuint64KHR *modifiers = NULL;
    EGLBoolean *external_only = NULL;

    c_log(C_LOG_INFO, "format=%s", drmGetFormatName(format));
    EGLint num_modifiers = c_egl_query_modifiers(egl, format, &modifiers, &external_only);
    if (num_modifiers < 0) continue;

    for (int m = 0; m < num_modifiers; m++) {
      struct c_format_table_pair *pair = &egl->format_table.pairs[i++];
      EGLuint64KHR modifier = modifiers[m];
      pair->format = format;
      pair->modifer = modifier;
     
      if (external_only[m])
        c_log(C_LOG_INFO, "   modifier=%s external_only", drmGetFormatModifierName(modifier));
      else
        c_log(C_LOG_INFO, "   modifier=%s", drmGetFormatModifierName(modifier));
    }

    free(modifiers);
    free(external_only);
  }

  free(formats);
  return 0;
}

int c_egl_import_dmabuf(struct c_egl *egl, struct c_wl_buffer *buffer) {
  uint64_t modifier = buffer->dmabuf->modifier;

  int image_attribs_size = 0;
  EGLint image_attribs[32];

#define add_attrib(key, value) \
  image_attribs[image_attribs_size++] = (key); \
  image_attribs[image_attribs_size++] = (value);

  add_attrib(EGL_WIDTH,                          buffer->width);
  add_attrib(EGL_HEIGHT,                         buffer->height);
  add_attrib(EGL_LINUX_DRM_FOURCC_EXT,           buffer->dmabuf->format);
  add_attrib(EGL_DMA_BUF_PLANE0_FD_EXT,          buffer->dmabuf->fd);
	add_attrib(EGL_DMA_BUF_PLANE0_OFFSET_EXT,      buffer->dmabuf->offset);
	add_attrib(EGL_DMA_BUF_PLANE0_PITCH_EXT,       buffer->dmabuf->stride);


  if (modifier != DRM_FORMAT_INVALID) {
    if (egl->ext_support.EXT_image_dma_buf_import_modifiers)
      modifier = DRM_FORMAT_MOD_LINEAR;
    
    add_attrib(EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, modifier >> 32);
    add_attrib(EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, modifier & 0xFFFFFFFF);
  }

  image_attribs[image_attribs_size] = EGL_NONE;

  EGLImageKHR image = egl->proc.eglCreateImageKHR(egl->display, EGL_NO_CONTEXT, 
                                                  EGL_LINUX_DMA_BUF_EXT, NULL, image_attribs);
  if (!image) {
    c_log(C_LOG_ERROR, "eglCreateImageKHR failed");
    return -1;
  }

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  egl->proc.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

  return 0;
}

void c_egl_free(struct c_egl *egl) {
  if (egl->display) {
    if (egl->context)
      eglDestroyContext(egl->display, egl->context);
    eglTerminate(egl->display);
  }

  if (egl->format_table.pairs)
    free(egl->format_table.pairs);

  free(egl);
}

struct c_egl *c_egl_init(struct gbm_device *device, struct gbm_surface *surface) {
  struct c_egl *egl = calloc(1, sizeof(struct c_egl));
  if (!egl) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  if (has_ext("EGL_EXT_platform_device", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS))) {
    load_proc(eglGetPlatformDisplayEXT);
  }

  EGLDisplay display;
  if (egl->proc.eglGetPlatformDisplayEXT) {
    display = egl->proc.eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, device, NULL);
  } else {
    c_log(C_LOG_ERROR, "EGL_EXT_platform_device not supported\n");
    free(egl);
    return NULL;
  }

  egl->display = display;

  int major, minor;
  if (!eglInitialize(display, &major, &minor)) {
    c_log(C_LOG_ERROR, "eglInitialize failed");
    free(egl);
    return NULL;
  }

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    c_log(C_LOG_ERROR, "Failed to to bind api EGL_OPENGL_ES_API");
    c_egl_free(egl);
    return NULL;
  }

  c_log(C_LOG_INFO, "EGL version %s", eglQueryString(display, EGL_VERSION));
  c_log(C_LOG_INFO, "EGL vendor %s", eglQueryString(display, EGL_VENDOR));
  c_log(C_LOG_INFO, "EGL display extensions %s", eglQueryString(display, EGL_EXTENSIONS));

  if (load_egl_exts(egl) == -1) {
    c_egl_free(egl);
    return NULL;
  }

  c_log(C_LOG_INFO, "GL version %s", glGetString(GL_VERSION));
  c_log(C_LOG_INFO, "GL vendor %s", glGetString(GL_VENDOR));
  c_log(C_LOG_INFO, "GL display extensions %s", glGetString(GL_EXTENSIONS));

  if (load_gl_exts(egl) == -1) {
    c_egl_free(egl);
    return NULL;
  }

  const EGLint config_attribs[] = {
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_DEPTH_SIZE,      8,
    EGL_NONE,
  };

  EGLint num_config = 0;
  if (!eglGetConfigs(display, NULL, 0, &num_config)) {
    c_log(C_LOG_ERROR, "eglGetConfigs failed");
    c_egl_free(egl);
    return NULL;
  }

  EGLConfig configs[num_config];
  EGLint matched;
  if (!eglChooseConfig(display, config_attribs, configs, num_config, &matched)) {
    c_log(C_LOG_ERROR, "eglChooseConfig failed");
    c_egl_free(egl);
    return NULL;

  }

  EGLConfig selected_config = NULL;
  for (int i = 0; i < matched; i++) {
    EGLint id;
    if (!eglGetConfigAttrib(display, configs[i], EGL_NATIVE_VISUAL_ID, &id)) continue;

    if (id == GBM_FORMAT_XRGB8888) { 
      selected_config = configs[i];
      break;
    }
  }

  if (!selected_config) {
    c_log(C_LOG_ERROR, "No config selected");
    c_egl_free(egl);
    return NULL;
  }

  const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE,
  };

  egl->context = eglCreateContext(display, selected_config, EGL_NO_CONTEXT, context_attribs);
  if (!egl->context) {
    c_log(C_LOG_ERROR, "eglCreateContext failed: %s", strerror(errno));
    c_egl_free(egl);
    return NULL;
  }

  egl->surface = eglCreatePlatformWindowSurface(display, selected_config, surface, NULL);
  if (!egl->surface) {
    c_log(C_LOG_ERROR, "eglCreatePlatformWindowSurface failed");
    c_egl_free(egl);
    return NULL;
  }

  if (!eglMakeCurrent(display, egl->surface, egl->surface, egl->context)) {
    c_log(C_LOG_ERROR, "eglMakeCurrent failed");
    c_egl_free(egl);
    return NULL;
  };

  if (get_format_table(egl) == -1) {
    c_egl_free(egl);
    return NULL;
  }

  return egl;
}
  
