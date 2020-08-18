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
#include <cstdint>
#include <string>

#include "graphics/types.h"
#include "graphics/bitmap.h"

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

            IRect viewport;
        };

        enum class Type {
            OpenGL_ES2
        };

        // OpenGL graphics context. The context is the interface for the device
        // to resolve the (possibly context specific) OpenGL entry points.
        // This abstraction allows the device to remain agnostic as to
        // what kind of windowing system/graphics subsystem is creating the context
        // and what is the ultimate rendering target (pbuffer, fbo, pixmap or window)
        class Context
        {
        public:
            virtual ~Context() = default;
            // Display the current contents of the rendering target.
            virtual void Display() = 0;
            // Make this context as the current context for the
            // calling thread.
            // Note: In OpenGL all the API functions assume
            // an "implicit" context for the calling thread to be a global
            // object that is set through the window system integration layer
            // i.e. through calling some method on  WGL, GLX, EGL or AGL.
            // So if an application is creating multiple contexts in some thread
            // before starting to use any particular context it has to be
            // made the "current contet". The context contains all the
            // OpenGL API state.
            virtual void MakeCurrent() = 0;
            // Resolve an OpenGL API function to a function pointer.
            // Note: The function pointers can indeed be different for
            // different contexts depending on their specific configuration.
            // Returns a valid pointer or nullptr if there's no such
            // function. (For example an extension function is not available).
            virtual void* Resolve(const char* name) = 0;
        private:
        };

        virtual ~Device() = default;

        virtual void ClearColor(const Color4f& color) = 0;
        virtual void ClearStencil(int value) = 0;

        // Texture minifying filter is used whenever the
        // pixel being textured maps to an area greater than
        // one texture element.
        enum class MinFilter {
            // Use the texture element nearest to the
            // center of the pixel (Manhattan distance)
            Nearest,
            // Use the weighted average of the four texture
            // elements that are closest to the pixel.
            Linear,
            // Use mips (precomputed) minified textures.
            // Use the nearest texture element from the nearest
            // mipmap level
            Mipmap,
            // Use mips (precomputed) minified textures.
            // Use the weighted average of the four texture
            // elements that are sampled from the closest mipmap level.
            Bilinear,
            // Use mips (precomputd minified textures.
            // Use the weigted average of the four texture
            // elements that are sampled from the two nearest mipmap levels.
            Trilinear
        };

        // Texture magnifying filter is used whenver the
        // pixel being textured maps to to an area less than
        // one texture element.
        enum class MagFilter {
            // Use the texture element nearest to the center
            // of the pixel. (Manhattan distance).
            Nearest,
            // Use the weighted average of the four texture
            // elements that are closest to the pixel.
            Linear
        };
        virtual void SetDefaultTextureFilter(MinFilter filter) = 0;
        virtual void SetDefaultTextureFilter(MagFilter filter) = 0;

        // resource creation APIs
        virtual Shader* FindShader(const std::string& name) = 0;
        virtual Shader* MakeShader(const std::string& name) = 0;
        virtual Program* FindProgram(const std::string& name) = 0;
        virtual Program* MakeProgram(const std::string& name) = 0;
        virtual Geometry* FindGeometry(const std::string& name) = 0;
        virtual Geometry* MakeGeometry(const std::string& name) = 0;
        virtual Texture* FindTexture(const std::string& name) = 0;
        virtual Texture* MakeTexture(const std::string& name) = 0;
        // Resource deletion APIs
        virtual void DeleteShaders() = 0;
        virtual void DeletePrograms() = 0;
        virtual void DeleteGeometries() = 0;
        virtual void DeleteTextures() = 0;

        // Draw the given geometry using the given program with the specified state applied.
        virtual void Draw(const Program& program, const Geometry& geometry, const State& state) = 0;

        virtual Type GetDeviceType() const = 0;

        // Delete GPU resources that are no longer being used and that are
        // eligible for garbage collection (i.e. are marked as okay to delete).
        // Resources that have not been used in the last N frames can be deleted.
        // For example if a texture was last used to render frame N and we're
        // currently at frame N+max_num_idle_frames then the texture is deleted.
        virtual void CleanGarbage(size_t max_num_idle_frames) = 0;

        // Prepare the device for the next frame.
        virtual void BeginFrame() = 0;
        // End redering a frame. If display is true then this will call
        // Context::Display as well as a convenience. If you're still
        // planning to do further rendering/drawing in the same render
        // surface then you should probably pass false for display.
        virtual void EndFrame(bool display = true) = 0;

        // Read the contents of the current render target's color
        // buffer into a bitmap.
        // Width and heigth specify the dimensions of the data to read.
        // If the dimensions exceed the dimensions of the current render
        // target's color surface then those pixels contents are undefined.
        virtual Bitmap<RGBA> ReadColorBuffer(unsigned width, unsigned height) const = 0;

        // Create a rendering device of the requested type.
        // Context should be a valid non null context object with the
        // right version.
        // Type = OpenGL_ES2, Context 2.0
        static
        std::shared_ptr<Device> Create(Type type, std::shared_ptr<Context> context);
        static
        std::shared_ptr<Device> Create(Type type, Context* context);
    private:
    };

} // namespace
