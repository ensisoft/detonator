// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "base/random.h"
#include "graphics/vertex_algo.h"

namespace gfx
{

void* InterpolateVertex(const void* v0_ptr, const void* v1_ptr, const VertexLayout& layout, VertexBuffer& buffer, float t)
{
    void* vertex = buffer.PushBack();

    for (const auto& attr : layout.attributes)
    {
        if (attr.num_vector_components == 2)
        {
            const auto& v0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(attr, v0_ptr));
            const auto& v1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(attr, v1_ptr));
            const auto& result = (v1 - v0) * t + v0;

            *VertexLayout::GetVertexAttributePtr<Vec2>(attr, vertex) = ToVec(result);
        }
        else if (attr.num_vector_components == 3)
        {
            const auto& v0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec3>(attr, v0_ptr));
            const auto& v1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec3>(attr, v1_ptr));
            const auto& result = (v1 - v0) * t + v0;

            *VertexLayout::GetVertexAttributePtr<Vec3>(attr, vertex) = ToVec(result);
        }
        else if (attr.num_vector_components == 4)
        {
            const auto& v0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec4>(attr, v0_ptr));
            const auto& v1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec4>(attr, v1_ptr));
            const auto& result = (v1 - v0) * t + v0;

            *VertexLayout::GetVertexAttributePtr<Vec4>(attr, vertex) = ToVec(result);
        }
    }
    return vertex;
}

std::tuple<glm::vec3, glm::vec3> ComputeTangent(
        const Vec3& vertex_pos0, const Vec3& vertex_pos1, const Vec3& vertex_pos2,
        const Vec2& vertex_uv0,  const Vec2& vertex_uv1,  const Vec2& vertex_uv2)
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

void SubdivideTriangle(const void* v0_ptr, const void* v1_ptr, const void* v2_ptr,
        const VertexLayout& layout, VertexBuffer& buffer, VertexBuffer& temp,
        TessellationAlgo algo, unsigned sub_div, unsigned sub_div_count, bool discard_skinny_slivers)
{
    ASSERT(sub_div <= sub_div_count);

    if (sub_div == sub_div_count)
    {
        // deal with the skinny slivers here.
        if (discard_skinny_slivers)
        {
            const auto* position_attribute = layout.FindAttribute("aPosition");
            ASSERT(position_attribute);
            if (position_attribute->num_vector_components == 2)
            {
                // find the ratio of the two sides.
                const auto& p0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*position_attribute, v0_ptr));
                const auto& p1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*position_attribute, v1_ptr));
                const auto& p2 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*position_attribute, v2_ptr));

                const auto& dist_p0_p1 = glm::length(p0 - p1);
                const auto& dist_p1_p2 = glm::length(p1 - p2);
                const auto min_dist = std::min(dist_p0_p1, dist_p1_p2);
                const auto max_dist = std::max(dist_p0_p1, dist_p1_p2);
                const auto dist_ratio = max_dist / min_dist;
                if (dist_ratio > 1.75f)
                {
                    //gfx::VertexBuffer temp_too(layout);
                    //SubdivideTriangle(v0_ptr, v1_ptr, v2_ptr, layout, buffer, temp_too,
                    //   TessellationAlgo::LongestEdgeBisection, 0, 1, false);
                }
                else
                {
                    buffer.PushBack(v0_ptr);
                    buffer.PushBack(v1_ptr);
                    buffer.PushBack(v2_ptr);
                }
            }
            else
            {
                buffer.PushBack(v0_ptr);
                buffer.PushBack(v1_ptr);
                buffer.PushBack(v2_ptr);
            }
        }
        else
        {
            buffer.PushBack(v0_ptr);
            buffer.PushBack(v1_ptr);
            buffer.PushBack(v2_ptr);
        }
    }
    else
    {
        if (temp.GetCapacity() == 0)
        {
            unsigned triangle_count = 0;
            if (algo == TessellationAlgo::ApexCut)
                triangle_count = 2;
            else if (algo == TessellationAlgo::MidpointSubdivision)
                triangle_count = 4;
            else if (algo == TessellationAlgo::CentroidSplit)
                triangle_count = 3;
            else if (algo == TessellationAlgo::RandomizedSplit)
                triangle_count = 3;
            else if (algo == TessellationAlgo::LongestEdgeBisection)
                triangle_count = 3;
            else BUG("Unhandled tessellation algo");

            for (unsigned i=0; i<sub_div_count; ++i)
                triangle_count *= 4;

            temp.Reserve(triangle_count * 3);
        }

        const auto vertex_capacity = temp.GetCapacity();
        const auto vertex_count = temp.GetCount();
        ASSERT(vertex_capacity > vertex_count &&
                (vertex_capacity - vertex_count) >= 3);

        if (algo == TessellationAlgo::ApexCut)
        {
            void* v1_v2 = InterpolateVertex(v1_ptr, v2_ptr, layout, temp);

            // left
            SubdivideTriangle(v0_ptr, v1_ptr, v1_v2, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
            // right
            SubdivideTriangle(v0_ptr, v1_v2, v2_ptr, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
        }
        else if (algo == TessellationAlgo::MidpointSubdivision)
        {
            void* v0_v1 = InterpolateVertex(v0_ptr, v1_ptr, layout, temp);
            void* v0_v2 = InterpolateVertex(v0_ptr, v2_ptr, layout, temp);
            void* v1_v2 = InterpolateVertex(v1_ptr, v2_ptr, layout, temp);

            // top triangle
            SubdivideTriangle(v0_ptr, v0_v1, v0_v2, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);

            // bottom half, left triangle
            SubdivideTriangle(v0_v1, v1_ptr, v1_v2, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);

            // bottom half center triangle
            SubdivideTriangle(v0_v1, v1_v2, v0_v2, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);

            // bottom half right triangle
            SubdivideTriangle(v0_v2, v1_v2, v2_ptr, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
        }
        else if (algo == TessellationAlgo::CentroidSplit)
        {
            void* v1_v2 = InterpolateVertex(v1_ptr, v2_ptr, layout, temp);

            void* v_c = InterpolateVertex(v0_ptr, v1_v2, layout, temp);

            // left
            SubdivideTriangle(v0_ptr, v1_ptr, v_c, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);

            // bottom
            SubdivideTriangle(v1_ptr, v2_ptr, v_c, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);

            // right
            SubdivideTriangle(v0_ptr, v_c, v2_ptr, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
        }
        else if (algo == TessellationAlgo::RandomizedSplit)
        {
            const auto t0 = 0.1f + base::rand(0.0f, 0.8f);
            const auto t1 = 0.1f + base::rand(0.0f, 0.8f);

            void* v0_v1 = InterpolateVertex(v0_ptr, v1_ptr, layout, temp, t0);
            void* v1_v2 = InterpolateVertex(v1_ptr, v2_ptr, layout, temp, t1);
            void* v_c = InterpolateVertex(v0_v1, v1_v2, layout, temp, 0.5f);

            // left
            SubdivideTriangle(v0_ptr, v1_ptr, v_c, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);

            // bottom
            SubdivideTriangle(v1_ptr, v2_ptr, v_c, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);

            // right
            SubdivideTriangle(v0_ptr, v_c, v2_ptr, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
        }
        else if (algo == TessellationAlgo::LongestEdgeBisection)
        {
            const auto* position_attribute = layout.FindAttribute("aPosition");
            ASSERT(position_attribute);
            if (position_attribute->num_vector_components == 2)
            {
                // find the ratio of the two sides.
                const auto& p0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*position_attribute, v0_ptr));
                const auto& p1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*position_attribute, v1_ptr));
                const auto& p2 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*position_attribute, v2_ptr));

                const auto& dist_p0_p1 = glm::length(p0 - p1);
                const auto& dist_p1_p2 = glm::length(p1 - p2);
                if (dist_p0_p1 > dist_p1_p2)
                {
                    void* v0_v1 = InterpolateVertex(v0_ptr, v1_ptr, layout, temp);
                    void* v0_v2 = InterpolateVertex(v0_ptr, v2_ptr, layout, temp);

                    SubdivideTriangle(v0_ptr, v0_v1, v0_v2, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
                    SubdivideTriangle(v0_v1, v1_ptr, v0_v2, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
                    SubdivideTriangle(v0_v2, v1_ptr, v2_ptr, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
                }
                else
                {
                    void* v1_v2 = InterpolateVertex(v1_ptr, v2_ptr, layout, temp);
                    void* v0_v2 = InterpolateVertex(v0_ptr, v2_ptr, layout, temp);
                    SubdivideTriangle(v0_ptr, v1_ptr, v0_v2, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
                    SubdivideTriangle(v0_v2, v1_ptr, v1_v2, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
                    SubdivideTriangle(v0_v2, v1_v2, v2_ptr, layout, buffer, temp, algo, sub_div + 1, sub_div_count, discard_skinny_slivers);
                }
            }
        }
    }
}

} // namespace