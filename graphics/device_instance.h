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
#include <cstddef>

#include "device/enum.h"
#include "device/graphics.h"
#include "graphics/instance.h"

namespace gfx
{
    class DeviceDrawInstanceBuffer : public gfx::InstancedDraw
    {
    public:
        explicit DeviceDrawInstanceBuffer(dev::GraphicsDevice* device) noexcept
          : mDevice(device)
        {}
       ~DeviceDrawInstanceBuffer() override;

        std::size_t GetContentHash() const override
        { return mContentHash; }
        std::string GetContentName() const override
        { return mContentName; }
        void SetContentHash(size_t hash) override
        { mContentHash = hash; }
        void SetContentName(std::string name) override
        { mContentName = std::move(name); }

        inline void SetBuffer(gfx::InstancedDrawBuffer&& buffer) noexcept
        { mPendingUpload = std::move(buffer); }
        inline void SetUsage(Usage usage) noexcept
        { mUsage = usage; }
        inline void SetFrameStamp(size_t frame_number) const noexcept
        { mFrameNumber = frame_number; }
        inline size_t GetFrameStamp() const noexcept
        { return mFrameNumber; }
        inline size_t GetVertexBufferByteOffset() const noexcept
        { return mBuffer.buffer_offset; }
        inline size_t GetVertexBufferIndex() const noexcept
        { return mBuffer.buffer_index; }
        inline size_t GetInstanceCount() const noexcept
        { return mBuffer.buffer_bytes / mLayout.vertex_struct_size; }
        inline const gfx::InstanceDataLayout& GetVertexLayout() const noexcept
        { return mLayout; }
        inline const dev::GraphicsBuffer& GetVertexBuffer() const noexcept
        { return mBuffer; }

        void Upload() const;

    private:
        dev::GraphicsDevice* mDevice = nullptr;
        std::size_t mContentHash = 0;
        std::string mContentName;
        dev::BufferUsage mUsage = gfx::BufferUsage::Static;
        mutable std::size_t mFrameNumber = 0;
        mutable std::optional<gfx::InstancedDrawBuffer> mPendingUpload;
        mutable gfx::InstanceDataLayout mLayout;
        mutable dev::GraphicsBuffer  mBuffer;
    };


} // namespace