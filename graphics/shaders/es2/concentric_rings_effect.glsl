#version 100

precision mediump float;

uniform float uRuntime;
uniform vec4 kBaseColor;
varying vec2 vTexCoord;

float ring_alpha(float ring_distance_from_origin, float fragment_distance_from_origin)
{
    float ring_width = 0.005;    
    float rd = ring_distance_from_origin;
    float fd = fragment_distance_from_origin;
    return smoothstep(rd - ring_width, rd, fd) - smoothstep(rd, rd + ring_width, fd);
}

void main()
{
    float time = 1.4;
    float s = mod(uRuntime / time, time) / time;

    float rings[4];
    rings[0] = 0.0 + s;
    rings[1] = 0.08 + s;
    rings[2] = 0.16 + s;
    rings[3] = 0.24 + s;

    vec2 pos   = vTexCoord - vec2(0.5, 0.5);
    vec2 ori   = vec2(0.0, 0.0);
    float dist = length(pos - ori);

    float ring_alpha0 = ring_alpha(rings[0], dist);
    float ring_alpha1 = ring_alpha(rings[1], dist);
    float ring_alpha2 = ring_alpha(rings[2], dist);
    float ring_alpha3 = ring_alpha(rings[3], dist);
    float any_ring_alpha = ring_alpha0 + ring_alpha1 + ring_alpha2 + ring_alpha3;

    float distance_alpha = 1.0 - dist/ 0.5 * 1.0;
    gl_FragColor = kBaseColor * vec4(1.0, 1.0, 1.0, distance_alpha * any_ring_alpha);
}
