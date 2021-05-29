// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#version 100

precision highp float;

uniform vec4 kBaseColor;
// todo: put into an array
uniform vec4 kColor0;
uniform vec4 kColor1;
uniform vec4 kColor2;
uniform vec4 kColor3;

uniform float kGamma;
uniform float kRenderPoints;

varying vec2 vTexCoord;
// per vertex alpha.
varying float vAlpha;

vec4 MixGradient(vec2 coords)
{
  vec4 top = mix(kColor0, kColor1, coords.x);
  vec4 bot = mix(kColor2, kColor3, coords.x);
  vec4 color = mix(top, bot, coords.y) * kBaseColor;
  color = pow(color, vec4(2.2));
  return color;
}

void main()
{
  vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
  vec4 color  = MixGradient(coords);
  color.a *= vAlpha;
  gl_FragColor = pow(color, vec4(kGamma));
}
