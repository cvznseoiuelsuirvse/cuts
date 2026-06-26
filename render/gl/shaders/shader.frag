#version 300 es
precision mediump float;

in vec2 v_uv;

uniform sampler2D tex;

uniform vec4 border_color;
uniform vec2 border_size;

out vec4 fragColor;

void main() {
	vec2 dist = min(v_uv, 1.0 - v_uv);
	if (dist.x < border_size.x || dist.y < border_size.y) {
		fragColor = border_color;
	} else {
		fragColor = texture(tex, (v_uv - border_size) / (1.0 - 2.0 * border_size));
	}
}
