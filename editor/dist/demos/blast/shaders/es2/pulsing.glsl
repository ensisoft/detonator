
#version 100

precision highp float;

uniform float kRuntime;
uniform float kRenderPoints;
uniform float kGamma;
uniform vec4  kBaseColor;

void main()
{
    float time = 1.2;
    float s = mod(kRuntime, time) / time;

    float a = sin(s*3.147) * 0.5 + 0.5;

    gl_FragColor = kBaseColor * a * .8;
    //gl_FragColor.a = a * 0.8;
}
