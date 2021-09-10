#version 100

precision highp float;

uniform float kRuntime;
uniform vec4 kTextureRect;
uniform sampler2D kTexture;
varying vec2 vTexCoord;

vec2 TransformTexCoords(vec2 coord)
{
    vec2 trans = kTextureRect.xy;
    vec2 scale = kTextureRect.zw;
    return coord * scale + trans;
}

void main()
{
    vec2 uv = TransformTexCoords(vTexCoord);

    float time  = 2.4;
    float cycle = mod(kRuntime / time, time) / time;
    float s  = sin(cycle * 3.14159*2.0);
    vec4 tex = texture2D(kTexture, uv);

    gl_FragColor = tex * 0.6 + (s * 0.2 * tex.a);
}


