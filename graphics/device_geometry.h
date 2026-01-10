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

#include <string>
#include <vector>
#include <cstddef>
#include <memory>

#include "device/enum.h"
#include "device/vertex.h"
#include "device/graphics.h"
#include "graphics/geometry.h"

namespace gfx
{
    class DeviceGeometry final : public Geometry
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
        std::string GetErrorLog() const override
        { return mErrorLog; }
        bool IsFallback() const override
        { return mFallback; }

        void SetBuffer(std::shared_ptr<const GeometryBuffer> buffer) noexcept
        { mPendingUpload = std::move(buffer); }
        void SetUsage(Usage usage) noexcept
        { mUsage = usage; }
        void SetDataHash(size_t hash) noexcept
        { mHash = hash; }
        void SetName(std::string name) noexcept
        { mName = std::move(name); }
        void SetErrorLog(std::string log) noexcept
        { mErrorLog = std::move(log); }
        void SetFrameStamp(size_t frame_number) const noexcept
        { mFrameNumber = frame_number; }
        void SetAsFallback(bool fallback)
        { mFallback = fallback; }
        size_t GetFrameStamp() const noexcept
        { return mFrameNumber; }

        bool IsEmpty() const noexcept
        { return mVertexBuffer.buffer_bytes == 0; }

        size_t GetVertexBufferByteOffset() const noexcept
        { return mVertexBuffer.buffer_offset; }
        size_t GetVertexBufferByteSize() const noexcept
        { return mVertexBuffer.buffer_bytes; }

        size_t GetIndexBufferByteOffset() const noexcept
        { return mIndexBuffer.buffer_offset; }
        size_t GetIndexBufferByteSize() const noexcept
        { return mIndexBuffer.buffer_bytes; }

        IndexType GetIndexBufferType() const noexcept
        { return mIndexBufferType; }

        bool UsesIndexBuffer() const noexcept
        { return mIndexBuffer.IsValid(); }

        const VertexLayout& GetVertexLayout() const noexcept
        { return mVertexLayout; }

        const dev::GraphicsBuffer& GetVertexBuffer() const noexcept
        { return mVertexBuffer; }

        const dev::GraphicsBuffer& GetIndexBuffer() const noexcept
        { return mIndexBuffer; }

        size_t GetVertexCount() const noexcept
        { return mVertexBuffer.buffer_bytes / mVertexLayout.vertex_struct_size; }

        size_t GetIndexCount() const noexcept
        { return mIndexBuffer.buffer_bytes / dev::GetIndexByteSize(mIndexBufferType); }

        size_t GetIndexByteSize() const noexcept
        { return dev::GetIndexByteSize(mIndexBufferType); }

        void Upload() const;
    private:
        dev::GraphicsDevice* mDevice = nullptr;
        dev::BufferUsage mUsage = BufferUsage::Static;
        std::size_t mHash = 0;
        std::string mName;
        std::string mErrorLog;
        bool mFallback = false;

        mutable std::size_t mFrameNumber = 0;
        mutable std::shared_ptr<const GeometryBuffer> mPendingUpload;
        mutable std::vector<DrawCommand> mDrawCommands;
        mutable dev::GraphicsBuffer mVertexBuffer;
        mutable dev::GraphicsBuffer mIndexBuffer;
        mutable dev::IndexType mIndexBufferType = IndexType::Index16;
        mutable dev::VertexLayout mVertexLayout;
    };
} // namespace