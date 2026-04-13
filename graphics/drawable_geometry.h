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

#include "graphics/texture.h"
#include "graphics/geometry.h"
#include "graphics/geometry_buffer.h"
#include "graphics/texture_buffer.h"

namespace gfx
{
    class Device;

    class DrawGeometryHandle
    {
    public:
        static DrawGeometryHandle Null;

        DrawGeometryHandle(GeometryPtr geometry) noexcept
          : mGeometry(std::move(geometry))
        {}
        DrawGeometryHandle(GeometryPtr geometry, const Texture* texture) noexcept
          : mGeometry(std::move(geometry))
         , mTexture(texture)
        {}
        DrawGeometryHandle() = default;

        bool IsNull() const noexcept
        { return mGeometry == nullptr; }

        bool IsFallback() const noexcept
        { return mGeometry && mGeometry->IsFallback(); }

        bool IsValid() const noexcept
        { return mGeometry && !mGeometry->IsFallback(); }

        const GeometryPtr& GetGeometry() const noexcept
        { return mGeometry; }

        const Texture* GetTexture() const noexcept
        { return mTexture; }

        std::string GetErrorLog() const noexcept
        {
            if (mGeometry)
                return mGeometry->GetErrorLog();
            return "";
        }
        std::string GetName() const noexcept
        {
            if (mGeometry)
                return mGeometry->GetName();
            return "";
        }

        static DrawGeometryHandle CreateErrorGeometry(const std::string& id,
            std::string message, std::string name,
            std::size_t content_hash, Device& device);
    private:
        GeometryPtr mGeometry;
        const Texture* mTexture = nullptr;
    };

    class DrawGeometryBuffer
    {
    public:
        static DrawGeometryBuffer Null;

        DrawGeometryBuffer(GeometryBuffer buffer) noexcept
          : mGeometryBuffer(std::move(buffer))
        {}
        DrawGeometryBuffer(GeometryBuffer geometry, TextureBuffer texture) noexcept
         : mGeometryBuffer(std::move(geometry))
         , mTextureBuffer(std::move(texture))
        {}

        DrawGeometryBuffer() noexcept
          : mNull(true)
        {}

        bool IsNull() const noexcept
        { return mNull; }

        const GeometryBuffer& GetGeometryBuffer() const noexcept
        { return mGeometryBuffer; }

        const TextureBuffer& GetTextureBuffer() const noexcept
        {  return mTextureBuffer; }

        GeometryBuffer&& TransferGeometryBuffer() noexcept
        {
            return std::move(mGeometryBuffer);
        }
        TextureBuffer&& TransferTextureBuffer() noexcept
        {
            return std::move(mTextureBuffer);
        }
    private:
        GeometryBuffer mGeometryBuffer;
        TextureBuffer mTextureBuffer;
        bool mNull = false;
    };

} // namespace
