// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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

#pragma once

#include "config.h"

#include <vector>

namespace gfx
{
    class Shader;
    class Texture;
    class Color4f;

    // Program object interface. Program objects are device
    // specific graphics programs that are built from shaders
    // and then uploaded and executed on the device.
    class Program
    {
    public:
        virtual ~Program() = default;

        // Build the program from the given list of shaders.
        // Returns true if the build was succesful.
        // Failed build will leave the program object in the previous state.
        // Trying to build from shaders that are not valid is considered a bug.
        virtual bool Build(const std::vector<const Shader*>& shaders) = 0;

        // Returns true if the program is valid or not I.e. it has been
        // succesfully build and can be used for drawing.
        virtual bool IsValid() const = 0;

        // Set uniforms in the program object.
        // Uniforms are normally set before using the program to perform
        // any drawing/rendering operations.
        // Each uniform is identified by it's name in the shader sources.
        // If the uniform doesn't actually exist in the shader
        // (for example because of dead code elimination) or by user's modification
        // of the shader the call is silently ignored.

        // Set scalar uniform.
        virtual void SetUniform(const char* name, int x) = 0;
        // Set scalar uniform.
        virtual void SetUniform(const char* name, int x, int y) = 0;

        // Set scalar uniform.
        virtual void SetUniform(const char* name, float x) = 0;
        // Set vec2 uniform.
        virtual void SetUniform(const char* name, float x, float y) = 0;
        // Set vec3 uniform.
        virtual void SetUniform(const char* name, float x, float y, float z) = 0;
        // Set vec4 uniform.
        virtual void SetUniform(const char* name, float x, float y, float z, float w) = 0;
        // set Color uniform
        virtual void SetUniform(const char* name, const Color4f& color) = 0;
        // Matrix memory layout is as follows:
        // {xx xy xz}
        // {yx yy yz}
        // {zx zy zz}
        // or {{xx xy xz}, {yx yy yz}, {zx zy zz}}
        // In other words the first 3 floats are the X vector followed by the
        // Y vector followed by the Z vector.

        using Matrix2x2 = float[2][2];
        using Matrix3x3 = float[3][3];
        using Matrix4x4 = float[4][4];

        // set 2x2 matrix uniform.
        virtual void SetUniform(const char* name, const Matrix2x2& matrix) = 0;
        // Set 3x3 matrix uniform.
        virtual void SetUniform(const char* name, const Matrix3x3& matrix) = 0;
        // Set 4x4 matrix uniform.
        virtual void SetUniform(const char* name, const Matrix4x4& matrix) = 0;

        // Set a texture sampler.
        // Sampler is the name of the texture sampler in the shader.
        // It's possible to sample multiple textures in the program by setting each
        // texture to a different texture unit.
        virtual void SetTexture(const char* sampler, unsigned unit, const Texture& texture) = 0;
        // Set the number of textures used by the next draw.
        // Todo: this api and the SetTexture are potentially bug prone, it'd be better to
        // combine both into a single api call that takes the whole array of textures.
        virtual void SetTextureCount(unsigned count) = 0;
    private:
    };

} //