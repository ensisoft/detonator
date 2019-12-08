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

#include <memory>
#include <vector>
#include <cstdint>
#include <string>

#include "types.h"

namespace gfx
{
    class Shader;
    class Program;
    class Geometry;
    class Texture;
    class Color4f;

    class Device
    {
    public:
        struct State {
            bool bEnablePointSize   = false;
            bool bEnablePointSprite = false;
            bool bWriteColor = true;

            enum class BlendOp {
                None,
                Transparent,
                Additive
            };
            BlendOp blending = BlendOp::None;

            enum class StencilFunc {
                Disabled,
                PassAlways,
                PassNever,
                RefIsLess,
                RefIsLessOrEqual,
                RefIsMore,
                RefIsMoreOrEqual,
                RefIsEqual,
                RefIsNotEqual
            };
            enum class StencilOp {
                DontModify,
                WriteZero,
                WriteRef,
                Increment,
                Decrement
            };
            StencilFunc  stencil_func  = StencilFunc::Disabled;
            StencilOp    stencil_fail  = StencilOp::DontModify;
            StencilOp    stencil_dpass = StencilOp::DontModify;
            StencilOp    stencil_dfail = StencilOp::DontModify;
            std::uint8_t stencil_mask  = 0xff;
            std::uint8_t stencil_ref   = 0x0;

            Rect viewport;
        };

        enum class Type {
            OpenGL_ES2
        };

        virtual ~Device() = default;

        virtual void ClearColor(const Color4f& color) = 0;
        virtual void ClearStencil(int value) = 0;

        // resource creation APIs
        virtual Shader* FindShader(const std::string& name) = 0;
        virtual Shader* MakeShader(const std::string& name) = 0;
        virtual Program* FindProgram(const std::string& name) = 0;
        virtual Program* MakeProgram(const std::string& name) = 0;
        virtual Geometry* FindGeometry(const std::string& name) = 0;
        virtual Geometry* MakeGeometry(const std::string& name) = 0;
        virtual Texture* FindTexture(const std::string& name) = 0;
        virtual Texture* MakeTexture(const std::string& name) = 0;

        virtual void DeleteShaders() = 0;
        virtual void DeletePrograms() = 0;

        // Draw the given geometry using the given program with the specified state applied.
        virtual void Draw(const Program& program, const Geometry& geometry, const State& state) = 0;

        virtual Type GetDeviceType() const = 0;

        using StateBuffer = std::vector<uint8_t>;

        virtual void GetState(StateBuffer* state) const = 0;

        virtual void SetState(const StateBuffer& state) = 0;

        // Delete GPU resources that are no longer being used and that are
        // eligible for garbage collection (i.e. are marked as okay to delete).
        // Resources that have not been used in the last N frames can be deleted.
        // For example if a texture was last used to render frame N and we're
        // currently at frame N+max_num_idle_frames then the texture is deleted.
        virtual void CleanGarbage(size_t max_num_idle_frames) = 0;

        // Prepare the device for the next frame.
        virtual void BeginFrame() = 0;
        // End redering a frame.
        virtual void EndFrame() = 0;

        static
        std::shared_ptr<Device> Create(Type type);
    private:
    };

} // namespace
