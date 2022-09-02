
#version 100

precision highp float;

uniform float kTime;
uniform float kRenderPoints;

uniform sampler2D kTexture0;
uniform float kGamma;
uniform vec4  kBaseColor;

varying vec2 vTexCoord;

float SlidingGlint()
{
    vec2 uv = vTexCoord;
    float time = 1.4;
    float cycle = mod(kTime / time, time) / time;
    float glint_width = 0.6;
    float left  = -1.0 + cycle * 2.0 + uv.y * -0.1;
    float right = left + glint_width;
    if (!(uv.x > left && uv.x < right))
        return 0.0;

    float d = (uv.x - left) / glint_width;
    float v = sin(radians(180.0*d));
    return v;
}

void main()
{
    float time = 1.2;
    float s = mod(kTime, time) / time;
    float p = sin(s*3.147) * 0.5 + 0.5;
    float a = texture2D(kTexture0, vTexCoord).w;
    gl_FragColor.rgb = kBaseColor.rgb * a + vec3(1.0) * SlidingGlint() * a;
}