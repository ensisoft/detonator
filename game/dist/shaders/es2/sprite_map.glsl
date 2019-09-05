#version 100

precision mediump float;

uniform sampler2D kTexture;
uniform float kBlendCoeff;
uniform vec4 kTextureFrame0;
uniform vec4 kTextureFrame1;
uniform vec4 kBaseColor;
varying vec2 vTexCoord;

void main()
{
    vec2 c1 = vec2(kTextureFrame0.x + vTexCoord.x * kTextureFrame0.z,
                   kTextureFrame0.y + vTexCoord.y * kTextureFrame0.w);
    vec2 c2 = vec2(kTextureFrame1.x + vTexCoord.x * kTextureFrame1.z,
                   kTextureFrame1.y + vTexCoord.y * kTextureFrame1.w);
    vec4 tex0 = texture2D(kTexture, c1);
    vec4 tex1 = texture2D(kTexture, c2);
    gl_FragColor = mix(tex0, tex1, kBlendCoeff) * kBaseColor;
}