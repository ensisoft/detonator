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

#include <vector>
#include <limits>
#include <cstdint>
#include <cstring> // for memcpy

#include "base/assert.h"
#include "graphics/vertex.h"
#include "graphics/vertex_buffer.h"
#include "graphics/enum.h"

namespace gfx
{
    using GeometryDataLayout = VertexLayout;

    // Geometry buffer contains the geometry data for producing
    // geometries on the GPU. This includes the vertex data and
    // vertex layout, index data and type (if any) and a list of
    // draw commands that refer to the data and describe how it
    // is supposed to be drawn. I.e. what kind of draw primitives
    // which offset and how many elements (vertices, indices etc.)
    // are to be drawn.
    //
    // A geometry buffer can only have a singe vertex format but
    // can have many draw commands.
    class GeometryBuffer
    {
    public:
        using IndexType = gfx::IndexType;
        using DrawType  = gfx::DrawType;
        using Usage     = gfx::BufferUsage;

        // the structure is packed for more compact and simpler
        // serialization purposes
#pragma pack(push, 1)
        struct DrawCommand {
            DrawType type = DrawType::Triangles;
            uint32_t count  = 0;
            uint32_t offset = 0;
        };
#pragma pack(pop)

        void UploadVertices(const void* data, size_t bytes)
        {
            ASSERT((data && bytes) || (!data && !bytes));
            mVertexData.resize(bytes);
            if (data)
                std::memcpy(&mVertexData[0], data, bytes);
        }
        void UploadIndices(const void* data, size_t bytes, IndexType type)
        {
            ASSERT((data && bytes) || (!data && !bytes));
            mIndexData.resize(bytes);
            if (data)
                std::memcpy(&mIndexData[0], data, bytes);
            mIndexType  = type;
        }

        void ClearDraws() noexcept
        { mDrawCmds.clear(); }
        void AddDrawCmd(const DrawCommand& cmd)
        {  mDrawCmds.push_back(cmd); }
        void SetVertexLayout(const VertexLayout& layout)
        { mVertexLayout = layout; }
        auto GetNumDrawCmds() const noexcept
        { return mDrawCmds.size(); }
        auto GetVertexBytes() const noexcept
        { return mVertexData.size(); }
        auto GetIndexBytes() const noexcept
        { return mIndexData.size(); }
        const void* GetVertexDataPtr() const noexcept
        { return mVertexData.empty() ? nullptr : &mVertexData[0]; }
        const void* GetIndexDataPtr() const noexcept
        { return mIndexData.empty() ? nullptr : &mIndexData[0]; }
        auto& GetLayout() const & noexcept
        { return mVertexLayout; }
        auto&& GetLayout() && noexcept
        { return std::move(mVertexLayout); }
        auto GetIndexType() const noexcept
        { return mIndexType; }
        const auto& GetDrawCmd(size_t index) const
        { return mDrawCmds[index]; }

        // Update the geometry object's data buffer contents.
        template<typename Vertex>
        void SetVertexBuffer(const Vertex* vertices, std::size_t count)
        { UploadVertices(vertices, count * sizeof(Vertex)); }

        template<typename Vertex>
        void SetVertexBuffer(const std::vector<Vertex>& vertices)
        { UploadVertices(vertices.data(), vertices.size() * sizeof(Vertex)); }

        template<typename Vertex>
        void SetVertexBuffer(const TypedVertexBuffer<Vertex>& buffer)
        { mVertexData = buffer.CopyRawBuffer(); }

        template<typename Vertex>
        void SetVertexBuffer(TypedVertexBuffer<Vertex>&& buffer)
        { mVertexData = std::move(buffer.TransferRawBuffer()); }

        void SetVertexBuffer(std::vector<uint8_t>&& data) noexcept
        { mVertexData = std::move(data); }

        void SetVertexBuffer(const VertexBuffer& buffer)
        { mVertexData = buffer.CopyBuffer(); }

        void SetVertexBuffer(VertexBuffer&& buffer)
        { mVertexData = std::move(buffer.TransferBuffer()); }

        void SetIndexBuffer(std::vector<uint8_t>&& data) noexcept
        { mIndexData = std::move(data); }

        void SetIndexBuffer(const Index16* indices, size_t count)
        { UploadIndices(indices, count * sizeof(Index16), IndexType::Index16); }

        void SetIndexBuffer(const Index32* indices, size_t count)
        { UploadIndices(indices, count * sizeof(Index32), IndexType::Index32); }

        void SetIndexBuffer(const std::vector<Index16>& indices)
        { SetIndexBuffer(indices.data(), indices.size()); }

        void SetIndexBuffer(const std::vector<Index32>& indices)
        { SetIndexBuffer(indices.data(), indices.size()); }

        // Add a draw command that starts at offset 0 and covers the whole
        // current vertex buffer (i.e. count = num of vertices)
        void AddDrawCmd(DrawType type)
        {
            DrawCommand cmd;
            cmd.type   = type;
            cmd.offset = 0;
            cmd.count  = std::numeric_limits<uint32_t>::max();
            AddDrawCmd(cmd);
        }

        // Add a draw command for some particular set of vertices within
        // the current vertex buffer.
        void AddDrawCmd(DrawType type, uint32_t offset, size_t count)
        {
            DrawCommand cmd;
            cmd.type   = type;
            cmd.offset = offset;
            cmd.count  = count;
            AddDrawCmd(cmd);
        }

        void SetDrawCommands(const std::vector<DrawCommand>& commands)
        { mDrawCmds = commands; }
        void SetDrawCommands(std::vector<DrawCommand>&& commands) noexcept
        { mDrawCmds = std::move(commands); }

        const auto& GetDrawCommands() const & noexcept
        { return mDrawCmds; }

        auto GetDrawCommands() && noexcept
        { return std::move(mDrawCmds); }

        bool HasVertexData() const noexcept
        { return !mVertexData.empty(); }
        bool HasIndexData() const noexcept
        { return !mIndexData.empty(); }

        auto GetVertexCount() const noexcept
        {
            ASSERT(mVertexLayout.vertex_struct_size);
            return mVertexData.size() / mVertexLayout.vertex_struct_size;
        }

        const auto& GetVertexBuffer() const noexcept
        { return mVertexData; }

        auto& GetVertexBuffer() noexcept
        { return mVertexData; }

        size_t GetHash() noexcept;

    private:
        GeometryDataLayout mVertexLayout;
        std::vector<DrawCommand> mDrawCmds;
        std::vector<uint8_t> mVertexData;
        std::vector<uint8_t> mIndexData;
        IndexType mIndexType = IndexType::Index16;
    };

} // namespace