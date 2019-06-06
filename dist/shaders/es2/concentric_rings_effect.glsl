#version 100

precision mediump float;

uniform float uRuntime;
uniform vec4 kBaseColor;
varying vec2 vTexCoord;


void main()
{
    float time = 1.4;
    float s = mod(uRuntime / time, time) / time;

    float rings[4];
    rings[0] = 0.0 + s;
    rings[1] = 0.08 + s;
    rings[2] = 0.16 + s;
    rings[3] = 0.24 + s;
    float ring_width = 0.005;

    vec2 pos   = vTexCoord - vec2(0.5, 0.5);
    vec2 ori   = vec2(0.0, 0.0);
    float dist = length(pos - ori);

    bool found = false;
    for (int i=0; i<4; ++i) {
        if (dist >= rings[i] && dist <= rings[i] + ring_width) {
            found = true;
            break;
        }
    }
    if (!found)
       discard;

    float alpha = 1.0 - dist/ 0.5 * 1.0;
    gl_FragColor = kBaseColor * vec4(1.0, 1.0, 1.0, alpha);
}