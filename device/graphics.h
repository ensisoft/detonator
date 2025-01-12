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

#include <string>
#include <vector>
#include <variant>

#include "base/color4f.h"
#include "device/handle.h"
#include "device/enum.h"
#include "device/types.h"

namespace dev
{
    using GraphicsShader  = detail::ResourceHandle<ResourceType::GraphicsShader>;
    using GraphicsProgram = detail::ResourceHandle<ResourceType::GraphicsProgram>;
    using GraphicsBuffer  = detail::ResourceHandle<ResourceType::GraphicsBuffer>;
    using TextureObject   = detail::ResourceHandle<ResourceType::Texture>;
    using Framebuffer     = detail::ResourceHandle<ResourceType::FrameBuffer>;

    struct VertexLayout;

    class GraphicsDevice
    {
    public:
        enum class MipStatus {
            Success, Error,
            UnsupportedSize,
            UnsupportedFormat,
        };
        struct BindWarnings {
            bool force_clamp_x = false;
            bool force_clamp_y = false;
            bool force_min_linear = false;
        };

        virtual Framebuffer GetDefaultFramebuffer() const = 0;
        virtual Framebuffer CreateFramebuffer(const FramebufferConfig& config) = 0;

        virtual void AllocateRenderTarget(const Framebuffer& framebuffer, unsigned color_attachment,
                                          unsigned width, unsigned height) = 0;
        virtual void BindRenderTargetTexture2D(const Framebuffer& framebuffer, const TextureObject& texture,
                                               unsigned color_attachment) = 0;
        virtual bool CompleteFramebuffer(const Framebuffer& framebuffer,
                                         const std::vector<unsigned>& color_attachments) = 0;
        virtual void ResolveFramebuffer(const Framebuffer& multisampled_framebuffer, const TextureObject& resolve_target,
                                        unsigned color_attachment) = 0;
        virtual void BindFramebuffer(const Framebuffer& framebuffer) const = 0;
        virtual void DeleteFramebuffer(const Framebuffer& fbo) = 0;

        virtual GraphicsShader CompileShader(const std::string& source, ShaderType type, std::string* compile_info) = 0;

        virtual GraphicsProgram BuildProgram(const std::vector<GraphicsShader>& shaders, std::string* build_info) = 0;

        virtual TextureObject AllocateTexture2D(unsigned texture_width,
                                                unsigned texture_height, TextureFormat format) = 0;
        virtual TextureObject UploadTexture2D(const void* bytes,
                                              unsigned texture_width,
                                              unsigned texture_height, TextureFormat format) = 0;
        virtual MipStatus GenerateMipmaps(const TextureObject& texture) = 0;

        virtual bool BindTexture2D(const TextureObject& texture, const GraphicsProgram& program, const std::string& sampler_name,
                                   unsigned texture_unit, TextureWrapping texture_x_wrap, TextureWrapping texture_y_wrap,
                                   TextureMinFilter texture_min_filter, TextureMagFilter texture_mag_filter, BindWarnings* warnings) const = 0;

        virtual void DeleteTexture(const TextureObject& texture) = 0;

        virtual GraphicsBuffer AllocateBuffer(size_t bytes, BufferUsage usage, BufferType type) = 0;
        virtual void FreeBuffer(const GraphicsBuffer& buffer) = 0;
        virtual void UploadBuffer(const GraphicsBuffer& buffer, const void* data, size_t bytes) = 0;

        virtual void BindVertexBuffer(const GraphicsBuffer& buffer, const GraphicsProgram& program, const VertexLayout& layout) const = 0;
        virtual void BindIndexBuffer(const GraphicsBuffer& buffer) const = 0;

        virtual void SetPipelineState(const GraphicsPipelineState& state) const = 0;
        virtual void SetProgramState(const GraphicsProgram& program, const ProgramState& state) const = 0;
        virtual void BindProgramBuffer(const GraphicsProgram& program, const GraphicsBuffer& buffer,
                                       const std::string& interface_block_name, unsigned binding_index) = 0;

        virtual void DeleteShader(const GraphicsShader& shader) = 0;
        virtual void DeleteProgram(const GraphicsProgram& program) = 0;

        // draw with vertex and index buffer
        virtual void Draw(DrawType draw_primitive, IndexType index_type,
                          unsigned primitive_count, unsigned index_buffer_byte_offset, unsigned instance_count) const = 0;
        virtual void Draw(DrawType draw_primitive, IndexType index_type,
                          unsigned primitive_count, unsigned index_buffer_byte_offset) const = 0;

        // draw with vertex buffer
        virtual void Draw(DrawType draw_primitive, unsigned vertex_start_index,
                          unsigned vertex_draw_count, unsigned instance_count) const = 0;
        virtual void Draw(DrawType draw_primitive, unsigned vertex_start_index, unsigned vertex_draw_count) const = 0;

        virtual void ClearColor(const base::Color4f& color, const Framebuffer& fbo, ColorAttachment attachment) const = 0;
        virtual void ClearStencil(int value, const Framebuffer& fbo) const = 0;
        virtual void ClearDepth(float value, const Framebuffer& fbo) const = 0;
        virtual void ClearColorDepth(const base::Color4f& color, float depth, const Framebuffer& fbo, ColorAttachment attachment) const = 0;
        virtual void ClearColorDepthStencil(const base::Color4f& color, float depth, int stencil, const Framebuffer& fbo, ColorAttachment attachment) const = 0;

        virtual void ReadColor(unsigned width, unsigned height, const Framebuffer& fbo, void* color_data) const = 0;
        virtual void ReadColor(unsigned x, unsigned y, unsigned width, unsigned  height,
                               const Framebuffer& fbo, void* color_data) const = 0;

        virtual void GetResourceStats(GraphicsDeviceResourceStats* stats) const = 0;
        virtual void GetDeviceCaps(GraphicsDeviceCaps* caps) const = 0;

        virtual void BeginFrame()  = 0;
        virtual void EndFrame(bool display) = 0;

    protected:
        virtual ~GraphicsDevice() = default;
    };
} // namespace