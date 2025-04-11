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

#include "device/types.h"
#include "device/graphics.h"
#include "graphics/types.h"
#include "graphics/color4f.h"
#include "graphics/bitmap.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/instance.h"
#include "graphics/framebuffer.h"

namespace gfx
{
    class Shader;
    class Program;
    class Geometry;
    class GeometryDrawCommand;
    class Texture;
    class Framebuffer;

    class Device
    {
    public:
        using RasterState = dev::RasterState;
        using ViewportState = dev::ViewportState;
        using ColorDepthStencilState = dev::ColorDepthStencilState;
        using ResourceStats = dev::GraphicsDeviceResourceStats;
        using DeviceCaps = dev::GraphicsDeviceCaps;
        using StateName = dev::StateName;
        using StateValue = dev::StateValue;
        using DepthTest = dev::DepthTest;

        using ColorAttachment = gfx::Framebuffer::ColorAttachment;

        virtual ~Device() = default;

        virtual void ClearColor(const Color4f& color, Framebuffer* fbo, ColorAttachment attachment) const = 0;
        virtual void ClearStencil(int value, Framebuffer* fbo) const = 0;
        virtual void ClearDepth(float value, Framebuffer* fbo) const = 0;
        virtual void ClearColorDepth(const Color4f& color, float depth, Framebuffer* fbo, ColorAttachment attachment) const = 0;
        virtual void ClearColorDepthStencil(const Color4f& color, float depth, int stencil, Framebuffer* fbo, ColorAttachment attachment) const = 0;

        void ClearColor(const Color4f& color) const
        {
            ClearColor(color, nullptr, ColorAttachment::Attachment0);
        }
        void ClearColor(const Color4f& color, Framebuffer* fbo) const
        {
            ClearColor(color, fbo, ColorAttachment::Attachment0);
        }
        void ClearStencil(int value) const
        {
            ClearStencil(value, nullptr);
        }
        void ClearDepth(float value) const
        {
            ClearDepth(value, nullptr);
        }
        void ClearColorDepth(const Color4f& color, float depth) const
        {
            ClearColorDepth(color, depth, nullptr, ColorAttachment::Attachment0);
        }
        void ClearColorDepthStencil(const Color4f& color, float depth, int stencil) const
        {
            ClearColorDepthStencil(color, depth, stencil, nullptr, ColorAttachment::Attachment0);
        }

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
        virtual ShaderPtr FindShader(const std::string& id) = 0;
        virtual ShaderPtr CreateShader(const std::string& id, const Shader::CreateArgs& args) = 0;
        virtual ProgramPtr FindProgram(const std::string& id) = 0;
        virtual ProgramPtr CreateProgram(const std::string& id, const Program::CreateArgs& args) = 0;
        virtual GeometryPtr FindGeometry(const std::string& id) = 0;
        virtual GeometryPtr CreateGeometry(const std::string& id, Geometry::CreateArgs args) = 0;
        virtual InstancedDrawPtr FindInstancedDraw(const std::string& id) = 0;
        virtual InstancedDrawPtr CreateInstancedDraw(const std::string& id, InstancedDraw::CreateArgs args) = 0;
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
        virtual void DeleteFramebuffer(const std::string& id) = 0;
        virtual void DeleteTexture(const std::string& id) = 0;

        using StateKey = unsigned;

        virtual StateKey PushState() = 0;
        virtual void PopState(StateKey key) = 0;
        virtual void SetViewportState(const ViewportState& state) const = 0;
        virtual void SetColorDepthStencilState(const ColorDepthStencilState& state) const = 0;
        virtual void ModifyState(const StateValue& value, StateName name) const = 0;

        // Draw the given geometry using the given program with the specified state applied.
        virtual void Draw(const Program& program, const ProgramState& program_state,
                          const GeometryDrawCommand& geometry, const RasterState& state, Framebuffer* fbo = nullptr) = 0;

        enum GCFlags {
            Textures   = 0x1,
            Programs   = 0x2,
            Geometries = 0x4,
            FBOs       = 0x8,
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
        virtual Bitmap<Pixel_RGBA> ReadColorBuffer(unsigned width, unsigned height, Framebuffer* fbo = nullptr) const = 0;
        virtual Bitmap<Pixel_RGBA> ReadColorBuffer(unsigned x, unsigned y, unsigned width, unsigned height, Framebuffer* fbo = nullptr) const = 0;

        virtual void GetResourceStats(ResourceStats* stats) const = 0;
        virtual void GetDeviceCaps(DeviceCaps* caps) const = 0;
    private:
    };

    class DeviceState
    {
    public:
        using ViewportState = Device::ViewportState;
        using ColorDepthStencilState = Device::ColorDepthStencilState;

        explicit DeviceState(Device& device)
          : mDevice(device)
        {
            mKey = mDevice.PushState();
        }
        explicit DeviceState(Device* device) : DeviceState(*device)
        {}

        ~DeviceState()
        {
            mDevice.PopState(mKey);
        }

        void SetState(const ViewportState& vs)
        {
            mDevice.SetViewportState(vs);
        }
        void SetState(const ColorDepthStencilState& state)
        {
            mDevice.SetColorDepthStencilState(state);
        }

        DeviceState(const DeviceState&) = delete;
        DeviceState& operator=(const DeviceState&) = delete;
    private:
        Device& mDevice;
        Device::StateKey mKey;
    };


    std::shared_ptr<Device> CreateDevice(std::shared_ptr<dev::GraphicsDevice> device);
    std::shared_ptr<Device> CreateDevice(dev::GraphicsDevice* device);

} // namespace
