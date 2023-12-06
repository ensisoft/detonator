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

#include "graphics/geometry.h"

namespace {
    void AddLine(gfx::VertexBuffer& buffer, const void* v0, const void* v1)
    {
        buffer.PushBack(v0);
        buffer.PushBack(v1);
    }
} // namespace

namespace gfx
{

void CreateWireframe(const GeometryBuffer& geometry, GeometryBuffer& wireframe)
{
    const VertexStream vertices(geometry.GetLayout(),
                                geometry.GetVertexDataPtr(),
                                geometry.GetVertexBytes());

    const IndexStream indices(geometry.GetIndexDataPtr(),
                              geometry.GetIndexBytes(),
                              geometry.GetIndexType());

    std::vector<uint8_t> vertex_data;
    VertexBuffer vertex_writer(geometry.GetLayout(), &vertex_data);

    const auto vertex_count = vertices.GetCount();
    const auto index_count  = indices.GetCount();
    const auto has_index = indices.IsValid();

    for (size_t i=0; i<geometry.GetNumDrawCmds(); ++i)
    {
        const auto& cmd = geometry.GetDrawCmd(i);
        const auto primitive_count = cmd.count != std::numeric_limits<size_t>::max()
                           ? (cmd.count)
                           : (has_index ? index_count : vertex_count);

        if (cmd.type == Geometry::DrawType::Triangles)
        {
            ASSERT((primitive_count % 3) == 0);
            const auto triangles = primitive_count / 3;

            for (size_t j=0; j<triangles; ++j)
            {
                const auto start = cmd.offset + j * 3;
                const uint32_t i0 = has_index ? indices.GetIndex(start+0) : start+0;
                const uint32_t i1 = has_index ? indices.GetIndex(start+1) : start+1;
                const uint32_t i2 = has_index ? indices.GetIndex(start+2) : start+2;
                const void* v0 = vertices.GetVertexPtr(i0);
                const void* v1 = vertices.GetVertexPtr(i1);
                const void* v2 = vertices.GetVertexPtr(i2);

                AddLine(vertex_writer, v0, v1);
                AddLine(vertex_writer, v1, v2);
                AddLine(vertex_writer, v2, v0);
            }
        }
        else if (cmd.type == Geometry::DrawType::TriangleFan)
        {
            ASSERT(primitive_count >= 3);
            // the first 3 vertices form a triangle and then
            // every subsequent vertex creates another triangle with
            // the first and previous vertex.
            const uint32_t i0 = has_index ? indices.GetIndex(cmd.offset+0) : cmd.offset+0;
            const uint32_t i1 = has_index ? indices.GetIndex(cmd.offset+1) : cmd.offset+1;
            const uint32_t i2 = has_index ? indices.GetIndex(cmd.offset+2) : cmd.offset+2;
            const void* v0 = vertices.GetVertexPtr(i0);
            const void* v1 = vertices.GetVertexPtr(i1);
            const void* v2 = vertices.GetVertexPtr(i2);

            AddLine(vertex_writer, v0, v1);
            AddLine(vertex_writer, v1, v2);
            AddLine(vertex_writer, v2, v0);

            for (size_t j=3; j<primitive_count; ++j)
            {
                const auto start = cmd.offset + j;
                const uint32_t iPrev = has_index ? indices.GetIndex(start-1) : start-1;
                const uint32_t iCurr = has_index ? indices.GetIndex(start-0) : start-0;
                const void* vPrev = vertices.GetVertexPtr(iPrev);
                const void* vCurr = vertices.GetVertexPtr(iCurr);

                AddLine(vertex_writer, vCurr, vPrev);
                AddLine(vertex_writer, vCurr, v0);
            }
        }
    }

    wireframe.SetVertexBuffer(std::move(vertex_data));
    wireframe.SetVertexLayout(geometry.GetLayout());
    wireframe.AddDrawCmd(Geometry::DrawType::Lines);
}

} // namespace
