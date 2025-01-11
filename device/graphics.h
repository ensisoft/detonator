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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#  include <glm/mat4x4.hpp>
#  include <glm/mat3x3.hpp>
#  include <glm/mat2x2.hpp>
#  include <glm/gtc/type_ptr.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <variant>

#include "base/color4f.h"
#include "device/enum.h"
#include "device/types.h"

namespace dev
{
    namespace detail {
        template<dev::ResourceType type>
        struct ResourceHandle {
            inline bool IsValid() const noexcept
            { return handle != 0; }
            inline auto GetHandle() const noexcept
            { return handle; }

            unsigned handle = 0;
        };

        template<>
        struct ResourceHandle<dev::ResourceType::GraphicsShader> {
            inline bool IsValid() const noexcept
            { return handle != 0; }
            inline auto GetHandle() const noexcept
            { return handle; }
            inline auto GetType() const noexcept
            { return type; }

            unsigned handle = 0;
            ShaderType type = ShaderType::Invalid;
        };

        template<>
        struct ResourceHandle<dev::ResourceType::GraphicsBuffer> {
            inline bool IsValid() const noexcept
            { return handle != 0; }
            inline auto GetHandle() const noexcept
            { return handle; }
            inline auto GetType() const noexcept
            { return type; }

            unsigned handle = 0;
            BufferType type = BufferType::Invalid;
            size_t buffer_index  = 0;
            size_t buffer_offset = 0;
            size_t buffer_bytes  = 0;
        };

        template<>
        struct ResourceHandle<dev::ResourceType::Texture> {
            inline bool IsValid() const noexcept
            { return handle != 0; }
            inline auto GetHandle() const noexcept
            { return handle; }
            inline auto GetType() const noexcept
            { return type; }

            unsigned handle = 0;
            TextureType type = TextureType::Invalid;
            TextureFormat format = TextureFormat::sRGBA;
            size_t texture_width  = 0;
            size_t texture_height = 0;
            size_t texture_depth  = 0;
        };

        template<>
        struct ResourceHandle<dev::ResourceType::FrameBuffer> {
            inline bool IsValid() const noexcept
            { return handle != 0xffffffff; }
            inline auto GetHandle() const noexcept
            { return handle; }
            inline auto GetFormat() const noexcept
            { return format; }
            inline bool IsDefault() const noexcept
            { return handle == 0; }
            inline bool IsCustom() const noexcept
            { return handle > 0; }
            inline bool IsMultisampled() const noexcept
            { return samples > 1; }

            unsigned handle = 0xffffffff;
            FramebufferFormat format = FramebufferFormat::Invalid;
            size_t width = 0;
            size_t height = 0;
            unsigned samples = 0;
        };

        template<dev::ResourceType type>
        inline bool operator==(const ResourceHandle<type>& lhs, const ResourceHandle<type>& rhs) noexcept
        {
            return lhs.handle == rhs.handle;
        }
        template<dev::ResourceType type>
        inline bool operator!=(const ResourceHandle<type>& lhs, const ResourceHandle<type> rhs)
        {
            return lhs.handle != rhs.handle;
        }

    } // detail

    struct ProgramState {
        struct Uniform {
            std::string name;
            std::variant<int, float,
                    base::Color4f,
                    glm::ivec2,
                    glm::vec2,
                    glm::vec3,
                    glm::vec4,
                    glm::mat2,
                    glm::mat3,
                    glm::mat4> value;
        };
        std::vector<const Uniform*> uniforms;
    };

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