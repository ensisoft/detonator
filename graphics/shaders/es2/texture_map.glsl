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

precision highp float;

uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
// todo: maybe pack some of these ?
uniform float kRenderPoints;
uniform float kGamma;
uniform float kRuntime;
uniform float kBlendCoeff;
// if the textures are alpha masks we sample white
// which gets modulated by the alpha value from the texture.
uniform float kIsAlphaMask0;
uniform float kIsAlphaMask1;
// texture coordinate scaling coefficients
uniform vec2 kTextureScale;
// the speed at which S/T coords are transformed.
uniform vec2 kTextureVelocity;

uniform vec4 kTextureBox0;
uniform vec4 kTextureBox1;
uniform vec4 kBaseColor;
varying vec2 vTexCoord;

uniform vec4 kColor0;
uniform vec4 kColor1;
uniform vec4 kColor2;
uniform vec4 kColor3;

// 0 disabled, 1 clamp, 2 wrap
uniform int kTextureWrapX;
uniform int kTextureWrapY;

vec4 MixGradient(vec2 coords)
{
  vec4 top = mix(kColor0, kColor1, coords.x);
  vec4 bot = mix(kColor2, kColor3, coords.x);
  vec4 color = mix(top, bot, coords.y);
  color = pow(color, vec4(2.2));
  return color;
}

// Support texture coordinate wrapping (clamp or repeat)
// for cases when hardware texture sampler setting is
// insufficient, i.e. when sampling from a sub rectangle
// in a packed texture. (or whenever we're using texture rects)
// This however can introduce some sampling artifacts depending
// on fhe filter.
// TODO: any way to fix those artifacs ?
vec2 WrapTextureCoords(vec2 coords, vec2 box)
{
  float x = coords.x;
  float y = coords.y;

  if (kTextureWrapX == 1)
    x = clamp(x, 0.0, box.x);
  else if (kTextureWrapX == 2)
    x = fract(x / box.x) * box.x;

  if (kTextureWrapY == 1)
    y = clamp(y, 0.0, box.y);
  else if (kTextureWrapY == 2)
    y = fract(y / box.y) * box.y;

  return vec2(x, y);
}

void main()
{

    // for texture coords we need either the coords from the
    // vertex data or gl_PointCoord if the geometry is being
    // rasterized as points.
    // we set kRenderPoints to 1.0f when rendering points.
    // note about gl_PointCoord:
    // "However, the gl_PointCoord fragment shader input defines
    // a per-fragment coordinate space (s, t) where s varies from
    // 0 to 1 across the point horizontally left-to-right, and t
    // ranges from 0 to 1 across the point vertically top-to-bottom."
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    vec4 gradient = MixGradient(coords);

    coords += kTextureVelocity * kRuntime;
    coords = coords * kTextureScale;

    // apply texture box transformation.
    vec2 scale_tex0 = kTextureBox0.zw;
    vec2 scale_tex1 = kTextureBox1.zw;
    vec2 trans_tex0 = kTextureBox0.xy;
    vec2 trans_tex1 = kTextureBox1.xy;

    // scale and transform based on texture box. (todo: maybe use texture matrix?)
    vec2 c1 = WrapTextureCoords(coords * scale_tex0, scale_tex0) + trans_tex0;
    vec2 c2 = WrapTextureCoords(coords * scale_tex1, scale_tex1) + trans_tex1;


    // sample textures, if texture is a just an alpha mask we use
    // only the alpha channel later.
    vec4 tex0 = texture2D(kTexture0, c1);
    vec4 tex1 = texture2D(kTexture1, c2);
    vec4 tex  = mix(tex0, tex1, kBlendCoeff);

    // mix mask values. (makes sense?)
    float mask = mix(kIsAlphaMask0, kIsAlphaMask1, kBlendCoeff);

    vec4 mix_base_gradient = kBaseColor * gradient;
    // either modulate/mask texture color with base color
    // or modulate base color with texture's alpha value if
    // texture is an alpha mask
    vec4 col = mix(tex * mix_base_gradient, mix_base_gradient * tex.a, mask);

    // apply gamma (in)correction.
    gl_FragColor = pow(col, vec4(kGamma));
}
