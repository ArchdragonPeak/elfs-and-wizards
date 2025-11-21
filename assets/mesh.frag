#version 430
precision mediump float;


uniform sampler2D texture0;

in vec2 v_uv;
in vec4 fragColor;          // Fragment input attribute: color
out vec4 finalColor;        // Fragment output: color

float noise(int p) {
    return fract(sin(dot(vec2(p) ,vec2(12.9898,78.233))) * 43758.5453123);
}


void main() {
    vec4 color = texture(texture0, v_uv);
    vec3 noise = vec3(noise(gl_PrimitiveID), noise(gl_PrimitiveID), noise(gl_PrimitiveID));
    color = vec4(noise, 1.0);
    finalColor = color;
}
