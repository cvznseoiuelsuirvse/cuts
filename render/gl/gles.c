#include <stdlib.h>
#include <unistd.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>

#include "render/render.h"
#include "render/gl/gles.h"

#include "util/log.h"
#include "util/helpers.h"

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

static int has_ext_gles(const char *ext, const char *exts) {
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

  if (has_ext_gles("GL_OES_EGL_image_external", exts)) {
    load_gl_proc(glEGLImageTargetTexture2DOES);
    gl->ext_support.OES_EGL_image_external = 1;

  } else {
    c_log(C_LOG_ERROR, "GL_OES_EGL_image_external not supported");
    return -1;
  }

  if (has_ext_gles("GL_OES_EGL_image", exts)) {
    load_gl_proc(glEGLImageTargetRenderbufferStorageOES);
  } else {
    c_log(C_LOG_ERROR, "GL_OES_EGL_image not supported");
    return -1;
  }

  if (has_ext_gles("GL_KHR_debug", exts)) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    load_gl_proc(glDebugMessageControlKHR);
    load_gl_proc(glDebugMessageCallbackKHR);

    gl->proc.glDebugMessageCallbackKHR(gl_log, NULL);
  }

  return 0;

}

void c_gles_texture_from_shm(struct c_shm *buf, uint32_t width, uint32_t height) {
  glGenTextures(1, &buf->texture);
  glBindTexture(GL_TEXTURE_2D, buf->texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glPixelStorei(GL_UNPACK_ROW_LENGTH, buf->stride / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (*buf->base_ptr) + buf->offset);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void c_gles_texture_from_dmabuf_image(struct c_gles *gl, struct c_dmabuf *buf) {
  glGenTextures(1, &buf->texture);
  glBindTexture(GL_TEXTURE_2D, buf->texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  gl->proc.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buf->image);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void c_gles_free(struct c_gles *gl) {
  if (gl->program) {
    glDeleteProgram(gl->program);
  }

  if (gl->vao)
    glDeleteVertexArrays(1, &gl->vao);

  if (gl->vbo)
    glDeleteBuffers(1, &gl->vbo);

  free(gl);
}

struct c_gles *c_gles_init() {
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

  glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

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
