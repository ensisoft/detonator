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

    inline Vec2 ToVec(const glm::vec2& vector) noexcept
    { return { vector.x, vector.y }; }
    inline Vec3 ToVec(const glm::vec3& vector) noexcept
    { return { vector.x, vector.y, vector.z }; }
    inline Vec4 ToVec(const glm::vec4& vector) noexcept
    { return { vector.x, vector.y, vector.z, vector.w }; }

    inline glm::vec2 ToVec(const Vec2& vec) noexcept
    { return { vec.x, vec.y }; }
    inline glm::vec3 ToVec(const Vec3& vec) noexcept
    { return { vec.x, vec.y, vec.z }; }
    inline glm::vec4 ToVec(const Vec4& vec) noexcept
    { return { vec.x, vec.y, vec.z, vec.w }; }

    // About texture coordinates.
    // In OpenGL the Y axis for texture coordinates goes so that
    // 0.0 is the first scan-row and 1.0 is the last scan-row of
    // the image. In other words st=0.0,0.0 is the first pixel
    // element of the texture and st=1.0,1.0 is the last pixel.
    // This is also reflected in the scan row memory order in
    // the call glTexImage2D which assumes that the first element
    // (pixel in the memory buffer) is the lower left corner.
    // This however is of course not the same order than what most
    // image loaders produce, they produce data chunks where the
    // first element is the first pixel of the first scan row which
    // is the "top" of the image. So this means that y=1.0f is then
    // the bottom of the image and y=0.0f is the top of the image.
    //
    // Currently all the 2D geometry shapes have "inversed" their
    // texture coordinates so that they use y=0.0f for the top
    // of the shape and y=1.0 for the bottom of the shape which then
    // produces the expected rendering.
    //
    // Complete solution requires making sure that both parts of the
    // system, i.e. the geometry part (drawables) and the material
    // part (which produces the texturing) understand and agree on
    // this.

    // Vertex for 2D drawing on the XY plane.
    struct Vertex2D {
        // Coordinate / position of the vertex in the model space.
        Vec2 aPosition;
        // Texture coordinate for the vertex.
        Vec2 aTexCoord;
    };

    // Vertex for #D drawing in the XYZ space.
    struct Vertex3D {
        // Coordinate / position of the vertex in the model space.
        Vec3 aPosition;
        // Vertex normal
        Vec3 aNormal;
        // Texture coordinate for the vertex.
        Vec2 aTexCoord;
    };

    // The offsetof macro is guaranteed to be usable only with types with standard layout.
    static_assert(std::is_standard_layout<Vertex3D>::value, "Vertex3D must meet standard layout.");
    static_assert(std::is_standard_layout<Vertex2D>::value, "Vertex2D must meet standard layout.");

    // memcpy, binary read/write as-is require trivially_copyable.
    static_assert(std::is_trivially_copyable<Vertex3D>::value, "Vertex3D must be trivial to copy.");
    static_assert(std::is_trivially_copyable<Vertex2D>::value, "Vertex2D must be trivial to copy.");

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

    template<> inline
    const VertexLayout& GetVertexLayout<Vertex3D>()
    {
        static const VertexLayout layout(sizeof(Vertex3D), {
            {"aPosition", 0, 3, 0, offsetof(Vertex3D, aPosition)},
            {"aNormal",   0, 3, 0, offsetof(Vertex3D, aNormal)},
            {"aTexCoord", 0, 2, 0, offsetof(Vertex3D, aTexCoord)}
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

        struct DrawCommand {
            DrawType type = DrawType::Triangles;
            size_t count  = 0;
            size_t offset = 0;
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
