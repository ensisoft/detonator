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

#pragma once

#include "config.h"

#include <memory>
#include <cstdint>
#include <string>

#include "graphics/types.h"
#include "graphics/color4f.h"
#include "graphics/bitmap.h"

namespace gfx
{
    class Shader;
    class Program;
    class Geometry;
    class Texture;
    class Framebuffer;

    class Device
    {
    public:
        // Device state including the rasterizer state
        // that is to be applied for any draw operation.
        struct State {
            enum class DepthTest {
                Disabled,
                // Depth test passes and color buffer is updated when the fragments
                // depth value is less or equal to previously written depth value.
                LessOrEQual
            };
            DepthTest depth_test = DepthTest::Disabled;

            // should write color buffer or not.
            bool bWriteColor = true;
            bool premulalpha = false;
            // how to mix the fragment with the existing color buffer value.
            enum class BlendOp {
                None,
                Transparent,
                Additive
            };
            BlendOp blending = BlendOp::None;
            // stencil test
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
            // the stencil action to take on various stencil tests.
            enum class StencilOp {
                DontModify,
                WriteZero,
                WriteRef,
                Increment,
                Decrement
            };
            // the stencil test function.
            StencilFunc  stencil_func  = StencilFunc::Disabled;
            // what to do when the stencil test fails.
            StencilOp    stencil_fail  = StencilOp::DontModify;
            // what to do when the stencil test passes.
            StencilOp    stencil_dpass = StencilOp::DontModify;
            // what to do when the stencil test passes but depth test fails.
            StencilOp    stencil_dfail = StencilOp::DontModify;
            // todo:
            std::uint8_t stencil_mask  = 0xff;
            // todo:
            std::uint8_t stencil_ref   = 0x0;
            // the device viewport into the render target. the viewport is in
            // device coordinates (pixels, texels) and the origin is at the bottom
            // left and Y axis grows upwards (towards the window top)
            IRect viewport;
            // the device scissor that can be used to limit the rendering to the
            // area inside the scissor. if the scissor rect is an empty rect
            // the scissor test is disabled.
            IRect scissor;
            // Which polygon faces to cull. Note that this only applies
            // to polygons, not to lines or points.
            enum class Culling {
                // Don't cull anything, both polygon front and back faces are rasterized
                None,
                // Cull front faces. Front face is determined by the polygon
                // winding order. Currently, counter-clockwise winding is used to
                // indicate front face.
                Front,
                // Cull back faces. This is the default.
                Back,
                // Cull both front and back faces.
                FrontAndBack
            };
            // polygon face culling setting.
            Culling culling = Culling::Back;
            // rasterizer setting for line width when rasterizing
            // geometries with lines.
            float line_width = 1.0f;
        };



        virtual ~Device() = default;

        virtual void ClearColor(const Color4f& color) = 0;
        virtual void ClearStencil(int value) = 0;
        virtual void ClearDepth(float value) = 0;
        virtual void ClearColorDepth(const Color4f& color, float depth) = 0;
        virtual void ClearColorDepthStencil(const Color4f& color, float depth, int stencil) = 0;

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
            // Use mips (precomputed) minified textures.
            // Use the weighted average of the four texture
            // elements that are sampled from the two nearest mipmap levels.
            Trilinear
        };

        // Texture magnifying filter is used whenever the
        // pixel being textured maps to an area less than
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
        virtual Framebuffer* FindFramebuffer(const std::string& name) = 0;
        virtual Framebuffer* MakeFramebuffer(const std::string& name) = 0;
        // Resource deletion APIs
        virtual void DeleteShaders() = 0;
        virtual void DeletePrograms() = 0;
        virtual void DeleteGeometries() = 0;
        virtual void DeleteTextures() = 0;
        virtual void DeleteFramebuffers() = 0;
        // Set the render buffer used for the next Draw, Clear and Read commands.
        // If the fbo is nullptr then set the frame buffer to the default fbo 
        // that is the rendering surface of the context.
        virtual void SetFramebuffer(const Framebuffer* fbo = nullptr) = 0;

        // Draw the given geometry using the given program with the specified state applied.
        virtual void Draw(const Program& program, const Geometry& geometry, const State& state) = 0;

        enum GCFlags {
            Textures   = 0x1,
            Programs   = 0x2,
            Geometries = 0x4
        };

        // Delete GPU resources that are no longer being used and that are
        // eligible for garbage collection (i.e. are marked as okay to delete).
        // Resources that have not been used in the last N frames can be deleted.
        // For example if a texture was last used to render frame N, and we're
        // currently at frame N+max_num_idle_frames then the texture is deleted.
        virtual void CleanGarbage(size_t max_num_idle_frames, unsigned flags) = 0;

        // Prepare the device for the next frame.
        virtual void BeginFrame() = 0;
        // End rendering a frame. If display is true then this will call
        // Context::Display as well as a convenience. If you're still
        // planning to do further rendering/drawing in the same render
        // surface then you should probably pass false for display.
        virtual void EndFrame(bool display = true) = 0;

        // Read the contents of the current render target's color
        // buffer into a bitmap.
        // Width and height specify the dimensions of the data to read.
        // If the dimensions exceed the dimensions of the current render
        // target's color surface then those pixels contents are undefined.
        virtual Bitmap<RGBA> ReadColorBuffer(unsigned width, unsigned height) const = 0;
        virtual Bitmap<RGBA> ReadColorBuffer(unsigned x, unsigned y, unsigned width, unsigned height) const = 0;

        struct ResourceStats {
            std::size_t dynamic_vbo_mem_use   = 0;
            std::size_t dynamic_vbo_mem_alloc = 0;
            std::size_t static_vbo_mem_use    = 0;
            std::size_t static_vbo_mem_alloc  = 0;
            std::size_t streaming_vbo_mem_use = 0;
            std::size_t streaming_vbo_mem_alloc = 0;
        };
        virtual void GetResourceStats(ResourceStats* stats) const = 0;

        struct DeviceCaps {
            unsigned num_texture_units = 0;
            unsigned max_fbo_width = 0;
            unsigned max_fbo_height = 0;
        };
        virtual void GetDeviceCaps(DeviceCaps* caps) const = 0;

        virtual const Framebuffer* GetCurrentFramebuffer() const = 0;
    private:
    };

    class AutoFBO
    {
    public:
        explicit  AutoFBO(Device& device)
          : mDevice(device)
        {
            mCurrent = mDevice.GetCurrentFramebuffer();
        }
        ~AutoFBO()
        {
            mDevice.SetFramebuffer(mCurrent);
        }
        AutoFBO(const AutoFBO&) = delete;
        AutoFBO& operator=(const AutoFBO&) = delete;
    private:
        const Framebuffer* mCurrent = nullptr;
        Device& mDevice;
    };

} // namespace
