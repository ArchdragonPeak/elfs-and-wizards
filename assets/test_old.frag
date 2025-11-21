#version 430
precision mediump float;

layout(std430, binding=0) buffer EaseLUT
{
    float values[];
};


uniform sampler2D texture0;

in vec2 fragTexCoord;       // Fragment input attribute: texture coordinate
in vec4 fragColor;          // Fragment input attribute: color
out vec4 finalColor;        // Fragment output: color
uniform vec2 resolution;


void main() {
    vec4 color = texture(texture0, fragTexCoord);
    vec2 pos = gl_FragCoord.xy;
    vec2 center = vec2(resolution.x*0.7, resolution.y*0.7);
    float d = distance(pos, center);

    float radius = 400.0;
    float dNorm = clamp(d / radius, 0.0, 1.0);
    dNorm = smoothstep(0.0, 0.5, dNorm);

    if(color.b > 150./255.) {
        color = mix(color, vec4(1.0), 1.0 - dNorm);
    } else {
        color = mix(color, vec4(1.0), clamp(1.0 - (dNorm*2.0), 0.0, 0.9));
    }

    finalColor = color;
}