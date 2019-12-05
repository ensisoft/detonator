#version 100

precision mediump float;

uniform sampler2D kTexture;
uniform float kRenderPoints; // 1.0 when rendering points
uniform float kGamma;
uniform vec4  kBaseColor;
varying vec2  vTexCoord;

void main()
{
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    vec4 tex = texture2D(kTexture, coords);
    vec4 col = kBaseColor * tex.r;
    gl_FragColor = pow(col, vec4(kGamma, kGamma, kGamma, kGamma));
}
