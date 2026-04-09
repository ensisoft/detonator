// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include <type_traits>
#include <optional>
#include <vector>
#include <cstdint>
#include <cstring>

#include "base/utility.h"
#include "graphics/enum.h"
#include "graphics/vertex.h"

namespace gfx
{
    class TextureBuffer
    {
    public:
        using Format = gfx::TextureFormat;

        TextureBuffer(std::vector<uint8_t>&& buffer, Format format,
            unsigned width, unsigned height)
            : mBuffer(std::move(buffer))
            , mFormat(format)
            , mWidth(width)
            , mHeight(height)
        {}
        explicit TextureBuffer(const std::vector<uint8_t>& buffer, Format format,
            unsigned width, unsigned height)
            : mBuffer(buffer)
            , mFormat(format)
            , mWidth(width)
            , mHeight(height)
        {}
        explicit TextureBuffer() = default;

        auto GetByteSize() const noexcept
        { return mBuffer.size(); }
        auto GetWidth() const noexcept
        { return mWidth; }
        auto GetHeight() const noexcept
        { return mHeight; }
        auto GetFormat() const noexcept
        { return mFormat; }
        bool IsEmpty() const noexcept
        { return mBuffer.empty(); }
        const void* GetBufferPtr() const noexcept
        { return mBuffer.data(); }
    private:
        std::vector<uint8_t> mBuffer;
        Format mFormat = Format::AlphaMask;
        unsigned mWidth = 0;
        unsigned mHeight = 0;
    };

    template<typename Struct>
    class TypedDataTextureBuffer
    {
    public:
        static_assert(sizeof(Struct) % sizeof(Vec4) == 0);
        static_assert(std::is_standard_layout<Struct>::value);
        static_assert(std::is_trivially_copyable<Struct>::value);

        TypedDataTextureBuffer()
          : mBuffer(&mStorage)
        {}
        explicit TypedDataTextureBuffer(std::vector<uint8_t>* buffer)
          : mBuffer(buffer)
        {}

        void PushBack(Struct value)
        {
            const auto size = mBuffer->size();
            auto& buffer = *mBuffer;
            mBuffer->resize(size + sizeof(value));
            std::memcpy(&buffer[size], &value, sizeof(value));
        }
        auto GetByteSize() const noexcept
        {
            return mBuffer->size();
        }

        auto GetVec4Count() const noexcept
        {
            return mBuffer->size() / sizeof(Vec4);
        }

        auto&& TransferBuffer() noexcept
        {
            return std::move(*mBuffer);
        }

        const auto& GetBuffer() const noexcept
        {
            return *mBuffer;
        }

        const void* GetBufferPtr() const noexcept
        {
            return mBuffer->data();
        }
    private:
        std::vector<uint8_t> mStorage;
        std::vector<uint8_t>* mBuffer = nullptr;
    };

    namespace detail {
        struct TextureSize {
            unsigned width  = 0;
            unsigned height = 0;
        };
        inline std::optional<TextureSize> ChooseDataTextureSize(const size_t vec4_count)
        {
            // one vec4 => one pixel when  the format is RGBA32f
            const auto pixel_count = vec4_count;
            static TextureSize texture_sizes[] = {
                {2, 2}, {2, 4}, {2, 8}, {2, 16}, {2, 32}, {2, 64}, {2, 128},
                {4, 2}, {4, 4}, {4, 8}, {4, 16}, {4, 32}, {4, 64}, {4, 128},
                {8, 2}, {8, 4}, {8, 8}, {8, 16}, {8, 32}, {8, 64}, {8, 128},
            };
            // todo: should we try to maintain aspect ratio close to 1.0 here?

            for (size_t i = 0; i<base::ArraySize(texture_sizes); ++i)
            {
                const auto pixel_space = texture_sizes[i].width * texture_sizes[i].height;
                if (pixel_space >= pixel_count)
                    return texture_sizes[i];
            }
            return std::nullopt;
        }
    } // namespace

    template<typename Struct>
    TextureBuffer PackDataTexture(const TypedDataTextureBuffer<Struct>& data)
    {
        const auto vec4_count = data.GetVec4Count();
        const auto& size = detail::ChooseDataTextureSize(vec4_count);
        if (!size.has_value())
            return TextureBuffer{};

        return TextureBuffer(data.GetBuffer(),
            TextureFormat::RGBA32f, size->width, size->height);
    }

    template<typename Struct>
    TextureBuffer PackDataTexture(TypedDataTextureBuffer<Struct>&& data)
    {
        const auto vec4_count = data.GetVec4Count();
        const auto& size = detail::ChooseDataTextureSize(vec4_count);
        if (!size.has_value())
            return TextureBuffer{};

        return TextureBuffer(data.TransferBuffer(),
            TextureFormat::RGBA32f, size->width, size->height);
    }

} // namespace