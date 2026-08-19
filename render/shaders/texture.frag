#version 300 es
precision highp float;

in vec2 v_uv;

uniform sampler2D tex;
uniform vec2 uv_offset;
uniform vec2 uv_scale;

out vec4 fragColor;

void main() {
  vec2 uv = uv_offset + v_uv * uv_scale;
  fragColor = texture(tex, v_uv);
}
