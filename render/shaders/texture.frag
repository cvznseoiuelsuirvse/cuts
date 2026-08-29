#version 300 es
precision highp float;

in vec2 v_uv;

uniform sampler2D tex;
uniform mat3 transform;

out vec4 fragColor;

void main() {
  vec2 uv = (vec3(v_uv, 1.0) * transform).xy;
  fragColor = texture(tex, uv);
}
