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

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>

#include "base/assert.h"

namespace gfx
{
    // 16bit vertex index for indexed drawing.
    using Index16 = std::uint16_t;
    // 32bit vertex index for indexed drawing.
    using Index32 = std::uint32_t;

    // 2 float vector data object. Use glm::vec2 for math.
    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;
    };
    // 3 float vector data object. Use glm::vec3 for math.
    struct Vec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };
    // 4 float vector data object. Use glm::vec4 for math.
    struct Vec4 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
    };

    // Vertex for 2D drawing on the XY plane.
    struct Vertex2D {
        // Coordinate / position of the vertex in the model space.
        // Model space is a 2D space on XY plane where X varies from
        // 0.0f to 1.0f (left to right when mapped onto render target)
        // and Y varies from 0.0f to -1.0f ( from top to bottom when
        // mapped onto render target).
        Vec2 aPosition;
        // Texture coordinate for the vertex. Texture coordinates
        // are normalized and can vary beyond 0.0f-1.0f range in
        // which case they either get clamped or wrapped.
        // Texture coordinates on the X axis vary from 0.0f to 1.0f
        // i.e. from left to right on horizontal axis.
        // Texture coordinates on the Y axis vary from 0.0f to 1.0f
        // i.e. from top to bottom on the vertical axis. (0.0f is
        // the *top* row of pixels and 1.0f is the *bottom* row of
        // pixels).
        Vec2 aTexCoord;
    };

    struct VertexLayout {
        struct Attribute {
            // name of the attribute in the shader code.
            std::string name;
            // the index of the attribute.
            // use glsl syntax
            // layout (binding=x) in vec3 myAttrib;
            unsigned index = 0;
            // number of vector components
            // must be one of [1, 2, 3, 4]
            unsigned num_vector_components = 0;
            // the attribute divisor. if this is 0 the
            // attribute updates for every vertex and instancing is off.
            // ignored for geometry attributes.
            unsigned divisor = 0;
            // relative offset in the vertex data
            // typically offsetof(MyVertex, member)
            std::size_t offset = 0;
        };
        std::size_t vertex_struct_size = 0;
        std::vector<Attribute> attributes;

        VertexLayout() = default;
        VertexLayout(std::size_t struct_size,
                     std::initializer_list<Attribute> attrs)
                     : vertex_struct_size(struct_size)
                     , attributes(std::move(attrs))
        {}
    };

    template<typename Vertex>
    const VertexLayout& GetVertexLayout();

    template<> inline
    const VertexLayout& GetVertexLayout<Vertex2D>()
    {
        // todo: if using GLSL layout bindings then need to
        // specify the vertex attribute indices properly.
        // todo: if using instanced rendering then need to specify
        // the divisors properly.
        static const VertexLayout layout(sizeof(Vertex2D), {
            {"aPosition", 0, 2, 0, offsetof(Vertex2D, aPosition)},
            {"aTexCoord", 0, 2, 0, offsetof(Vertex2D, aTexCoord)}
        });
        return layout;
    }

    // Encapsulate information about a particular geometry and how
    // that geometry is to be rendered and rasterized. A geometry
    // object contains a set of vertex data and then multiple draw
    // commands each command addressing some subset of the vertices.
    class Geometry
    {
    public:
        // Define how the geometry is to be rasterized.
        enum class DrawType {
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
        enum class Usage {
            // The geometry is updated once and drawn multiple times.
            Static,
            // The geometry is updated multiple times and drawn once/few times.
            Stream,
            // The buffer is updated multiple times and drawn multiple times.
            Dynamic
        };
        enum class IndexType {
            Index16, Index32
        };

        virtual ~Geometry() = default;
        // Clear previous draw commands.
        virtual void ClearDraws() = 0;
        // Add a draw command that starts at offset 0 and covers the whole
        // current vertex buffer (i.e. count = num of vertices)
        virtual void AddDrawCmd(DrawType type) = 0;
        // Add a draw command for some particular set of vertices within
        // the current vertex buffer.
        virtual void AddDrawCmd(DrawType type, size_t offset, size_t count) = 0;
        // Set the layout object that describes the contents of the vertex buffer vertices.
        virtual void SetVertexLayout(const VertexLayout& layout) = 0;
        // Upload the vertex data that defines the geometry.
        virtual void UploadVertices(const void* data, size_t bytes, Usage usage = Usage::Static) = 0;
        // Upload 16bit index data for indexed drawing.
        virtual void UploadIndices(const void* data, size_t bytes, IndexType type, Usage usage = Usage::Static) = 0;
        // Set the hash value that identifies the data.
        virtual void SetDataHash(size_t hash) = 0;
        // Get the hash value that was used in the latest data upload.
        virtual size_t GetDataHash() const  = 0;

        // Update the geometry object's data buffer contents.
        template<typename Vertex>
        void SetVertexBuffer(const Vertex* vertices, std::size_t count, Usage usage = Usage::Static)
        {
            UploadVertices(vertices, count * sizeof(Vertex), usage);
            // for compatibility sakes set the vertex layout here.
            if constexpr (std::is_same_v<Vertex, gfx::Vertex2D>)
                SetVertexLayout(GetVertexLayout<Vertex>());
        }

        template<typename Vertex> inline
        void SetVertexBuffer(const std::vector<Vertex>& vertices, Usage usage = Usage::Static)
        { SetVertexBuffer(vertices.data(), vertices.size(), usage); }

        void SetIndexBuffer(const Index16* indices, size_t count, Usage usage = Usage::Static)
        { UploadIndices(indices, count * sizeof(Index16), IndexType::Index16, usage); }
        void SetIndexBuffer(const Index32* indices, size_t count, Usage usage = Usage::Static)
        { UploadIndices(indices, count * sizeof(Index32), IndexType::Index32, usage); }
        void SetIndexBuffer(const std::vector<Index16>& indices, Usage usage = Usage::Static)
        { SetIndexBuffer(indices.data(), indices.size(), usage); }
        void SetIndexBuffer(const std::vector<Index32>& indices, Usage usage = Usage::Static)
        { SetIndexBuffer(indices.data(), indices.size(), usage); }

        // Map the type of the index to index size in bytes.
        static size_t GetIndexByteSize(IndexType type)
        {
            if (type == IndexType::Index16)
                return 2;
            else if (type == IndexType::Index32)
                return 4;
            else BUG("Missing index type.");
            return 0;
        }
    private:
    };
} // namespace
