#include <stdlib.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <inttypes.h>

#include "render/gl/gles.h"

#include "util/log.h"
#include "util/helpers.h"
#include "output/drm/util.h"

#include "render/shaders/shader-vert.h"
#include "render/shaders/solid-frag.h"
#include "render/shaders/texture-frag.h"
#include "render/shaders/textureext-frag.h"

#define VERT_POS_TOP_LEFT(vp)     vp.tl_x, -(vp.tl_y)
#define VERT_POS_BOTTOM_LEFT(vp)  vp.bl_x, -(vp.bl_y)
#define VERT_POS_TOP_RIGHT(vp)    vp.tr_x, -(vp.tr_y)
#define VERT_POS_BOTTOM_RIGHT(vp) vp.br_x, -(vp.br_y)

#define VERT_TOP_LEFT(vp)     VERT_POS_TOP_LEFT(vp),     0.0f, 0.0f
#define VERT_BOTTOM_LEFT(vp)  VERT_POS_BOTTOM_LEFT(vp),  0.0f, 1.0f
#define VERT_BOTTOM_RIGHT(vp) VERT_POS_BOTTOM_RIGHT(vp), 1.0f, 1.0f
#define VERT_TOP_RIGHT(vp)    VERT_POS_TOP_RIGHT(vp),    1.0f, 0.0f

#define VERTS(vp)                                                          \
  {                                                                            \
      VERT_TOP_LEFT(vp),     VERT_BOTTOM_LEFT(vp),                     \
      VERT_BOTTOM_RIGHT(vp), VERT_TOP_LEFT(vp),                        \
      VERT_BOTTOM_RIGHT(vp), VERT_TOP_RIGHT(vp),                       \
  }


#define get_location(prog, name)                                               \
  GLint name##_loc = glGetUniformLocation(prog, #name);

struct vert_pos {
	float tl_x, tl_y;
	float bl_x, bl_y;
	float br_x, br_y;
	float tr_x, tr_y;
};

const char *asdfasdf = 
"asdfasdf";

static GLuint compile_shader(const char *shader_text, GLenum type) {
  GLuint shader = glCreateShader(type);

  glShaderSource(shader, 1, &shader_text, NULL);
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
    if (STREQ(token, ext)) {
      c_log(C_LOG_DEBUG, "loaded GL %s extension", ext);
      return 1;
    }
    token = strtok(NULL, " ");
  }

  return 0;

}

static void gl_log(GLenum source, GLenum type, GLuint id, GLenum severity,
                   GLsizei length, const GLchar *message,
                   const void *userParam) {
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

  // if (has_ext_gles("GL_OES_EGL_image", exts)) {
  //   load_gl_proc(glEGLImageTargetRenderbufferStorageOES);
  // } else {
  //   c_log(C_LOG_ERROR, "GL_OES_EGL_image not supported");
  //   return -1;
  // }

  if (has_ext_gles("GL_KHR_debug", exts)) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    load_gl_proc(glDebugMessageControlKHR);
    load_gl_proc(glDebugMessageCallbackKHR);

    gl->proc.glDebugMessageCallbackKHR(gl_log, NULL);
  }

  if (has_ext_gles("GL_EXT_texture_format_BGRA8888", exts)) {
    gl->ext_support.EXT_texture_format_BGRA8888 = 1;
  } else {
    c_log(C_LOG_WARNING, "GL_EXT_texture_format_BGRA8888 not supported");
  }

  

  return 0;

}

static inline double value_transform_x(double value, int max_value) {
	return -1 + (double)value/max_value * 2;
}

static inline double value_transform_y(double value, int max_value) {
	return 1 + (double)value/max_value * -2;
}

static void create_verts(uint32_t width, uint32_t height, double x, double y,
		struct vert_pos *vp, uint32_t max_width, uint32_t max_height) {

	vp->tl_x = value_transform_x(x, max_width);
	vp->tl_y = value_transform_y(y, max_height);

	vp->bl_x = value_transform_x(x, max_width);
	vp->bl_y = value_transform_y(y + height, max_height);

	vp->br_x = value_transform_x(x + width, max_width);
	vp->br_y = value_transform_y(y + height, max_height);

	vp->tr_x = value_transform_x(x + width, max_width);
	vp->tr_y = value_transform_y(y, max_height);
}

int c_gles_texture_from_raw(struct c_gles *gl, struct c_rawbuf *buf) {
  struct c_gles_texture *texture;
  GLenum internal_format, format, type;
  if (drm_fmt_to_gl_fmt(buf->format, &internal_format, &format, &type,
                        gl->ext_support.EXT_texture_format_BGRA8888) == -1) {
    c_log(C_LOG_WARNING, "unable to find corresponding GL formats for the DRM format 0x%"PRIX32);
    c_log(C_LOG_WARNING, "using default values");
    internal_format = GL_RGBA;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
  }

  if (!buf->texture) {
    texture = calloc(1, sizeof(*texture));
    if (!texture) {
      c_log_errno(C_LOG_ERROR, "failed to allocate c_gles_texture");
      return -1;
    }

    buf->texture = texture;
    texture->target = GL_TEXTURE_2D;

    glGenTextures(1, &texture->texture);
    glBindTexture(texture->target, texture->texture);

    glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, buf->stride / 4);

    glTexImage2D(texture->target, 0, internal_format, buf->width, buf->height, 0,
                 format, type, buf->base_ptr + buf->offset);


  } else {
    texture = buf->texture;
    glBindTexture(texture->target, texture->texture);
    glTexSubImage2D(texture->target, 0, 0, 0, buf->width, buf->height, format, type, buf->base_ptr + buf->offset);
  }

  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glBindTexture(texture->target, 0);

  return 0;
}

int c_gles_texture_from_dma(struct c_gles *gl, struct c_dmabuf *buf) {
  struct c_gles_texture *texture = calloc(1, sizeof(*texture));
  if (!texture) {
    c_log_errno(C_LOG_ERROR, "failed to allocate c_gles_texture");
    return -1;
  }

  buf->texture = texture;
  texture->target = GL_TEXTURE_EXTERNAL_OES;

  glGenTextures(1, &texture->texture);
  glBindTexture(texture->target, texture->texture);

  glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->proc.glEGLImageTargetTexture2DOES(texture->target, buf->image);
  glBindTexture(texture->target, 0);

  return 0;
}

void c_gles_add_solid(struct c_gles *gl, struct c_output *output, struct c_renderer_quad *quad) {
  struct c_output_mode *mode = output->current_mode;
  GLuint gl_program = gl->solid_program;

  glUseProgram(gl_program);
  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  
	struct vert_pos vp = {0};
	create_verts(quad->width, quad->height, quad->x, quad->y, &vp, mode->width, mode->height);

  float verts[] = VERTS(vp);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

  get_location(gl_program, color);
  glUniform4f(color_loc, quad->color[0], quad->color[1], quad->color[2], quad->color[3]);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void c_gles_add_texture(struct c_gles *gl, struct c_output *output,
                      struct c_renderer_quad *quad,
                      struct c_gles_texture *texture) {
  struct c_output_mode *mode = output->current_mode;

  GLuint gl_program;
  if (texture->target == GL_TEXTURE_2D)
    gl_program = gl->tex_program;
  else
    gl_program = gl->tex_ext_program;


  glUseProgram(gl_program);
  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  
  glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->texture);

	struct vert_pos vp = {0};
	create_verts(quad->width, quad->height, quad->x, quad->y, &vp, mode->width, mode->height);

  float verts[] = VERTS(vp);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

  get_location(gl_program, tex);
  glUniform1i(tex_loc, 0);

  get_location(gl_program, transform);
  glUniformMatrix3fv(transform_loc, 1, GL_TRUE, (GLfloat *)quad->transform);

	glDrawArrays(GL_TRIANGLES, 0, 6);

}

void c_gles_free(struct c_gles *gl) {
  if (gl->tex_program)
    glDeleteProgram(gl->tex_program);

  if (gl->tex_ext_program)
    glDeleteProgram(gl->tex_ext_program);

  if (gl->solid_program)
    glDeleteProgram(gl->solid_program);

  if (gl->vao)
    glDeleteVertexArrays(1, &gl->vao);

  if (gl->vbo)
    glDeleteBuffers(1, &gl->vbo);

  free(gl);
}


static int create_program(struct c_gles *gl, const char *frag_shader, GLuint *prog) {
  GLuint vertex, fragment;
  if (!(vertex = compile_shader(shader_vert, GL_VERTEX_SHADER))) {
    c_log(C_LOG_ERROR, "failed to compile vertex shader");
    return -1;
  }

  if (!(fragment = compile_shader(frag_shader, GL_FRAGMENT_SHADER))) {
    c_log(C_LOG_ERROR, "failed to compile %s shader", frag_shader);
    return -1;
  }

  *prog = glCreateProgram();
  glAttachShader(*prog, vertex);
  glAttachShader(*prog, fragment);

  glLinkProgram(*prog);

  GLint success_link = 0;
  glGetProgramiv(*prog, GL_LINK_STATUS, &success_link);
  if (success_link == GL_FALSE) {
    GLint err_size = 0;
    glGetProgramiv(*prog, GL_INFO_LOG_LENGTH, &err_size);
    char err[err_size];
    glGetProgramInfoLog(*prog, err_size, &err_size, err);
    c_log(C_LOG_ERROR, "glLinkProgram failed: %s", err);
    goto err_link;
  }

  glDetachShader(*prog, vertex);
  glDetachShader(*prog, fragment);
  glDeleteShader(vertex);
  glDeleteShader(fragment);

  return 0;

err_link:
  glDetachShader(*prog, vertex);
  glDetachShader(*prog, fragment);
  glDeleteProgram(*prog);
  glDeleteShader(vertex);
  glDeleteShader(fragment);

  return -1;
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

  if (create_program(gl, solid_frag, &gl->solid_program) == -1) {
    c_log(C_LOG_ERROR, "failed to create program");
    goto err;
  }

  if (create_program(gl, texture_frag, &gl->tex_program) == -1) {
    c_log(C_LOG_ERROR, "failed to create program");
    goto err;
  }

  if (create_program(gl, textureext_frag, &gl->tex_ext_program) == -1) {
    c_log(C_LOG_ERROR, "failed to create program");
    goto err;
  }

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

err:
  c_gles_free(gl);
  return NULL;
}
