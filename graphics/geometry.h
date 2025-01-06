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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>

#include "base/assert.h"
#include "graphics/vertex.h"
#include "graphics/types.h"

namespace gfx
{
    using GeometryDataLayout = VertexLayout;

    class GeometryBuffer
    {
    public:
        using IndexType = gfx::IndexType;

        // Define how the geometry is to be rasterized.
        enum class DrawType : uint32_t {
            // Draw the given vertices as triangles, i.e.
            // each 3 vertices make a single triangle.
            Triangles,
            // Draw each given vertex as a separate point
            Points,
            // Draw a series of triangles all connected to the
            // first vertex
            TriangleFan,
            // Draw the vertices as a series of connected lines
            // where each pair of adjacent vertices are connected
            // by a line.
            // In this draw the line width setting applies.
            Lines,
            // Draw a line between the given vertices looping back
            // from the last vertex to the first.
            LineLoop
        };

        // Define how the contents of the geometry object are expected
        // to be used.
        using Usage = BufferUsage;

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

        inline void ClearDraws() noexcept
        { mDrawCmds.clear(); }
        inline void AddDrawCmd(const DrawCommand& cmd)
        {  mDrawCmds.push_back(cmd); }
        inline void SetVertexLayout(const VertexLayout& layout)
        { mVertexLayout = layout; }
        inline size_t GetNumDrawCmds() const noexcept
        { return mDrawCmds.size(); }
        inline size_t GetVertexBytes() const noexcept
        { return mVertexData.size(); }
        inline size_t GetIndexBytes() const noexcept
        { return mIndexData.size(); }
        inline const void* GetVertexDataPtr() const noexcept
        { return mVertexData.empty() ? nullptr : &mVertexData[0]; }
        inline const void* GetIndexDataPtr() const noexcept
        { return mIndexData.empty() ? nullptr : &mIndexData[0]; }
        inline auto& GetLayout() const & noexcept
        { return mVertexLayout; }
        inline auto&& GetLayout() && noexcept
        { return std::move(mVertexLayout); }
        inline IndexType GetIndexType() const noexcept
        { return mIndexType; }
        inline const DrawCommand GetDrawCmd(size_t index) const
        { return mDrawCmds[index]; }

        // Update the geometry object's data buffer contents.
        template<typename Vertex>
        inline void SetVertexBuffer(const Vertex* vertices, std::size_t count)
        { UploadVertices(vertices, count * sizeof(Vertex)); }

        template<typename Vertex>
        inline void SetVertexBuffer(const std::vector<Vertex>& vertices)
        { UploadVertices(vertices.data(), vertices.size() * sizeof(Vertex)); }

        template<typename Vertex>
        inline void SetVertexBuffer(const TypedVertexBuffer<Vertex>& buffer)
        { mVertexData = buffer.CopyRawBuffer(); }

        template<typename Vertex>
        inline void SetVertexBuffer(TypedVertexBuffer<Vertex>&& buffer)
        { mVertexData = std::move(buffer.TransferRawBuffer()); }

        inline void SetVertexBuffer(std::vector<uint8_t>&& data) noexcept
        { mVertexData = std::move(data); }

        inline void SetVertexBuffer(const VertexBuffer& buffer)
        { mVertexData = buffer.CopyBuffer(); }

        inline void SetVertexBuffer(VertexBuffer&& buffer)
        { mVertexData = std::move(buffer.TransferBuffer()); }

        inline void SetIndexBuffer(std::vector<uint8_t>&& data) noexcept
        { mIndexData = std::move(data); }

        inline void SetIndexBuffer(const Index16* indices, size_t count)
        { UploadIndices(indices, count * sizeof(Index16), IndexType::Index16); }

        inline void SetIndexBuffer(const Index32* indices, size_t count)
        { UploadIndices(indices, count * sizeof(Index32), IndexType::Index32); }

        inline void SetIndexBuffer(const std::vector<Index16>& indices)
        { SetIndexBuffer(indices.data(), indices.size()); }

        inline void SetIndexBuffer(const std::vector<Index32>& indices)
        { SetIndexBuffer(indices.data(), indices.size()); }

        // Add a draw command that starts at offset 0 and covers the whole
        // current vertex buffer (i.e. count = num of vertices)
        inline void AddDrawCmd(DrawType type)
        {
            DrawCommand cmd;
            cmd.type   = type;
            cmd.offset = 0;
            cmd.count  = std::numeric_limits<uint32_t>::max();
            AddDrawCmd(cmd);
        }

        // Add a draw command for some particular set of vertices within
        // the current vertex buffer.
        inline void AddDrawCmd(DrawType type, uint32_t offset, size_t count)
        {
            DrawCommand cmd;
            cmd.type   = type;
            cmd.offset = offset;
            cmd.count  = count;
            AddDrawCmd(cmd);
        }

        inline void SetDrawCommands(const std::vector<DrawCommand>& commands)
        { mDrawCmds = commands; }
        inline void SetDrawCommands(std::vector<DrawCommand>&& commands) noexcept
        { mDrawCmds = std::move(commands); }

        inline const auto& GetDrawCommands() const & noexcept
        { return mDrawCmds; }

        inline auto GetDrawCommands() && noexcept
        { return std::move(mDrawCmds); }

        inline bool HasData() const noexcept
        { return !mVertexData.empty(); }

        inline size_t GetVertexCount() const noexcept
        {
            ASSERT(mVertexLayout.vertex_struct_size);
            return mVertexData.size() / mVertexLayout.vertex_struct_size;
        }

        inline const auto& GetVertexBuffer() const noexcept
        { return mVertexData; }

        inline auto& GetVertexBuffer() noexcept
        { return mVertexData; }

    private:
        GeometryDataLayout mVertexLayout;
        std::vector<DrawCommand> mDrawCmds;
        std::vector<uint8_t> mVertexData;
        std::vector<uint8_t> mIndexData;
        IndexType mIndexType = IndexType::Index16;
    };


    // Encapsulate information about a particular geometry and how
    // that geometry is to be rendered and rasterized. A geometry
    // object contains a set of vertex data and then multiple draw
    // commands each command addressing some subset of the vertices.
    class Geometry
    {
    public:
        using Usage       = GeometryBuffer::Usage;
        using DrawType    = GeometryBuffer::DrawType;
        using DrawCommand = GeometryBuffer::DrawCommand;
        using IndexType   = GeometryBuffer::IndexType;

        struct CreateArgs {
            GeometryBuffer buffer;
            // Set the expected usage of the geometry. Should be set before
            // calling any methods to upload the data.
            Usage usage = Usage::Static;
            // Set the (human-readable) name of the geometry.
            // This has debug significance only.
            std::string content_name;
            // Get the hash value based on the buffer contents.
            std::size_t content_hash = 0;
        };

        virtual ~Geometry() = default;

        // Get the human-readable geometry name.
        virtual std::string GetName() const = 0;
        // Get the current usage set on the geometry.
        virtual Usage GetUsage() const = 0;
        // Get the hash value computed from the geometry buffer.
        virtual std::size_t GetContentHash() const  = 0;
        // Get the number of draw commands set on the geometry.
        virtual size_t GetNumDrawCmds() const = 0;
        // Get the draw command at the specified index.
        virtual DrawCommand GetDrawCmd(size_t index) const = 0;
    private:
    };

    using GeometryPtr = std::shared_ptr<const Geometry>;

    void CreateWireframe(const GeometryBuffer& geometry, GeometryBuffer& wireframe);

    enum NormalMeshFlags {
        Normals = 0x1,
        Tangents = 0x2,
        Bitangents = 0x4
    };

    bool CreateNormalMesh(const GeometryBuffer& geometry, GeometryBuffer& normals,
                          unsigned flags = NormalMeshFlags::Normals, float line_length = 0.2f);

    bool ComputeTangents(GeometryBuffer& geometry);

} // namespace
