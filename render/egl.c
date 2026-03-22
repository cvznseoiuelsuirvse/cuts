#include <stdlib.h>
#include <drm_fourcc.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <drm_mode.h>
#include <xf86drm.h>
#include <gbm.h>


#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>

#include "render/render.h"
#include "render/egl.h"

#include "backend/drm/drm.h"
#include "util/log.h"
#include "util/helpers.h"

#define EGL_DMA_BUF_PLANEX_FD_EXT(n)          EGL_DMA_BUF_PLANE0_FD_EXT + (n) * 3
#define EGL_DMA_BUF_PLANEX_OFFSET_EXT(n)      EGL_DMA_BUF_PLANE0_FD_EXT + (n) * 3 + 1
#define EGL_DMA_BUF_PLANEX_PITCH_EXT(n)       EGL_DMA_BUF_PLANE0_FD_EXT + (n) * 3 + 2
#define EGL_DMA_BUF_PLANEX_MODIFIER_LO_EXT(n) EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT + (n) * 2
#define EGL_DMA_BUF_PLANEX_MODIFIER_HI_EXT(n) EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT + (n) * 2 + 1


static const GLchar *vertex_shader = 
  "#version 300 es\n"
  "layout(location = 0) in vec2 pos;\n"
  "layout(location = 1) in vec2 uv;\n"
  "\n"
  "out vec2 v_uv;\n"
  "\n"
  "void main() {\n"
  "    v_uv = uv;\n"
  "    gl_Position = vec4(pos, 0.0, 1.0);\n"
  "}\n";

static const GLchar *fragment_shader = 
  "#version 300 es\n"
  "precision mediump float;\n"
  "\n"
  "in vec2 v_uv;\n"
  "uniform sampler2D tex;\n"
  "\n"
  "out vec4 frag_color;\n"
  "\n"
  "void main() {\n"
  "    frag_color = texture(tex, v_uv);\n"
  "}\n";


static GLuint compile_shader(GLenum type) {
  GLuint shader = glCreateShader(type);
  if (type == GL_VERTEX_SHADER)
    glShaderSource(shader, 1, &vertex_shader, NULL);
  else
    glShaderSource(shader, 1, &fragment_shader, NULL);

  glCompileShader(shader);

  GLint success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    GLint err_size = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &err_size);
    char err[err_size];
    glGetShaderInfoLog(shader, err_size, &err_size, err);
    if (type == GL_VERTEX_SHADER)
      c_log(C_LOG_ERROR, "failed to compile vertex shader: %s", err);
    else
      c_log(C_LOG_ERROR, "failed to compile fragment shader: %s", err);

    glDeleteShader(shader);
    return 0;
  }

  return shader;
  
}

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

static void gl_log(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
  enum c_log_level log_level;
  switch (type) {
    case GL_DEBUG_TYPE_ERROR_KHR:               log_level = C_LOG_ERROR;  break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR: log_level = C_LOG_INFO;   break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR:  log_level = C_LOG_ERROR;  break;
    case GL_DEBUG_TYPE_PORTABILITY_KHR:         log_level = C_LOG_INFO;   break;
    case GL_DEBUG_TYPE_PERFORMANCE_KHR:         log_level = C_LOG_INFO;   break;
    case GL_DEBUG_TYPE_OTHER_KHR:               log_level = C_LOG_INFO;   break;
    case GL_DEBUG_TYPE_MARKER_KHR:              log_level = C_LOG_INFO;   break;
    case GL_DEBUG_TYPE_PUSH_GROUP_KHR:          log_level = C_LOG_INFO;   break;
    case GL_DEBUG_TYPE_POP_GROUP_KHR:           log_level = C_LOG_INFO;   break;
    default:                                    log_level = C_LOG_INFO;   break;
	}

  c_log(log_level, "[GL] %s", message);
  
};

static int load_gl_exts(struct c_gles *gl) {
  const char *exts = (const char *)glGetString(GL_EXTENSIONS);

#define load_gl_proc(name) gl->proc.name = (void *)eglGetProcAddress(#name);

  if (has_ext("GL_OES_EGL_image_external", exts)) {
    load_gl_proc(glEGLImageTargetTexture2DOES);
  } else {
    c_log(C_LOG_ERROR, "GL_OES_EGL_image_external not supported");
    return -1;
  }

  if (has_ext("GL_KHR_debug", exts)) {
    load_gl_proc(glDebugMessageControlKHR);
    load_gl_proc(glDebugMessageCallbackKHR);

    gl->proc.glDebugMessageCallbackKHR(gl_log, NULL);
  }

  return 0;

}

static int load_egl_exts(struct c_egl *egl) {
  const char *exts_display = eglQueryString(egl->display, EGL_EXTENSIONS);

#define load_egl_proc(name) egl->proc.name = (void *)eglGetProcAddress(#name);
  if (has_ext("EGL_KHR_image_base", exts_display)) {
    load_egl_proc(eglCreateImageKHR);
    load_egl_proc(eglDestroyImageKHR);
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
    load_egl_proc(eglQueryDmaBufFormatsEXT);
    load_egl_proc(eglQueryDmaBufModifiersEXT);
  } else {    
    c_log(C_LOG_ERROR, "EGL_EXT_image_dma_buf_import_modifiers not supported");
    return -1;
  }

  return 0;

}

static int c_egl_get_modifiers(struct c_egl *egl, EGLint format, 
                                 EGLuint64KHR **modifiers, EGLBoolean **external_only) {
  EGLint num_modifiers;
  if (egl->proc.eglQueryDmaBufModifiersEXT(egl->display, format, 0, NULL, NULL, &num_modifiers) != EGL_TRUE) {
    c_log(C_LOG_ERROR, "eglQueryDmaBufModifiersEXT(NULL) failed");
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
    c_log(C_LOG_ERROR, "eglQueryDmaBufModifiersEXT failed");
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
    c_log(C_LOG_DEBUG, "format=%4s", format_name, num_modifiers);
    free(format_name);

    for (int m = 0; m < num_modifiers; m++) {
      struct c_format *entry = &table[i++];
      EGLuint64KHR modifier;
      if (no_modifiers_found)
        modifier = DRM_FORMAT_MOD_INVALID;
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
        c_log(C_LOG_DEBUG, "   modifier=%s external_only", modifier_name);
      else
        c_log(C_LOG_DEBUG, "   modifier=%s", modifier_name);
      free(modifier_name);
    }

    free(modifiers);
    free(external_only);
  }

  free(formats);
  return table;
}

int c_egl_import_dmabuf(struct c_egl *egl, struct c_dmabuf_params *params, struct c_dmabuf *buf) {
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

  buf->image = egl->proc.eglCreateImageKHR(egl->display, EGL_NO_CONTEXT, 
                                                  EGL_LINUX_DMA_BUF_EXT, NULL, image_attribs);
  if (!buf->image) {
    c_log(C_LOG_ERROR, "eglCreateImageKHR failed");
    return -1;
  }
      
  for (uint32_t i = 0; i < buf->n_planes; i++) {
    close(buf->planes[i].fd);
  } 

  glGenTextures(1, &buf->texture);
  glBindTexture(GL_TEXTURE_2D, buf->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  egl->gl->proc.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buf->image);
  glBindTexture(GL_TEXTURE_2D, 0);

  return 0;
}

static void c_gles_free(struct c_gles *gl) {
  if (gl->program) {
    glDeleteProgram(gl->program);
  }

  if (gl->vao)
    glDeleteVertexArrays(1, &gl->vao);

  if (gl->vbo)
    glDeleteBuffers(1, &gl->vbo);

  free(gl);
}

void c_egl_free(struct c_egl *egl) {
  if (egl->display) {
    if (egl->context && egl->surface)
      eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);

    if (egl->gl)
      c_gles_free(egl->gl);

    eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (egl->surface) eglDestroySurface(egl->display, egl->surface);
    if (egl->context) eglDestroyContext(egl->display, egl->context);
    eglTerminate(egl->display);
  }

  free(egl);
}


static struct c_gles *c_gles_init() {
  struct c_gles *gl = calloc(1, sizeof(*gl));
  if (!gl) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  if (load_gl_exts(gl) == -1)
    goto err;

  c_log(C_LOG_INFO, "GL version %s", glGetString(GL_VERSION));
  c_log(C_LOG_INFO, "GL vendor %s", glGetString(GL_VENDOR));
  c_log(C_LOG_INFO, "GL display extensions %s", glGetString(GL_EXTENSIONS));


  GLuint vertex, fragment;
  if (!(vertex = compile_shader(GL_VERTEX_SHADER))) goto err;
  if (!(fragment = compile_shader(GL_FRAGMENT_SHADER))) goto err;

  GLuint prog = glCreateProgram();
  glAttachShader(prog, vertex);
  glAttachShader(prog, fragment);

  glLinkProgram(prog);

  GLint success_link = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &success_link);
  if (success_link == GL_FALSE) {
    GLint err_size = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &err_size);
    char err[err_size];
    glGetProgramInfoLog(prog, err_size, &err_size, err);
    c_log(C_LOG_ERROR, "glLinkProgram failed: %s", err);

    goto err_link;
  }

  glDetachShader(prog, vertex);
  glDetachShader(prog, fragment);
  glDeleteShader(vertex);
  glDeleteShader(fragment);

  gl->program = prog;

  int stride = 4 * sizeof(float);
  glGenVertexArrays(1, &gl->vao);
  glGenBuffers(1, &gl->vbo);

  glBindVertexArray(gl->vao);

  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  glBufferData(GL_ARRAY_BUFFER, 4 * 6 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  return gl;

err_link:
  glDetachShader(prog, vertex);
  glDetachShader(prog, fragment);
  glDeleteProgram(prog);
  glDeleteShader(vertex);
  glDeleteShader(fragment);

err:
  c_gles_free(gl);
  return NULL;
}

int draw_quad() {
  return 0;
}

inline void c_egl_swap_buffers(struct c_egl *egl) {
  eglSwapBuffers(egl->display, egl->surface);
}

struct c_egl *c_egl_init(struct gbm_device *device, struct gbm_surface *surface) {
  struct c_egl *egl = calloc(1, sizeof(struct c_egl));
  if (!egl) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  if (has_ext("EGL_EXT_platform_device", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS)))
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

  const EGLint config_attribs[] = {
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_DEPTH_SIZE,      8,
    EGL_NONE,
  };

  
  EGLint n_configs;
  if (!eglGetConfigs(display, NULL, 0, &n_configs)) {
    c_log(C_LOG_ERROR, "eglGetConfigs failed");
    c_egl_free(egl);
    return NULL;
  }

  EGLConfig configs[n_configs];
  EGLint matched;
  if (!eglChooseConfig(display, config_attribs, configs, n_configs, &matched)) {
    c_log(C_LOG_ERROR, "eglChooseConfig failed");
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
    c_log_errno(C_LOG_ERROR, "eglCreateContext failed");
    goto err;
  }

  egl->surface = eglCreatePlatformWindowSurface(display, selected_config, surface, NULL);
  if (!egl->surface) {
    c_log(C_LOG_ERROR, "eglCreatePlatformWindowSurface failed");
    goto err;
  }

  if (!eglMakeCurrent(display, egl->surface, egl->surface, egl->context)) {
    c_log(C_LOG_ERROR, "eglMakeCurrent failed");
    goto err;
  };


  egl->gl = c_gles_init();
  if (!egl->gl)
    goto err;
  
  return egl;

err:
  c_egl_free(egl);
  return NULL;
}
