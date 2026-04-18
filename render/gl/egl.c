#include <unistd.h>
#include <inttypes.h>
#include <gbm.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>

#include "render/render.h"
#include "render/gl/egl.h"

#include "backend/drm/util.h"
#include "util/log.h"
#include "util/helpers.h"

#define EGL_DMA_BUF_PLANEX_FD_EXT(n)          EGL_DMA_BUF_PLANE0_FD_EXT + (n) * 3
#define EGL_DMA_BUF_PLANEX_OFFSET_EXT(n)      EGL_DMA_BUF_PLANE0_FD_EXT + (n) * 3 + 1
#define EGL_DMA_BUF_PLANEX_PITCH_EXT(n)       EGL_DMA_BUF_PLANE0_FD_EXT + (n) * 3 + 2
#define EGL_DMA_BUF_PLANEX_MODIFIER_LO_EXT(n) EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT + (n) * 2
#define EGL_DMA_BUF_PLANEX_MODIFIER_HI_EXT(n) EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT + (n) * 2 + 1
#define egl_error(func) c_log(C_LOG_ERROR, func " failed: %s", egl_error_string(eglGetError()))
#define egl_case_str( value ) case value: return #value

const char* egl_error_string(EGLint error) {
  switch(error) {
  egl_case_str( EGL_NOT_INITIALIZED     );
  egl_case_str( EGL_BAD_ACCESS          );
  egl_case_str( EGL_BAD_ALLOC           );
  egl_case_str( EGL_BAD_ATTRIBUTE       );
  egl_case_str( EGL_BAD_CONTEXT         );
  egl_case_str( EGL_BAD_CONFIG          );
  egl_case_str( EGL_BAD_CURRENT_SURFACE );
  egl_case_str( EGL_BAD_DISPLAY         );
  egl_case_str( EGL_BAD_SURFACE         );
  egl_case_str( EGL_BAD_MATCH           );
  egl_case_str( EGL_BAD_PARAMETER       );
  egl_case_str( EGL_BAD_NATIVE_PIXMAP   );
  egl_case_str( EGL_BAD_NATIVE_WINDOW   );
  egl_case_str( EGL_CONTEXT_LOST        );
  default: return "Unknown";
  }
}


static int has_ext_egl(const char *ext, const char *exts) {
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

static int load_egl_exts(struct c_egl *egl) {
  const char *exts_display = eglQueryString(egl->display, EGL_EXTENSIONS);

#define load_egl_proc(name) egl->proc.name = (void *)eglGetProcAddress(#name);
  if (has_ext_egl("EGL_KHR_image_base", exts_display)) {
    load_egl_proc(eglCreateImageKHR);
    load_egl_proc(eglDestroyImageKHR);
  } else {
    c_log(C_LOG_ERROR, "EGL_KHR_image_base not supported");
    return -1;
  }

  if (!has_ext_egl("EGL_EXT_image_dma_buf_import", exts_display)) {
    c_log(C_LOG_ERROR, "EGL_EXT_image_dma_buf_import not supported");
    return -1;
  }

  if (has_ext_egl("EGL_EXT_image_dma_buf_import_modifiers", exts_display)) {
    egl->ext_support.EXT_image_dma_buf_import_modifiers = 1;
    load_egl_proc(eglQueryDmaBufFormatsEXT);
    load_egl_proc(eglQueryDmaBufModifiersEXT);
  }

  if (!has_ext_egl("EGL_KHR_surfaceless_context", exts_display)) {
    c_log(C_LOG_ERROR, "EGL_KHR_surfaceless_context not supported");
    return -1;
  }

  return 0;

}

static int c_egl_get_modifiers(struct c_egl *egl, EGLint format, 
                                 EGLuint64KHR **modifiers, EGLBoolean **external_only) {
  EGLint num_modifiers;
  if (egl->proc.eglQueryDmaBufModifiersEXT(egl->display, format, 0, NULL, NULL, &num_modifiers) != EGL_TRUE) {
    egl_error("eglQueryDmaBufModifiersEXT(NULL)");
    return -1;
  }

  *modifiers = calloc(num_modifiers, sizeof(**modifiers));
  if (!modifiers) {
    c_log(C_LOG_ERROR, "calloc modifiers failed");
    goto error;
  }
  *external_only = calloc(num_modifiers, sizeof(**external_only));
  if (!external_only) {
    c_log(C_LOG_ERROR, "calloc external_only failed");
    goto error;
  }

  if (egl->proc.eglQueryDmaBufModifiersEXT(egl->display, format, num_modifiers, 
                                           *modifiers, *external_only, &num_modifiers) != EGL_TRUE) {
    egl_error("eglQueryDmaBufModifiersEXT");
    goto error;
  }

  return num_modifiers;

error:
  if (modifiers) free(modifiers);
  if (external_only) free(external_only);
  return -1;

}

static int c_egl_get_formats(struct c_egl *egl, EGLint **formats) {
  EGLint num_formats;
  if (egl->proc.eglQueryDmaBufFormatsEXT(egl->display, 0, NULL, &num_formats) != EGL_TRUE) {
    egl_error("eglQueryDmaBufFormatsEXT(NULL)");
    return -1;
  }

  *formats = calloc(num_formats, sizeof(**formats));
  if (!formats) {
    c_log(C_LOG_ERROR, "calloc failed");
    return -1;
  }

  if (egl->proc.eglQueryDmaBufFormatsEXT(egl->display, num_formats, *formats, &num_formats) != EGL_TRUE) {
    free(formats);
    egl_error("eglQueryDmaBufFormatsEXT");
    return -1;
  }

  return num_formats;
}


struct c_format *c_egl_query_formats(struct c_egl *egl, size_t *n_entries) {
  EGLint *formats = NULL;
  EGLint num_formats = c_egl_get_formats(egl, &formats);
  if (num_formats == -1) return NULL;


  for (int f = 0; f < num_formats; f++) {
    EGLint format = formats[f];    
    EGLuint64KHR *modifiers = NULL;
    EGLBoolean *external_only = NULL;

    EGLint num_modifiers = c_egl_get_modifiers(egl, format, &modifiers, &external_only);
    if (num_modifiers < 0) continue;

    if (num_modifiers == 0)
      (*n_entries)++;
    else
      for (int m = 0; m < num_modifiers; m++, (*n_entries)++);

    free(modifiers);
    free(external_only);
  }

  struct c_format *table = malloc(*n_entries * sizeof(*table));
  if (!table) {
    c_log(C_LOG_ERROR, "calloc failed");
    free(formats);
    return NULL;
  }

  int i = 0;
  for (int f = 0; f < num_formats; f++) {
    EGLint format = formats[f];    

    EGLuint64KHR *modifiers = NULL;
    EGLBoolean *external_only = NULL;


    EGLint num_modifiers = c_egl_get_modifiers(egl, format, &modifiers, &external_only);

    int no_modifiers_found = 0;
    if (num_modifiers == 0) {
      no_modifiers_found = 1;
      num_modifiers = 1;
    }

    if (num_modifiers < 0)  continue;

    char *format_name = drmGetFormatName(format);
    c_log(C_LOG_DEBUG, "format=0x%"PRIX32" %4s", format, format_name, num_modifiers);
    free(format_name);

    for (int m = 0; m < num_modifiers; m++) {
      struct c_format *entry = &table[i++];
      EGLuint64KHR modifier;
      if (no_modifiers_found)
        modifier = DRM_FORMAT_MOD_LINEAR;
      else
        modifier = modifiers[m];

      entry->drm_format = format;
      entry->modifier = modifier;
      entry->n_planes = drm_format_num_planes(format);

      GLint max_tex_size;
      glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);

      entry->max_width = max_tex_size;
      entry->max_height = max_tex_size;
     
      char *modifier_name = drmGetFormatModifierName(modifier);
      if (!no_modifiers_found && external_only[m])
        c_log(C_LOG_DEBUG, "   modifier=0x%"PRIX64" %s", modifier, modifier_name);
      else
        c_log(C_LOG_DEBUG, "   modifier=0x%"PRIX64" %s (render)", modifier, modifier_name);
      free(modifier_name);
    }

    free(modifiers);
    free(external_only);
  }

  free(formats);
  return table;
}

EGLImageKHR c_egl_create_image_from_dmabuf(struct c_egl *egl, struct c_dmabuf_params *params) {
  uint64_t modifier = params->modifier;
  int use_modifier = modifier != DRM_FORMAT_MOD_INVALID && egl->ext_support.EXT_image_dma_buf_import_modifiers;

  int image_attribs_size = 0;
  EGLint image_attribs[32];

#define add_attrib(key, value) \
  image_attribs[image_attribs_size++] = (key); \
  image_attribs[image_attribs_size++] = (value);

  add_attrib(EGL_WIDTH,                          params->width);
  add_attrib(EGL_HEIGHT,                         params->height);
  add_attrib(EGL_LINUX_DRM_FOURCC_EXT,           params->drm_format);

  for (size_t i = 0; i < params->n_planes; i++) {
    add_attrib(EGL_DMA_BUF_PLANEX_FD_EXT(i), params->planes[i].fd);
    add_attrib(EGL_DMA_BUF_PLANEX_OFFSET_EXT(i), params->planes[i].offset);
    add_attrib(EGL_DMA_BUF_PLANEX_PITCH_EXT(i), params->planes[i].stride);
    if (use_modifier) {
      add_attrib(EGL_DMA_BUF_PLANEX_MODIFIER_HI_EXT(i), modifier >> 32);
      add_attrib(EGL_DMA_BUF_PLANEX_MODIFIER_LO_EXT(i), modifier & 0xFFFFFFFF);
    }
  }

  image_attribs[image_attribs_size] = EGL_NONE;

  EGLImageKHR image = egl->proc.eglCreateImageKHR(egl->display, EGL_NO_CONTEXT, 
                                                  EGL_LINUX_DMA_BUF_EXT, NULL, image_attribs);
  if (!image)
    egl_error("eglCreateImageKHR");

  return image;
}

void c_egl_free(struct c_egl *egl) {
  if (egl->display) {
    eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (egl->context) eglDestroyContext(egl->display, egl->context);
    eglTerminate(egl->display);
  }

  free(egl);
}


struct c_egl *c_egl_init(struct gbm_device *device) {
  struct c_egl *egl = calloc(1, sizeof(struct c_egl));
  if (!egl) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  if (has_ext_egl("EGL_EXT_platform_device", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS)))
    egl->proc.eglGetPlatformDisplayEXT = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
  

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
    egl_error("eglInitialize");
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

  const EGLint config_attribs[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_DEPTH_SIZE,      8,
    EGL_NONE,
  };

  
  EGLint n_configs;
  if (!eglGetConfigs(display, NULL, 0, &n_configs)) {
    egl_error("eglGetConfigs");
    c_egl_free(egl);
    return NULL;
  }

  EGLConfig configs[n_configs];
  EGLint matched;
  if (!eglChooseConfig(display, config_attribs, configs, n_configs, &matched)) {
    egl_error("eglChooseConfig");
    goto err;
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
    goto err;
  }

  const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE,
  };

  egl->context = eglCreateContext(display, selected_config, EGL_NO_CONTEXT, context_attribs);
  if (!egl->context) {
    egl_error("eglCreateContext");
    goto err;
  }

  if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl->context)) {
    egl_error("eglMakeCurrent");
    goto err;
  };

  return egl;

err:
  c_egl_free(egl);
  return NULL;
}
