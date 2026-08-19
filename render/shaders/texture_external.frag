#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision highp float;

in vec2 v_uv;

uniform samplerExternalOES tex;
uniform vec2 uv_offset;
uniform vec2 uv_scale;

out vec4 fragColor;

void main() {
  vec2 uv = uv_offset + v_uv * uv_scale;
  fragColor = texture(tex, uv);
}
