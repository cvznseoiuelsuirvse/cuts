#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision highp float;

in vec2 v_uv;

uniform samplerExternalOES tex;
uniform mat3 transform;

out vec4 fragColor;

void main() {
  vec2 uv = (vec3(v_uv, 1.0) * transform).xy;
  fragColor = texture(tex, uv);
}
