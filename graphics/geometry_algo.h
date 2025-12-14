// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "graphics/enum.h"

namespace gfx
{
    class GeometryBuffer;
    class VertexBuffer;

    enum NormalMeshFlags {
        Normals = 0x1,
        Tangents = 0x2,
        Bitangents = 0x4
    };

    void CreateWireframe(const GeometryBuffer& geometry, GeometryBuffer& wireframe);

    bool CreateNormalMesh(const GeometryBuffer& geometry, GeometryBuffer& normals,
                      unsigned flags = NormalMeshFlags::Normals, float line_length = 0.2f);

    bool TessellateMesh(const GeometryBuffer& geometry, GeometryBuffer& buffer,
                        TessellationAlgo algo, unsigned sub_div_count);

    bool ComputeTangents(GeometryBuffer& geometry);

    bool FindGeometryMinMax(const GeometryBuffer& buffer, glm::vec3* minimums, glm::vec3* maximums);
} // namespace