#version 100

precision mediump float;

uniform vec4 kColorA;

void main()
{
  gl_FragColor = kColorA;
}
