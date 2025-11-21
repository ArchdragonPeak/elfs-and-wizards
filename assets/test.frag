#version 430
precision mediump float;

layout(std430, binding=0) buffer EaseLUT
{
    float values[];
};


uniform sampler2D texture0;
uniform float u_time;

in vec2 fragTexCoord;       // Fragment input attribute: texture coordinate
in vec4 fragColor;          // Fragment input attribute: color
out vec4 finalColor;        // Fragment output: color
uniform vec2 resolution;


void main() {
    vec4 color = texture(texture0, fragTexCoord);
    vec2 pos = gl_FragCoord.xy;
    vec2 center = vec2(resolution.x*0.8, resolution.y*0.7);
    float d = distance(pos, center);

    float radius = 1200.0;
    float dNorm = 1.0 - clamp(d / radius, 0.0, 1.0);

    int up = int(ceil(dNorm*15.0));
    int down = int(floor(dNorm*15.0));
    float fraction = fract(dNorm*15.0);
    
    dNorm = mix(values[down], values[up], fraction);
    //dNorm = (values[down] + values[up]) / 2.0;
    
    if(color.b > 150./255.) {
        color = mix(color, vec4(1.0, 1.0, 0.0, 1.0), dNorm);
    } else {
        color = mix(color, vec4(1.0, 1.0, 0.0, 1.0), dNorm/1.3);
    }

    finalColor = color;
}
