#version 100

precision mediump float;

uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
uniform float kBlendCoeff;
varying vec2 vTexCoord;

void main()
{
    vec2 coords = vTexCoord;
    vec4 tex0   = texture2D(kTexture0, coords);
    vec4 tex1   = texture2D(kTexture1, coords);
    gl_FragColor = mix(tex0, tex1, kBlendCoeff);
}
