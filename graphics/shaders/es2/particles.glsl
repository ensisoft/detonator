// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

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