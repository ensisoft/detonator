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

#include <cstring>
#include <tuple>

#include "graphics/geometry.h"

namespace {
    std::tuple<glm::vec3, glm::vec3> ComputeTangent(const gfx::Vec3& vertex_pos0,
                                                    const gfx::Vec3& vertex_pos1,
                                                    const gfx::Vec3& vertex_pos2,
                                                    const gfx::Vec2& vertex_uv0,
                                                    const gfx::Vec2& vertex_uv1,
                                                    const gfx::Vec2& vertex_uv2)
    {
        glm::vec3 pos1 = ToVec(vertex_pos0);
        glm::vec3 pos2 = ToVec(vertex_pos1);
        glm::vec3 pos3 = ToVec(vertex_pos2);

        // flipeti flip, we use "flipped" texture coordinates where y=0.0 is "up"
        // and y=1.0 is "bottom"
        auto FlipUV = [](glm::vec2 uv) {
            float x = uv.x;
            float y = 1.0f - uv.y;
            return glm::vec2 {x, y};
        };

        glm::vec2 uv1 = FlipUV(ToVec(vertex_uv0));
        glm::vec2 uv2 = FlipUV(ToVec(vertex_uv1));
        glm::vec2 uv3 = FlipUV(ToVec(vertex_uv2));

        glm::vec3 tangent;
        glm::vec3 bitangent;

        glm::vec3 edge1 = pos2 - pos1;
        glm::vec3 edge2 = pos3 - pos1;
        glm::vec2 deltaUV1 = uv2 - uv1;
        glm::vec2 deltaUV2 = uv3 - uv1;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        tangent = glm::normalize(tangent);

        bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
        bitangent = glm::normalize(bitangent);

        return {tangent, bitangent };
    }

    void* InterpolateVertex(const void* v0_ptr, const void* v1_ptr,
        const gfx::VertexLayout& layout, gfx::VertexBuffer& buffer)
    {
        using namespace gfx;

        void* vertex = buffer.PushBack();

        for (const auto& attr : layout.attributes)
        {
            if (attr.num_vector_components == 2)
            {
                const auto& v0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(attr, v0_ptr));
                const auto& v1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(attr, v1_ptr));
                const auto& result = (v1 - v0) * 0.5f + v0;

                *VertexLayout::GetVertexAttributePtr<Vec2>(attr, vertex) = ToVec(result);
            }
            else if (attr.num_vector_components == 3)
            {
                const auto& v0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec3>(attr, v0_ptr));
                const auto& v1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec3>(attr, v1_ptr));
                const auto& result = (v1 - v0) * 0.5f + v0;

                *VertexLayout::GetVertexAttributePtr<Vec3>(attr, vertex) = ToVec(result);
            }
            else if (attr.num_vector_components == 4)
            {
                const auto& v0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec4>(attr, v0_ptr));
                const auto& v1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec4>(attr, v1_ptr));
                const auto& result = (v1 - v0) * 0.5f + v0;

                *VertexLayout::GetVertexAttributePtr<Vec4>(attr, vertex) = ToVec(result);
            }
        }

        return vertex;
    }

    void SubdivideTriangle(const void* v0_ptr, const void* v1_ptr, const void* v2_ptr,
        const gfx::VertexLayout& layout, gfx::VertexBuffer& buffer, gfx::VertexBuffer& temp,
        unsigned current_subdivision,
        unsigned maximum_subdivisions)
    {
        if (current_subdivision == maximum_subdivisions)
        {
            buffer.PushBack(v0_ptr);
            buffer.PushBack(v1_ptr);
            buffer.PushBack(v2_ptr);
        }
        else
        {
            // first algo
            // split triangle in half and then the bottom half
            // into 3 triangles.

            void* v0_v1 = InterpolateVertex(v0_ptr, v1_ptr, layout, temp);
            void* v0_v2 = InterpolateVertex(v0_ptr, v2_ptr, layout, temp);
            void* v1_v2 = InterpolateVertex(v1_ptr, v2_ptr, layout, temp);

            // top triangle
            SubdivideTriangle(v0_ptr, v0_v1, v0_v2, layout, buffer, temp, current_subdivision + 1, maximum_subdivisions);
            // bottom half, left triangle
            SubdivideTriangle(v0_v1, v1_ptr, v1_v2, layout, buffer, temp, current_subdivision + 1, maximum_subdivisions);

            // bottom half center triangle
            SubdivideTriangle(v0_v1, v1_v2, v0_v2, layout, buffer, temp, current_subdivision + 1, maximum_subdivisions);

            // bottom half right triangle
            SubdivideTriangle(v0_v2, v1_v2, v2_ptr, layout, buffer, temp, current_subdivision + 1, maximum_subdivisions);
        }
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

bool CreateTriangleMesh(const GeometryBuffer& geometry, GeometryBuffer& buffer, unsigned subdiv_count)
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

                const auto* v0 = vertices.GetVertexPtr(i0);
                const auto* v1 = vertices.GetVertexPtr(i1);
                const auto* v2 = vertices.GetVertexPtr(i2);

                // triangle v0, v1, v2
                gfx::VertexBuffer temp(vertex_layout);
                SubdivideTriangle(v0, v1, v2, vertex_layout, vertex_buffer, temp, 0, subdiv_count);
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

            // first triangle v0, v1, v2
            gfx::VertexBuffer temp(vertex_layout);
            SubdivideTriangle(v0, v1, v2, vertex_layout, vertex_buffer, temp, 0, subdiv_count);

            for (size_t j=3; j<primitive_count; ++j)
            {
                const auto start = cmd.offset + j;
                const uint32_t iPrev = has_index ? indices.GetIndex(start-1) : start-1;
                const uint32_t iCurr = has_index ? indices.GetIndex(start-0) : start-0;
                const void* vPrev = vertices.GetVertexPtr(iPrev);
                const void* vCurr = vertices.GetVertexPtr(iCurr);

                // triangle v0, vPrev, vCurr
                gfx::VertexBuffer temp(vertex_layout);
                SubdivideTriangle(v0, v1, v2, vertex_layout, vertex_buffer, temp, 0, subdiv_count);
            }
        }
    }
    buffer.SetVertexBuffer(std::move(vertex_data));
    buffer.SetVertexLayout(geometry.GetLayout());
    buffer.AddDrawCmd(Geometry::DrawType::Triangles);
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
    normals.AddDrawCmd(Geometry::DrawType::Lines);
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

                ASSERT(i0 < vertex_count);
                ASSERT(i1 < vertex_count);
                ASSERT(i2 < vertex_count);
                CalcTangents(i0, i1, i2);
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



} // namespace
