#version 100

precision mediump float;

uniform sampler2D kTexture;

varying vec2 vTexCoord;

void main()
{
    gl_FragColor = texture2D(kTexture, vTexCoord);
}

