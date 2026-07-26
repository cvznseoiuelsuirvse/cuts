#version 300 es
precision mediump float;

in vec2 v_uv;

uniform sampler2D tex;

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

  fragColor = texture(tex, (content_uv + uv_offset) * uv_scale);
}
