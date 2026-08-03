#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision highp float;

in vec2 v_uv;

uniform samplerExternalOES tex;
uniform vec4 border_color;
uniform vec2 border_size;
uniform bool draw_border;
uniform vec2 uv_offset;
uniform vec2 uv_scale;

out vec4 fragColor;

void main() {
  vec2 content_uv = v_uv;

  if (draw_border) {
    vec2 dist = min(v_uv, 1.0 - v_uv);
    if (dist.x < border_size.x || dist.y < border_size.y) {
      fragColor = border_color;
      return;
    }

    content_uv = (content_uv - border_size) / (1.0 - 2.0 * border_size);
  }

  vec2 uv = uv_offset + content_uv * uv_scale;
  fragColor = texture(tex, uv);
}
