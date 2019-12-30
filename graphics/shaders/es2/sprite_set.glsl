#version 100

precision mediump float;

uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
uniform float kBlendCoeff;
uniform vec4 kBaseColor;
uniform mat3 kDeviceTextureMatrix;
varying vec2 vTexCoord;

void main()
{
    vec3 transformed_coords = kDeviceTextureMatrix * vec3(vTexCoord.xy, 1.0);
    vec2 coords = transformed_coords.xy;
    vec4 tex0   = texture2D(kTexture0, coords);
    vec4 tex1   = texture2D(kTexture1, coords);
    gl_FragColor = mix(tex0, tex1, kBlendCoeff) * kBaseColor;
}
