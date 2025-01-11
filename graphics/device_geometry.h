// Copyright (C) 2020-2025 Sami Väisänen
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

#include <optional>
#include <string>
#include <vector>
#include <cstddef>

#include "device/enum.h"
#include "device/vertex.h"
#include "device/graphics.h"
#include "graphics/geometry.h"

namespace gfx
{
    class DeviceGeometry : public gfx::Geometry
    {
    public:
        explicit DeviceGeometry(dev::GraphicsDevice* device) noexcept
            : mDevice(device)
        {}
        ~DeviceGeometry() override;

        size_t GetContentHash() const  override
        { return mHash; }
        size_t GetNumDrawCmds() const override
        { return mDrawCommands.size(); }
        DrawCommand GetDrawCmd(size_t index) const override
        { return mDrawCommands[index]; }
        Usage GetUsage() const override
        { return mUsage; }
        std::string GetName() const override
        { return mName; }

        inline void SetBuffer(gfx::GeometryBuffer&& buffer) noexcept
        { mPendingUpload = std::move(buffer); }
        inline void SetUsage(Usage usage) noexcept
        { mUsage = usage; }
        inline void SetDataHash(size_t hash) noexcept
        { mHash = hash; }
        inline void SetName(const std::string& name) noexcept
        { mName = name; }
        inline void SetFrameStamp(size_t frame_number) const noexcept
        { mFrameNumber = frame_number; }
        inline size_t GetFrameStamp() const noexcept
        { return mFrameNumber; }

        inline bool IsEmpty() const noexcept
        { return mVertexBuffer.buffer_bytes == 0; }

        inline size_t GetVertexBufferByteOffset() const noexcept
        { return mVertexBuffer.buffer_offset; }
        inline size_t GetVertexBufferByteSize() const noexcept
        { return mVertexBuffer.buffer_bytes; }

        inline size_t GetIndexBufferByteOffset() const noexcept
        { return mIndexBuffer.buffer_offset; }
        inline size_t GetIndexBufferByteSize() const noexcept
        { return mIndexBuffer.buffer_bytes; }

        inline IndexType GetIndexBufferType() const noexcept
        { return mIndexBufferType; }

        inline bool UsesIndexBuffer() const noexcept
        { return mIndexBuffer.IsValid(); }

        inline const gfx::VertexLayout& GetVertexLayout() const noexcept
        { return mVertexLayout; }

        inline const dev::GraphicsBuffer& GetVertexBuffer() const noexcept
        { return mVertexBuffer; }

        inline const dev::GraphicsBuffer& GetIndexBuffer() const noexcept
        { return mIndexBuffer; }

        inline size_t GetVertexCount() const noexcept
        { return mVertexBuffer.buffer_bytes / mVertexLayout.vertex_struct_size; }

        inline size_t GetIndexCount() const noexcept
        { return mIndexBuffer.buffer_bytes / dev::GetIndexByteSize(mIndexBufferType); }

        inline size_t GetIndexByteSize() const noexcept
        { return dev::GetIndexByteSize(mIndexBufferType); }

        void Upload() const;
    private:
        dev::GraphicsDevice* mDevice = nullptr;
        dev::BufferUsage mUsage = dev::BufferUsage::Static;
        std::size_t mHash = 0;
        std::string mName;

        mutable std::size_t mFrameNumber = 0;
        mutable std::optional<gfx::GeometryBuffer> mPendingUpload;
        mutable std::vector<DrawCommand> mDrawCommands;
        mutable dev::GraphicsBuffer mVertexBuffer;
        mutable dev::GraphicsBuffer mIndexBuffer;
        mutable dev::IndexType mIndexBufferType = dev::IndexType::Index16;
        mutable dev::VertexLayout mVertexLayout;
    };
} // namespace