#version 100

precision mediump float;

uniform vec4 kColor;

void main()
{
  gl_FragColor = kColor;
}
