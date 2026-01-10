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

#include "config.h"

#include <cstring>
#include <tuple>
#include <limits>
#include <algorithm>

#include "base/hash.h"
#include "base/math.h"
#include "graphics/geometry_buffer.h"
#include "graphics/geometry_algo.h"
#include "graphics/vertex_algo.h"

#include "base/snafu.h"

namespace gfx
{

size_t GeometryBuffer::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mVertexLayout.GetHash());
    hash = base::hash_combine(hash, mDrawCmds);
    hash = base::hash_combine(hash, mVertexData);
    hash = base::hash_combine(hash, mIndexData);
    hash = base::hash_combine(hash, mIndexType);
    return hash;
}

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

    auto AddLine = [](gfx::VertexBuffer& buffer, const void* v0, const void* v1) {
        buffer.PushBack(v0);
        buffer.PushBack(v1);
    };

    for (size_t i=0; i<geometry.GetNumDrawCmds(); ++i)
    {
        const auto& cmd = geometry.GetDrawCmd(i);
        const auto primitive_count = cmd.count != std::numeric_limits<uint32_t>::max()
                           ? (cmd.count)
                           : (has_index ? index_count : vertex_count);

        if (cmd.type == GeometryBuffer::DrawType::Triangles)
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
        else if (cmd.type == GeometryBuffer::DrawType::TriangleFan)
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
        else if (cmd.type == GeometryBuffer::DrawType::TriangleStrip)
        {
            ASSERT(primitive_count >= 3);
            // the first 3 vertices form a triangle and then every subsequent
            // vertex creates another triangle with the previous two vertices.

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
                const uint32_t vi0 = has_index ? indices.GetIndex(start-1) : start-1;
                const uint32_t vi1 = has_index ? indices.GetIndex(start-2) : start-2;
                const void* v0 = vertices.GetVertexPtr(start);
                const void* v1 = vertices.GetVertexPtr(vi0);
                const void* v2 = vertices.GetVertexPtr(vi1);

                AddLine(vertex_writer, v0, v1);
                AddLine(vertex_writer, v0, v2);
            }
        }
    }

    wireframe.SetVertexBuffer(std::move(vertex_data));
    wireframe.SetVertexLayout(geometry.GetLayout());
    wireframe.AddDrawCmd(GeometryBuffer::DrawType::Lines);
}

bool TessellateMesh(const GeometryBuffer& geometry, GeometryBuffer& buffer,
    TessellationAlgo algo, unsigned sub_div_count)
{
    const VertexStream vertices(geometry.GetLayout(),
                            geometry.GetVertexDataPtr(),
                            geometry.GetVertexBytes());

    const IndexStream indices(geometry.GetIndexDataPtr(),
                              geometry.GetIndexBytes(),
                              geometry.GetIndexType());

    std::vector<uint8_t> vertex_data;
    VertexBuffer vertex_buffer(geometry.GetLayout(), &vertex_data);

    const auto& vertex_layout = geometry.GetLayout();
    const auto vertex_count = vertices.GetCount();
    const auto index_count  = indices.GetCount();
    const auto has_index = indices.IsValid();

    for (size_t i=0; i<geometry.GetNumDrawCmds(); ++i)
    {
        const auto& cmd = geometry.GetDrawCmd(i);
        const auto primitive_count = cmd.count != std::numeric_limits<uint32_t>::max()
                           ? (cmd.count)
                           : (has_index ? index_count : vertex_count);

        if (cmd.type == GeometryBuffer::DrawType::Triangles)
        {
            ASSERT((primitive_count % 3) == 0);
            const auto triangles = primitive_count / 3;

            for (size_t j=0; j<triangles; ++j)
            {
                const auto start = cmd.offset + j * 3;
                const uint32_t i0 = has_index ? indices.GetIndex(start+0) : start+0;
                const uint32_t i1 = has_index ? indices.GetIndex(start+1) : start+1;
                const uint32_t i2 = has_index ? indices.GetIndex(start+2) : start+2;

                const auto* v0 = vertices.GetVertexPtr(i0);
                const auto* v1 = vertices.GetVertexPtr(i1);
                const auto* v2 = vertices.GetVertexPtr(i2);

                // triangle v0, v1, v2
                gfx::VertexBuffer temp(vertex_layout);
                SubdivideTriangle(v0, v1, v2, vertex_layout, vertex_buffer, temp, algo, 0, sub_div_count);
            }
        }
        else if (cmd.type == GeometryBuffer::DrawType::TriangleFan)
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

            // first triangle v0, v1, v2
            gfx::VertexBuffer temp(vertex_layout);
            SubdivideTriangle(v0, v1, v2, vertex_layout, vertex_buffer, temp, algo, 0, sub_div_count);

            for (size_t j=3; j<primitive_count; ++j)
            {
                const auto start = cmd.offset + j;
                const uint32_t iPrev = has_index ? indices.GetIndex(start-1) : start-1;
                const uint32_t iCurr = has_index ? indices.GetIndex(start-0) : start-0;
                const void* vPrev = vertices.GetVertexPtr(iPrev);
                const void* vCurr = vertices.GetVertexPtr(iCurr);

                // triangle v0, vPrev, vCurr
                gfx::VertexBuffer temp(vertex_layout);
                SubdivideTriangle(v0, vPrev, vCurr, vertex_layout, vertex_buffer, temp, algo, 0, sub_div_count);
            }
        }
        else if (cmd.type == GeometryBuffer::DrawType::TriangleStrip)
        {
            ASSERT(primitive_count >= 3);
            // the first 3 vertices form a triangle and then every subsequent
            // vertex creates another triangle with the previous two vertices.
            // the order between the last two vertices flip-flops based on the
            // index being odd or even.
            const uint32_t i0 = has_index ? indices.GetIndex(cmd.offset+0) : cmd.offset+0;
            const uint32_t i1 = has_index ? indices.GetIndex(cmd.offset+1) : cmd.offset+1;
            const uint32_t i2 = has_index ? indices.GetIndex(cmd.offset+2) : cmd.offset+2;
            const void* v0 = vertices.GetVertexPtr(i0);
            const void* v1 = vertices.GetVertexPtr(i1);
            const void* v2 = vertices.GetVertexPtr(i2);

            // first triangle v0, v1, v2
            VertexBuffer temp(vertex_layout);
            SubdivideTriangle(v0, v1, v2, vertex_layout, vertex_buffer, temp, algo, 0, sub_div_count);

            for (size_t j=3; j<primitive_count; ++j)
            {
                const auto start = cmd.offset + j;
                const uint32_t vi0 = has_index ? indices.GetIndex(start-1) : start-1;
                const uint32_t vi1 = has_index ? indices.GetIndex(start-2) : start-2;
                const void* v0 = vertices.GetVertexPtr(start);
                const void* v1 = vertices.GetVertexPtr(vi0);
                const void* v2 = vertices.GetVertexPtr(vi1);

                const bool is_odd = (j & 0x1) == 0x1;
                if (is_odd)
                {
                    VertexBuffer temp(vertex_layout);
                    SubdivideTriangle(v0, v1, v2, vertex_layout, vertex_buffer, temp, algo, 0, sub_div_count);
                }
                else
                {
                    VertexBuffer temp(vertex_layout);
                    SubdivideTriangle(v0, v2, v1, vertex_layout, vertex_buffer, temp, algo, 0, sub_div_count);
                }
            }

        }
    }
    buffer.SetVertexBuffer(std::move(vertex_data));
    buffer.SetVertexLayout(geometry.GetLayout());
    buffer.AddDrawCmd(GeometryBuffer::DrawType::Triangles);
    return true;
}

bool CreateNormalMesh(const GeometryBuffer& geometry, GeometryBuffer& normals, unsigned flags, float line_length)
{
    const VertexStream vertices(geometry.GetLayout(),
                                geometry.GetVertexDataPtr(),
                                geometry.GetVertexBytes());
    const auto vertex_count = vertices.GetCount();

    const auto* vertex_position = vertices.FindAttribute("aPosition");
    if (!vertex_position || vertex_position->num_vector_components != 3)
        return false;

    unsigned data_count = 0;

    if (flags & NormalMeshFlags::Normals)
    {
        const auto* normal   = vertices.FindAttribute("aNormal");
        if (!normal || normal->num_vector_components != 3)
            return false;

        data_count++;
    }

    if (flags & NormalMeshFlags::Tangents)
    {
        const auto* tangent = vertices.FindAttribute("aTangent");
        if (!tangent || tangent->num_vector_components != 3)
            return false;

        data_count++;
    }
    if (flags & NormalMeshFlags::Bitangents)
    {
        const auto* bitangent = vertices.FindAttribute("aBitangent");
        if (!bitangent || bitangent->num_vector_components != 3)
            return false;

        data_count++;
    }

    if (data_count == 0)
        return true;


    std::vector<uint8_t> vertex_buffer;
    VertexBuffer vertex_writer(GetVertexLayout<Vertex3D>(), &vertex_buffer);
    vertex_writer.Resize(vertex_count * data_count * 2);

    for (size_t i=0; i<vertex_count; ++i)
    {
        const auto& aPosition = ToVec(*vertices.GetAttribute<Vec3>("aPosition", i));

        const auto vertex_base_index = i * data_count * 2;

        unsigned vertex_data_index = 0;

        if (flags & NormalMeshFlags::Normals)
        {
            const auto& aNormal = ToVec(*vertices.GetAttribute<Vec3>("aNormal", i));

            Vertex3D a;
            a.aPosition = ToVec(aPosition);

            Vertex3D b;
            b.aPosition = ToVec(aPosition + aNormal * line_length);

            const auto vertex_index = vertex_base_index + vertex_data_index;
            vertex_writer.SetVertex(a, vertex_index + 0);
            vertex_writer.SetVertex(b, vertex_index + 1);
            vertex_data_index += 2;
        }
        if (flags & NormalMeshFlags::Tangents)
        {
            const auto& aTangent = ToVec(*vertices.GetAttribute<Vec3>("aTangent", i));

            Vertex3D a;
            a.aPosition = ToVec(aPosition);

            Vertex3D b;
            b.aPosition = ToVec(aPosition + aTangent * line_length);

            const auto vertex_index = vertex_base_index + vertex_data_index;
            vertex_writer.SetVertex(a, vertex_index + 0);
            vertex_writer.SetVertex(b, vertex_index + 1);
            vertex_data_index += 2;
        }

        if (flags & NormalMeshFlags::Bitangents)
        {
            const auto& aBitangent = ToVec(*vertices.GetAttribute<Vec3>("aBitangent", i));

            Vertex3D a;
            a.aPosition = ToVec(aPosition);

            Vertex3D b;
            b.aPosition = ToVec(aPosition + aBitangent * line_length);

            const auto vertex_index = vertex_base_index + vertex_data_index;
            vertex_writer.SetVertex(a, vertex_index + 0);
            vertex_writer.SetVertex(b, vertex_index + 1);
            vertex_data_index += 2;
        }
    }

    normals.SetVertexBuffer(std::move(vertex_buffer));
    normals.SetVertexLayout(GetVertexLayout<Vertex3D>());
    normals.AddDrawCmd(GeometryBuffer::DrawType::Lines);
    return true;
}

bool CreateShardEffectMesh(const GeometryBuffer& original_geometry_buffer,
                               GeometryBuffer* shard_geometry_buffer, unsigned mesh_subdivision_count)
{
    // the triangle mesh computation produces a  mesh that  has the same
    // vertex layout as the original drawables geometry  buffer.
    if (!TessellateMesh(original_geometry_buffer, *shard_geometry_buffer, TessellationAlgo::LongestEdgeBisection, mesh_subdivision_count))
    {
        return false;
    }

    ASSERT(shard_geometry_buffer->GetLayout() == GetVertexLayout<Vertex2D>());
    ASSERT(shard_geometry_buffer->HasIndexData() == false);

    const VertexStream vertex_stream(shard_geometry_buffer->GetLayout(),
                                      shard_geometry_buffer->GetVertexBuffer());
    const auto vertex_count = vertex_stream.GetCount();

    // change the vertex format to ShardVertex2D and compute shard indices
    // for each vertex using this vertex buffer.
    VertexBuffer vertex_buffer;
    vertex_buffer.SetVertexLayout(GetVertexLayout<ShardVertex2D>());
    vertex_buffer.Resize(vertex_count);

    for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
    {
        const auto triangle_index = vertex_index / 3;
        const auto* src_vertex = vertex_stream.GetVertex<Vertex2D>(vertex_index);

        ShardVertex2D vertex;
        vertex.aPosition   = src_vertex->aPosition;
        vertex.aTexCoord   = src_vertex->aTexCoord;
        vertex.aShardIndex = triangle_index;
        vertex_buffer.SetVertex(vertex, vertex_index);
    }
    // change the layout and the vertex data.
    // the draw commands remain unchanged.
    shard_geometry_buffer->SetVertexLayout(GetVertexLayout<ShardVertex2D>());
    shard_geometry_buffer->SetVertexBuffer(vertex_buffer.TransferBuffer());
    return true;
}

bool ComputeTangents(GeometryBuffer& geometry)
{
    // allow read+write access to the data.
    VertexBuffer vertices(geometry.GetLayout(), &geometry.GetVertexBuffer());

    const IndexStream indices(geometry.GetIndexDataPtr(),
                              geometry.GetIndexBytes(),
                              geometry.GetIndexType());

    const auto vertex_count = vertices.GetCount();
    const auto index_count  = indices.GetCount();
    const auto has_index = indices.IsValid();

    const auto* vertex_position = vertices.FindAttribute("aPosition");
    const auto* vertex_tangent  = vertices.FindAttribute("aTangent");
    const auto* vertex_bitangent = vertices.FindAttribute("aBitangent");
    const auto* vertex_texcoord  = vertices.FindAttribute("aTexCoord");

    if (!vertex_position || vertex_position->num_vector_components != 3)
        return false;
    if (!vertex_tangent || vertex_tangent->num_vector_components != 3)
        return false;
    if (!vertex_bitangent || vertex_bitangent->num_vector_components != 3)
        return false;
    if (!vertex_texcoord || vertex_texcoord->num_vector_components != 2)
        return false;

    std::vector<uint16_t> vertex_use_count;

    // use 3 vertex indices into the vertex array to take 3 vertices
    // and 3 texture coordinates and use those to compute the surface
    // (triangle) tangent and bitangent vectors. these vectors are the
    // same for every vertex of the triangle.
    // Then for each vertex take the previous vectors and compute the
    // new average vectors based on previous and current data.
    auto CalcTangents = [&vertex_use_count, &vertices](uint32_t i0, uint32_t i1, uint32_t i2) {
        const auto [tangent, bitangent] = ComputeTangent(
                *vertices.GetAttribute<Vec3>("aPosition", i0),
                *vertices.GetAttribute<Vec3>("aPosition", i1),
                *vertices.GetAttribute<Vec3>("aPosition", i2),
                *vertices.GetAttribute<Vec2>("aTexCoord", i0),
                *vertices.GetAttribute<Vec2>("aTexCoord", i1),
                *vertices.GetAttribute<Vec2>("aTexCoord", i2));

        // every vertex has the same normal, tangent and bitangent.
        // if the vertex is shared with many surfaces then average
        // out the tangent vectors.
        const uint32_t indices[3] = {i0, i1, i2};
        for (int i=0; i<3; ++i)
        {
            const auto vertex_index = indices[i];
            if (vertex_index >= vertex_use_count.size())
                vertex_use_count.resize(vertex_index+1);

            const auto use_count = vertex_use_count[vertex_index];

            auto* tangent_attr_ptr = vertices.GetAttribute<Vec3>("aTangent", vertex_index);
            auto* bitagent_attr_ptr = vertices.GetAttribute<Vec3>("aBitangent", vertex_index);

            auto current_tangent_value = ToVec(*tangent_attr_ptr);
            auto current_bitangent_value = ToVec(*bitagent_attr_ptr);

            auto new_tangent_value = math::RunningAvg(current_tangent_value, use_count + 1, tangent);
            auto new_bitagent_value = math::RunningAvg(current_bitangent_value, use_count + 1, bitangent);

            *tangent_attr_ptr = ToVec(new_tangent_value);
            *bitagent_attr_ptr = ToVec(new_bitagent_value);

            vertex_use_count[vertex_index] = use_count + 1;
        }
    };

    for (size_t i=0; i<geometry.GetNumDrawCmds(); ++i)
    {
        const auto& cmd = geometry.GetDrawCmd(i);
        const auto primitive_count = cmd.count != std::numeric_limits<uint32_t>::max()
                                     ? (cmd.count)
                                     : (has_index ? index_count : vertex_count);

        if (cmd.type == GeometryBuffer::DrawType::Triangles)
        {
            ASSERT((primitive_count % 3) == 0);
            const auto triangles = primitive_count / 3;

            for (size_t j=0; j<triangles; ++j)
            {
                const auto start = cmd.offset + j * 3;
                const uint32_t i0 = has_index ? indices.GetIndex(start+0) : start+0;
                const uint32_t i1 = has_index ? indices.GetIndex(start+1) : start+1;
                const uint32_t i2 = has_index ? indices.GetIndex(start+2) : start+2;

                ASSERT(i0 < vertex_count);
                ASSERT(i1 < vertex_count);
                ASSERT(i2 < vertex_count);
                CalcTangents(i0, i1, i2);
            }
        }
        else if (cmd.type == GeometryBuffer::DrawType::TriangleFan)
        {
            ASSERT(primitive_count >= 3);
            // the first 3 vertices form a triangle and then
            // every subsequent vertex creates another triangle with
            // the first and previous vertex.
            const uint32_t i0 = has_index ? indices.GetIndex(cmd.offset+0) : cmd.offset+0;
            const uint32_t i1 = has_index ? indices.GetIndex(cmd.offset+1) : cmd.offset+1;
            const uint32_t i2 = has_index ? indices.GetIndex(cmd.offset+2) : cmd.offset+2;

            ASSERT(i0 < vertex_count);
            ASSERT(i1 < vertex_count);
            ASSERT(i2 < vertex_count);
            CalcTangents(i0, i1, i2);

            for (size_t j=3; j<primitive_count; ++j)
            {
                const auto start = cmd.offset + j;
                const uint32_t iPrev = has_index ? indices.GetIndex(start-1) : start-1;
                const uint32_t iCurr = has_index ? indices.GetIndex(start-0) : start-0;

                ASSERT(iPrev < vertex_count);
                ASSERT(iCurr < vertex_count);
                CalcTangents(i0, iPrev, iCurr);
            }
        }
        else if (cmd.type == GeometryBuffer::DrawType::TriangleStrip)
        {
            ASSERT(primitive_count >= 3);
            // the first 3 vertices form a triangle and then every subsequent
            // vertex creates another triangle with the previous two vertices.
            // the order between the last two vertices flip-flops based on the
            // index being odd or even.
            const uint32_t i0 = has_index ? indices.GetIndex(cmd.offset+0) : cmd.offset+0;
            const uint32_t i1 = has_index ? indices.GetIndex(cmd.offset+1) : cmd.offset+1;
            const uint32_t i2 = has_index ? indices.GetIndex(cmd.offset+2) : cmd.offset+2;

            CalcTangents(i0, i1, i2);

            for (size_t j=3; j<primitive_count; ++j)
            {
                const auto start = cmd.offset + j;
                const uint32_t i0 = has_index ? indices.GetIndex(start-0) : start-0;
                const uint32_t i1 = has_index ? indices.GetIndex(start-1) : start-1;
                const uint32_t i2 = has_index ? indices.GetIndex(start-2) : start-2;

                const bool is_odd = (j & 0x1) == 0x1;
                if (is_odd)
                {
                    CalcTangents(i0, i1, i2);
                }
                else
                {
                    CalcTangents(i0, i2, i1);
                }
            }
        }
    }

    for (size_t i=0; i<vertex_count; ++i)
    {
        auto* tangent_ptr = vertices.GetAttribute<Vec3>("aTangent", i);
        auto* bitangent_ptr = vertices.GetAttribute<Vec3>("aBitangent", i);

        auto tangent = glm::normalize(ToVec(*tangent_ptr));
        auto bitangent = glm::normalize(ToVec(*bitangent_ptr));

        *tangent_ptr = ToVec(tangent);
        *bitangent_ptr = ToVec(bitangent);
    }

    return true;
}

bool FindGeometryMinMax(const GeometryBuffer& buffer, glm::vec3* minimums, glm::vec3* maximums)
{
    const VertexStream vertex_stream(buffer.GetLayout(), buffer.GetVertexBuffer());
    const auto vertex_count = vertex_stream.GetCount();
    const auto* vertex_position = vertex_stream.FindAttribute("aPosition");
    if (!vertex_position)
        return false;

    glm::vec3 max = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };
    glm::vec3 min = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };

    for (size_t i=0; i<vertex_count; ++i)
    {
        const auto* vertex = vertex_stream.GetVertexPtr(i);

        if (vertex_position->num_vector_components == 2)
        {
            const auto p = *VertexLayout::GetVertexAttributePtr<Vec2>(*vertex_position, vertex);
            min.x = std::min(min.x, p.x);
            min.y = std::min(min.y, p.y);

            max.x = std::max(max.x, p.x);
            max.y = std::max(max.y, p.y);
        }
        else if (vertex_position->num_vector_components == 3)
        {
            const auto p = *VertexLayout::GetVertexAttributePtr<Vec3>(*vertex_position, vertex);
            min.x = std::min(min.x, p.x);
            min.y = std::min(min.y, p.y);
            min.z = std::min(min.z, p.z);

            max.x = std::max(max.x, p.x);
            max.y = std::max(max.y, p.y);
            max.z = std::max(max.z, p.z);
        }
    }
    *minimums = min;
    *maximums = max;

    if (vertex_position->num_vector_components == 2)
    {
        minimums->z = 0.0f;
        maximums->z = 0.0f;
    }
    return true;
}

} // namespace
