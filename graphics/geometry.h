// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include <cstddef>
#include <vector>
#include <string>
#include <algorithm>

namespace gfx
{
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
    struct Vertex {
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

    class VertexBuffer
    {
    public:
        virtual ~VertexBuffer() = default;
        virtual bool IsCpuBuffer() const = 0;
        virtual const void* GetRawPtr() const = 0;
        virtual size_t GetCount() const = 0;
    private:
    };

    namespace detail {
        template<typename Vertex>
        class VertexCpuBuffer : public VertexBuffer
        {
        public:
            VertexCpuBuffer(const Vertex* vertices, size_t count)
            {
                std::copy(vertices, vertices+count, std::back_inserter(mData));
            }
            VertexCpuBuffer(const std::vector<Vertex>& vertices) : mData(vertices)
            {}
            VertexCpuBuffer(std::vector<Vertex>&& vertices) : mData(std::move(vertices))
            {}
            virtual bool IsCpuBuffer() const
            { return true; }
            virtual const void* GetRawPtr() const override
            { return &mData[0]; }
            virtual size_t GetCount() const override
            { return mData.size(); }
        private:
            std::vector<Vertex> mData;
        };
    } // namespace

    // Encapsulate information about a particular geometry
    // and how how that geometry is to be rendered and
    // rasterized. A geometry object contains a set of vertex
    // data and then multiple draw commands each command
    // addressing some subset of the vertices.
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

        virtual ~Geometry() = default;
        // Clear previous draw commands.
        virtual void ClearDraws() = 0;
        // Add a draw command that starts at offset 0 and covers the whole
        // current vertex buffer (i.e. count = num of vertices)
        virtual void AddDrawCmd(DrawType type) = 0;
        // Add a draw command for some particular set of vertices within
        // the current vertex buffer.
        virtual void AddDrawCmd(DrawType type, size_t offset, size_t count) = 0;
        // Set the vertex buffer that contains the vertex data for this geometry.
        virtual void SetVertexBuffer(std::unique_ptr<VertexBuffer> buffer) = 0;
        // Set the layout object that describes the contents of the vertex buffer vertices.
        virtual void SetVertexLayout(const VertexLayout& layout) = 0;
        // Update the geometry object's data buffer contents.
        template<typename Vertex>
        void SetVertexBuffer(const Vertex* vertices, std::size_t count)
        {
            SetVertexBuffer(std::make_unique<detail::VertexCpuBuffer<Vertex>>(vertices, count));
            // for compatibility sakes set the vertex layout here.
            if constexpr (std::is_same_v<Vertex, gfx::Vertex>)
                SetVertexLayout(GetVertexLayout());
        }
        // Update the geometry objects' data buffer contents
        // with the given vector of data.
        template<typename Vertex>
        void SetVertexBuffer(const std::vector<Vertex>& vertices)
        {
            SetVertexBuffer(std::make_unique<detail::VertexCpuBuffer<Vertex>>(vertices));
            // for compatibility sakes set the vertex layout here.
            if constexpr (std::is_same_v<Vertex, gfx::Vertex>)
                SetVertexLayout(GetVertexLayout());
        }
        // Update the geometry object's data buffer contents
        // by moving the contents of vertices into geometry object.
        template<typename Vertex>
        void SetVertexBuffer(std::vector<Vertex>&& vertices)
        {
            SetVertexBuffer(std::make_unique<detail::VertexCpuBuffer<Vertex>>(std::move(vertices)));
            // for compatibility sakes set the vertex layout here.
            if constexpr (std::is_same_v<Vertex, gfx::Vertex>)
                SetVertexLayout(GetVertexLayout());
        }
        static const VertexLayout& GetVertexLayout()
        {
            // todo: if using GLSL layout bindings then need to
            // specify the vertex attribute indices properly.
            // todo: if using instanced rendering then need to specify
            // the divisors properly.
            static VertexLayout layout(sizeof(Vertex), {
               {"aPosition", 0, 2, 0, offsetof(Vertex, aPosition)},
               {"aTexCoord", 0, 2, 0, offsetof(Vertex, aTexCoord)}
            });
            return layout;
        }
    private:
    };
} // namespace
