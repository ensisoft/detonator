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

precision mediump float;

uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
uniform float kRenderPoints;
uniform float kGamma;
uniform float kBlendCoeff;
// if the textures are alpha masks we sample white
// which gets modulated by the alpha value from the texture.
uniform float kIsAlphaMask0;
uniform float kIsAlphaMask1;
// texture coordinate scaling coefficients
uniform vec2 kTextureScale;

uniform vec4 kTextureBox0;
uniform vec4 kTextureBox1;
uniform vec4 kBaseColor;
uniform mat3 kDeviceTextureMatrix;
varying vec2 vTexCoord;

vec2 SamplePointCoord()
{
    // "However, the gl_PointCoord fragment shader input defines
    // a per-fragment coordinate space (s, t) where s varies from
    // 0 to 1 across the point horizontally left-to-right, and t
    // ranges from 0 to 1 across the point vertically top-to-bottom."
    return vec2(gl_PointCoord.x, 1.0-gl_PointCoord.y);
}

void main()
{
    // for texture coords we need either the coords from the
    // vertex data or gl_PointCoord if the geometry is being
    // rasterized as points.
    // we set kRenderPoints to 1.0f when rendering points.
    vec2 coords = mix(vTexCoord, SamplePointCoord(), kRenderPoints);
    coords = coords * kTextureScale;

    // apply texture box transformation.
    vec2 scale_tex0 = kTextureBox0.zw;
    vec2 scale_tex1 = kTextureBox1.zw;
    vec2 trans_tex0 = kTextureBox0.xy;
    vec2 trans_tex1 = kTextureBox1.xy;

    vec2 c1 = coords * scale_tex0 + trans_tex0;
    vec2 c2 = coords * scale_tex1 + trans_tex1;

    // apply device specific texture transformation.
    vec3 c1_transformed = kDeviceTextureMatrix * vec3(c1.xy, 1.0);
    vec3 c2_transformed = kDeviceTextureMatrix * vec3(c2.xy, 1.0);
    c1 = c1_transformed.xy;
    c2 = c2_transformed.xy;

    // sample textures, if texture is a just an alpha mask we use
    // only the alpha channel later.
    vec4 tex0 = texture2D(kTexture0, c1);
    vec4 tex1 = texture2D(kTexture1, c2);
    vec4 tex  = mix(tex0, tex1, kBlendCoeff);

    // mix mask values. (makes sense?)
    float mask = mix(kIsAlphaMask0, kIsAlphaMask1, kBlendCoeff);

    // either modulate/mask texture color with base color
    // or modulate base color with texture's alpha value if
    // texture is an alpha mask
    vec4 col = mix(tex * kBaseColor, kBaseColor * tex.a, mask);

    // apply gamma (in)correction.
    gl_FragColor = pow(col, vec4(kGamma));
}
