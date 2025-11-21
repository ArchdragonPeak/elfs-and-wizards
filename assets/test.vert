#version 430

in vec4 position;
in vec2 uv;

out vec2 v_uv;

void main() {
    gl_Position = position;
    v_uv = uv;
}
