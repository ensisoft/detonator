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
#include <string>
#include <memory>

#include "graphics/vertex.h"
#include "graphics/geometry_buffer.h"

namespace gfx
{
    // Encapsulate information about a particular geometry and how
    // that geometry is to be rendered and rasterized. A geometry
    // object contains a set of vertex data and then multiple draw
    // commands each command addressing some subset of the vertices.
    class Geometry
    {
    public:
        using Usage       = GeometryBuffer::Usage;
        using DrawType    = GeometryBuffer::DrawType;
        using DrawCommand = GeometryBuffer::DrawCommand;
        using IndexType   = GeometryBuffer::IndexType;

        struct CreateArgs {
            // This is the geometry data buffer with vertex and index data
            // and the draw commands. Use this or the buffer as an alternative.
            std::shared_ptr<const GeometryBuffer> buffer_ptr;
            // This is the geometry data buffer with vertex and index data
            // and the draw commands. Use this or the buffer_ptr as an alternative.
            GeometryBuffer buffer;
            // Set the expected usage of the geometry. Should be set before
            // calling any methods to upload the data.
            Usage usage = Usage::Static;
            // Set the (human-readable) name of the geometry.
            // This has debug significance only.
            std::string content_name;
            // Get the hash value based on the buffer contents.
            std::size_t content_hash = 0;
        };

        virtual ~Geometry() = default;

        // Get the human-readable geometry name.
        virtual std::string GetName() const = 0;
        // Get the current usage set on the geometry.
        virtual Usage GetUsage() const = 0;
        // Get the hash value computed from the geometry buffer.
        virtual std::size_t GetContentHash() const  = 0;
        // Get the number of draw commands set on the geometry.
        virtual std::size_t GetNumDrawCmds() const = 0;
        // Get the draw command at the specified index.
        virtual DrawCommand GetDrawCmd(size_t index) const = 0;
    private:
    };

    using GeometryPtr = std::shared_ptr<const Geometry>;

    void CreateWireframe(const GeometryBuffer& geometry, GeometryBuffer& wireframe);

    enum NormalMeshFlags {
        Normals = 0x1,
        Tangents = 0x2,
        Bitangents = 0x4
    };

    bool CreateNormalMesh(const GeometryBuffer& geometry, GeometryBuffer& normals,
                          unsigned flags = NormalMeshFlags::Normals, float line_length = 0.2f);

    bool TessellateMesh(const GeometryBuffer& geometry, GeometryBuffer& buffer,
        TessellationAlgo algo, unsigned sub_div_count);

    bool ComputeTangents(GeometryBuffer& geometry);

    bool FindGeometryMinMax(const GeometryBuffer& buffer, glm::vec3* minimums, glm::vec3* maximums);

} // namespace
