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

// incoming vertex attributes.
// position is the vertex position in model space
attribute vec2 aPosition;
// per particle data.
attribute vec4 aData;
// transform vertex from view space into NDC
// we're using orthographic projection
uniform mat4 kProjectionMatrix;

// transform vertex from model space into viewspace
uniform mat4 kViewMatrix;

// random per particle value.
varying float vRandomValue;
// per particle alpha
varying float vAlpha;

// actuall we don't write tex coords here to the fragment stage
// because gl_PointCoord must be used instead.
varying vec2 vTexCoord;
void main()
{
    // put the vertex from "model" space into same coordinate space
    // with view space. I.e. x grows left and y grows down
    // the model data is defined in the lower right quadrant in
    // NDC (normalied device coordinates) (x grows right to 1.0 and
    // y grows up to 1.0 to the top of the screen)
    vec4 vertex = vec4(aPosition.x, aPosition.y * -1.0, 1.0, 1.0);
    // take the per particle data.
    gl_PointSize = aData.x;
    vRandomValue = aData.y;
    vAlpha       = aData.z;
    // outgoing vertex position.
    gl_Position = kProjectionMatrix * kViewMatrix * vertex;
}