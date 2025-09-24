// Copyright (C) 2020-2025o Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#include <type_traits>

#include "device/enum.h"

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
            bool IsValid() const noexcept
            { return handle != 0; }
            auto GetHandle() const noexcept
            { return handle; }
            auto GetType() const noexcept
            { return type; }
            auto GetFormat() const noexcept
            { return format; }

            TextureType type = TextureType::Invalid;
            TextureFormat format = TextureFormat::sRGBA;
            unsigned handle = 0;
            unsigned texture_width  = 0;
            unsigned texture_height = 0;
            unsigned texture_depth  = 0;
            unsigned texture_array_size = 0;
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
            using HandleType = ResourceHandle<type>;
            static_assert(std::is_standard_layout<HandleType>::value,
                          "Device graphics resource handle must have standard layout.");
            return std::memcmp(&lhs, &rhs) == 0;
        }
        template<dev::ResourceType type>
        inline bool operator!=(const ResourceHandle<type>& lhs, const ResourceHandle<type> rhs)
        {
            using HandleType = ResourceHandle<type>;
            static_assert(std::is_standard_layout<HandleType>::value,
                          "Device graphics resource handle must have standard layout.");
            return std::memcmp(&lhs, &rhs) != 0;
        }

    } // detail
} // namespace