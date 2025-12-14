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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <tuple>

#include "graphics/enum.h"
#include "graphics/vertex.h"
#include "graphics/vertex_buffer.h"

namespace gfx
{
    // Interpolate between two vertices and push the interpolation
    // result into result buffer. Returns a pointer to the vertex.
    void* InterpolateVertex(const void* v0_ptr, const void* v1_ptr,
        const VertexLayout& layout, VertexBuffer& buffer, float t = 0.5f);

    std::tuple<glm::vec3, glm::vec3> ComputeTangent(
        const Vec3& vertex_pos0, const Vec3& vertex_pos1, const Vec3& vertex_pos2,
        const Vec2& vertex_uv0,  const Vec2& vertex_uv1,  const Vec2& vertex_uv2);

    void SubdivideTriangle(const void* v0_ptr, const void* v1_ptr, const void* v2_ptr,
        const VertexLayout& layout, VertexBuffer& buffer, VertexBuffer& temp,
        TessellationAlgo algo, unsigned sub_div, unsigned sub_div_count, bool discard_skinny_slivers = true);

} // namespace