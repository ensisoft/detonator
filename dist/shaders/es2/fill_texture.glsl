#version 100

precision mediump float;

uniform sampler2D kTexture;
uniform float kRenderPoints; // 1.0 when rendering points

varying vec2 vTexCoord;

void main()
{
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    gl_FragColor = texture2D(kTexture, coords);
}

