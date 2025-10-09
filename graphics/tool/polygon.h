// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <cstring> // for memcpy

#include "graphics/geometry.h"
#include "graphics/vertex.h"

// the stuff here is *NOT NEEDED* at runtime. The builder could
// be placed somwhere else for example under the editor itself but
// because the graphics test app also needs this type of functionality
// the builder is here.

namespace gfx {
    class PolygonMeshClass;

namespace tool {

    class IPolygonBuilder
    {
    public:
        using DrawCommand = gfx::Geometry::DrawCommand;

        virtual ~IPolygonBuilder() = default;
        virtual void ClearAll() noexcept = 0;
        virtual void ClearDrawCommands() noexcept = 0;
        virtual void ClearVertices() noexcept = 0;

        // Erase the vertex at the given index.
        virtual void EraseVertex(size_t index) = 0;

        // Update the vertex at the given index.
        virtual void UpdateVertex(const void* vertex, size_t index) = 0;

        // Insert a vertex into the vertex array where the index is
        // an index within the given draw command. Index can be in
        // the range [0, cmd.count].
        // After the new vertex has been inserted the list of draw
        // commands is then modified to include the new vertex.
        // I.e. the draw command that includes the
        // new vertex will grow its count by 1 and all draw commands
        // that come after the will have their starting offsets
        // incremented by 1.
        virtual void InsertVertex(const void* vertex, size_t cmd_index, size_t index) = 0;

        virtual void AppendVertex(const void* vertex) = 0;

        virtual void AddDrawCommand(const DrawCommand& cmd) = 0;

        virtual void UpdateDrawCommand(const DrawCommand& cmd, size_t index) noexcept = 0;

        // Find the draw command that contains the vertex at the given index.
        // Returns index to the draw command.
        virtual size_t FindDrawCommand(size_t vertex_index) const noexcept = 0;

        // Compute a hash value based on the content only, i.e. the
        // vertices and the draw commands.
        // The hash value is used to realize changes done to a polygon
        // with dynamic content and then to re-upload the data to the GPU.
        virtual size_t GetContentHash() const noexcept = 0;

        virtual size_t GetVertexCount() const noexcept = 0;

        virtual size_t GetCommandCount() const noexcept = 0;

        virtual const DrawCommand& GetDrawCommand(size_t index ) const = 0;

        virtual void GetVertex(void* vertex, size_t index) const noexcept = 0;

        virtual const void* GetVertexPtr(size_t vertex_index) const noexcept = 0;

        virtual void* GetVertexPtr(size_t vertex_index) noexcept = 0;

        virtual bool IsStatic() const noexcept = 0;
        virtual void SetStatic(bool on_off) noexcept = 0;

        virtual void IntoJson(data::Writer& writer) const = 0;
        virtual bool FromJson(const data::Reader& reader) = 0;

        virtual void BuildPoly(PolygonMeshClass& polygon) const = 0;
        virtual void InitFrom(const PolygonMeshClass& polygon) = 0;
    };

    template<typename Vertex>
    class PolygonBuilder : public IPolygonBuilder
    {
    public:
        // Define how the geometry is to be rasterized.
        using DrawCommand = gfx::Geometry::DrawCommand;

        void ClearAll() noexcept override;
        void ClearDrawCommands() noexcept override;
        void ClearVertices() noexcept override;

        // Add the array of vertices to the existing vertex buffer.
        void AddVertices(const std::vector<Vertex>& vertices);
        void AddVertices(std::vector<Vertex>&& vertices);
        void AddVertices(const Vertex* vertices, size_t num_vertices);

        void UpdateVertex(const Vertex& vert, size_t index);
        void UpdateVertex(const void* vertex, size_t index) override;

        void EraseVertex(size_t index) override;

        void InsertVertex(const Vertex& vertex, size_t cmd_index, size_t index);
        void InsertVertex(const void* vertex, size_t cmd_index, size_t index) override;

        void AppendVertex(const void* vertex) override;

        void AddDrawCommand(const DrawCommand& cmd) override;

        void UpdateDrawCommand(const DrawCommand& cmd, size_t index) noexcept override;

        size_t FindDrawCommand(size_t vertex_index) const noexcept override;

        size_t GetContentHash() const noexcept override;

        size_t GetVertexCount() const noexcept override
        { return mVertices.size(); }
        size_t GetCommandCount() const noexcept override
        { return mDrawCommands.size(); }
        const DrawCommand& GetDrawCommand(size_t index) const noexcept override
        { return mDrawCommands[index]; }

        void GetVertex(void* vertex, size_t vertex_index) const noexcept override
        {
            ASSERT(vertex_index < mVertices.size());
            const auto& vert = mVertices[vertex_index];
            std::memcpy(vertex, &vert, sizeof(vert));
        }
        const void* GetVertexPtr(size_t vertex_index) const noexcept override
        {
            ASSERT(vertex_index < mVertices.size());
            const auto* vert = &mVertices[vertex_index];
            return vert;
        }
        void* GetVertexPtr(size_t vertex_index) noexcept override
        {
            ASSERT(vertex_index < mVertices.size());
            auto* vert =  &mVertices[vertex_index];
            return vert;
        }

        const Vertex& GetVertex(size_t index) const noexcept
        { return mVertices[index]; }
        Vertex& GetVertex(size_t index) noexcept
        { return mVertices[index]; }

        bool IsStatic() const noexcept override
        { return mStatic; }
        void SetStatic(bool on_off) noexcept override
        { mStatic = on_off; }

        void IntoJson(data::Writer& writer) const override;
        bool FromJson(const data::Reader& reader) override;

        void BuildPoly(PolygonMeshClass& polygon) const override;
        void InitFrom(const PolygonMeshClass& polygon) override;

    private:
        std::vector<Vertex> mVertices;
        std::vector<DrawCommand> mDrawCommands;
        bool mStatic = true;
    };

    using PolygonBuilder2D = PolygonBuilder<Vertex2D>;
    using PolygonBuilderPerceptual3D = PolygonBuilder<Perceptual3DVertex>;

} // namespace
} // namespace
