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

uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
// todo: maybe pack some of these ?
uniform float kRenderPoints;
uniform float kGamma;
uniform float kRuntime;
uniform float kBlendCoeff;
uniform float kApplyRandomParticleRotation;
// if the textures are alpha masks we sample white
// which gets modulated by the alpha value from the texture.
// x = texture0, y = texture1
uniform vec2 kIsAlphaMask;
// texture coordinate scaling coefficients
uniform vec2 kTextureScale;
// the speed at which S/T coords are transformed.
uniform vec2 kTextureVelocityXY;
uniform float kTextureVelocityZ;
// 0 disabled, 1 clamp, 2 wrap
uniform ivec2 kTextureWrap;
// texture boxes
uniform vec4 kTextureBox0;
uniform vec4 kTextureBox1;

// base color modulated with gradient mix and texture samples (if any)
uniform vec4 kBaseColor;
// gradient base colors.
uniform vec4 kColor0;
uniform vec4 kColor1;
uniform vec4 kColor2;
uniform vec4 kColor3;

// the texture coords from vertex shader.
varying vec2 vTexCoord;
// random per particle value.
varying float vRandomValue;
// per vertex alpha
varying float vAlpha;

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

  if (kTextureWrap.x == 1)
    x = clamp(x, 0.0, box.x);
  else if (kTextureWrap.x == 2)
    x = fract(x / box.x) * box.x;

  if (kTextureWrap.y == 1)
    y = clamp(y, 0.0, box.y);
  else if (kTextureWrap.y == 2)
    y = fract(y / box.y) * box.y;

  return vec2(x, y);
}

vec2 RotateCoords(vec2 coords)
{
    float random_angle = mix(0.0, vRandomValue, kApplyRandomParticleRotation);
    float angle = kTextureVelocityZ * kRuntime + random_angle * 3.1415926;
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
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
    coords = RotateCoords(coords);

    coords += kTextureVelocityXY * kRuntime;
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
    float mask = mix(kIsAlphaMask.x, kIsAlphaMask.y, kBlendCoeff);

    vec4 mix_base_gradient = kBaseColor * gradient;
    // either modulate/mask texture color with base color
    // or modulate base color with texture's alpha value if
    // texture is an alpha mask
    vec4 col = mix(tex * mix_base_gradient, mix_base_gradient * tex.a, mask);
    col.a *= vAlpha;

    // apply gamma (in)correction.
    gl_FragColor = pow(col, vec4(kGamma));
}
