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
uniform vec4 kColorA;
uniform vec4 kColorB;
uniform mat3 kDeviceTextureMatrix;
varying vec2 vTexCoord;

void main()
{
    // for texture coords we need either the coords from the
    // vertex data or gl_PointCoord if the geometry is being
    // rasterized as points.
    // we set kRenderPoints to 1.0f when rendering points.
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    coords = coords * kTextureScale;

    // apply texture box translation.
    vec2 c1 = vec2(kTextureBox0.x + coords.x * kTextureBox0.z,
                   kTextureBox0.y + coords.y * kTextureBox0.w);
    vec2 c2 = vec2(kTextureBox1.x + coords.x * kTextureBox1.z,
                   kTextureBox1.y + coords.y * kTextureBox1.w);
    
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

    // mix between the modulating colors.
    vec4 modk = mix(kColorA, kColorB, kBlendCoeff);

    // either modulate/mask texture color with base color 
    // or modulate base color with texture's alpha value if 
    // texture is an alpha mask
    vec4 col = mix(tex * modk, modk * tex.a, mask);

    // apply gamma (in)correction.
    gl_FragColor = pow(col, vec4(kGamma));
}
