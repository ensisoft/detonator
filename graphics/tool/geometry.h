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

#include "graphics/geometry.h"

// the stuff here is *NOT NEEDED* at runtime. The builder could
// be placed somwhere else for example under the editor itself but
// because the graphics test app also needs this type of functionality
// the builder is here.

namespace gfx {
    class PolygonMeshClass;

namespace tool {

    class PolygonBuilder
    {
    public:
        // Define how the geometry is to be rasterized.
        using DrawCommand = gfx::Geometry::DrawCommand;

        using Vertex = gfx::Vertex2D;

        void Clear() noexcept;
        void ClearDrawCommands() noexcept;
        void ClearVertices() noexcept;

        // Add the array of vertices to the existing vertex buffer.
        void AddVertices(const std::vector<Vertex>& vertices);
        void AddVertices(std::vector<Vertex>&& vertices);
        void AddVertices(const Vertex* vertices, size_t num_vertices);

        // Update the vertex at the given index.
        void UpdateVertex(const Vertex& vert, size_t index);

        // Erase the vertex at the given index.
        void EraseVertex(size_t index);

        // Insert a vertex into the vertex array where the index is
        // an index within the given draw command. Index can be in
        // the range [0, cmd.count].
        // After the new vertex has been inserted the list of draw
        // commands is then modified to include the new vertex.
        // I.e. the draw command that includes the
        // new vertex will grow its count by 1 and all draw commands
        // that come after the will have their starting offsets
        // incremented by 1.
        void InsertVertex(const Vertex& vertex, size_t cmd_index, size_t index);

        void AddDrawCommand(const DrawCommand& cmd);

        void UpdateDrawCommand(const DrawCommand& cmd, size_t index) noexcept;
        // Find the draw command that contains the vertex at the given index.
        // Returns index to the draw command.
        size_t FindDrawCommand(size_t vertex_index) const noexcept;

        // Compute a hash value based on the content only, i.e. the
        // vertices and the draw commands.
        // The hash value is used to realize changes done to a polygon
        // with dynamic content and then to re-upload the data to the GPU.
        size_t GetContentHash() const noexcept;

        inline size_t GetNumVertices() const noexcept
        { return mVertices.size(); }
        inline size_t GetNumDrawCommands() const noexcept
        { return mDrawCommands.size(); }
        inline const DrawCommand& GetDrawCommand(size_t index) const noexcept
        { return mDrawCommands[index]; }
        inline const Vertex& GetVertex(size_t index) const noexcept
        { return mVertices[index]; }
        inline bool IsStatic() const noexcept
        { return mStatic; }
        inline void SetStatic(bool on_off) noexcept
        { mStatic = on_off; }

        void IntoJson(data::Writer& writer) const;
        bool FromJson(const data::Reader& reader);

        void BuildPoly(PolygonMeshClass& polygon) const;
        void InitFrom(const PolygonMeshClass& polygon);

    private:
        std::vector<Vertex> mVertices;
        std::vector<DrawCommand> mDrawCommands;
        bool mStatic = true;
    };

} // namespace
} // namespace
