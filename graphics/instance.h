// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <cstring> // for memcpy
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

#include "base/assert.h"
#include "graphics/vertex.h"
#include "graphics/types.h"

namespace gfx
{
    using GeometryInstanceDataLayout = VertexLayout;

    // A CPU buffer for (geometry) instance data, containing InstanceDataLayout
    // and per geometry instance
    class GeometryInstanceBuffer
    {
    public:
        // Define how the contents of the instance buffer are used
        using Usage = BufferUsage;

        void AddInstanceData(const void* data, size_t size)
        {
            ASSERT(size == mLayout.vertex_struct_size);
            const auto offset = mVertexData.size();
            mVertexData.resize(mVertexData.size() + size);
            std::memcpy(&mVertexData[offset], data, size);
        }

        template<typename InstanceAttr>
        void AddInstanceData(const InstanceAttr& attr)
        { AddInstanceData(&attr, sizeof(attr)); }

        void SetInstanceBuffer(const void* ptr, size_t bytes)
        {
            ASSERT(ptr && bytes);
            mVertexData.resize(bytes);
            std::memcpy(&mVertexData[0], ptr, bytes);
        }

        template<typename InstanceAttr>
        void SetInstanceBuffer(const InstanceAttr* data, std::size_t count)
        { SetInstanceBuffer((const void*)data, count * sizeof(InstanceAttr)); }

        template<typename InstanceAttr>
        inline void SetInstanceBuffer(const std::vector<InstanceAttr>& data)
        { SetInstanceBuffer(data.data(), data.size()); }

        inline void SetInstanceBuffer(std::vector<uint8_t>&& buffer) noexcept
        { mVertexData = std::move(buffer); }
        inline void SetInstanceBuffer(const std::vector<uint8_t>& buffer)
        { mVertexData = buffer; }
        inline void Clear()
        {
            mVertexData.clear();
            mLayout = GeometryInstanceDataLayout{};
        }
        inline bool IsValid() const noexcept
        {
            if (mLayout.vertex_struct_size == 0)
                return false;
            if (mVertexData.empty())
                return false;
            if (mVertexData.size() % mLayout.vertex_struct_size)
                return false;
            return true;
        }
        inline bool IsEmpty() const noexcept
        {  return mVertexData.empty(); }

        inline size_t GetInstanceCount() const noexcept
        {
            ASSERT(mLayout.vertex_struct_size);
            return mVertexData.size() / mLayout.vertex_struct_size;
        }
        inline void SetVertexLayout(const GeometryInstanceDataLayout& layout)
        { mLayout = layout; }
        inline size_t GetVertexBytes() const noexcept
        { return mVertexData.size(); }
        inline const void* GetVertexDataPtr() const noexcept
        { return mVertexData.data(); }

        inline const auto& GetLayout() const & noexcept
        { return mLayout; }
        inline auto&& GetLayout() const && noexcept
        { return std::move(mLayout); }
    private:
        GeometryInstanceDataLayout mLayout;
        std::vector<uint8_t> mVertexData;
    };

    // Per geometry instance vertex data.
    class GeometryInstance
    {
    public:
        using Usage = GeometryInstanceBuffer::Usage;
        struct CreateArgs {
            GeometryInstanceBuffer buffer;
            // The expected usage of the geometry instance data.
            Usage usage = Usage::Stream;
            // Set the (human-readable) name of the instance geometry.
            // This has debug significance only.
            std::string content_name;
            // Set the hash value based on the contents of the buffer.
            std::size_t content_hash = 0;
        };
    private:
    };

    using GeometryInstancePtr = std::shared_ptr<const GeometryInstance>;

} // namespace