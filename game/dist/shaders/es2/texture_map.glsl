#version 100

precision mediump float;

uniform sampler2D kTexture;
uniform float kRenderPoints; // 1.0 when rendering points
uniform float kGamma;
uniform vec4  kBaseColor;
uniform mat3  kMatrix;
varying vec2  vTexCoord;

void main()
{
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    vec3 transformed_coords = kMatrix * vec3(coords.xy, 1.0);
    coords = transformed_coords.xy;
    vec4 tex = texture2D(kTexture, coords);
    vec4 col = tex * kBaseColor;
    gl_FragColor = pow(col, vec4(kGamma, kGamma, kGamma, kGamma));
}

