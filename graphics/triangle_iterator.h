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

#include <vector>
#include <limits>

#include "base/assert.h"
#include "graphics/enum.h"
#include "graphics/types.h"
#include "graphics/vertex_buffer.h"

namespace gfx
{
    class TriangleIterator
    {
    public:
        using IndexType   = gfx::IndexType;
        using DrawCommand = gfx::DrawCommand;
        using CommandStream = std::vector<DrawCommand>;

        struct Triangle {
            uint32_t cmd_index = 0;
            uint32_t i0 = 0;
            uint32_t i1 = 0;
            uint32_t i2 = 0;
        };
        explicit TriangleIterator(const CommandStream& commands,
                                  const VertexStream& vertices)
        {
            const auto vertex_count = vertices.GetCount();
            for (uint32_t cmd_index=0; cmd_index<commands.size(); ++cmd_index)
            {
                const auto& cmd = commands[cmd_index];
                const auto primitive_count = cmd.count == std::numeric_limits<uint32_t>::max() ? vertex_count : cmd.count;
                if (!ValidateCommand(cmd, primitive_count))
                    continue;
                CreateTriangles(cmd, cmd_index, primitive_count, nullptr);
            }
        }
        TriangleIterator(const CommandStream& commands,
                         const VertexStream& vertices,
                         const IndexStream& indices)
        {
            const auto index_count = indices.GetCount();
            for (uint32_t cmd_index=0; cmd_index<commands.size(); ++cmd_index)
            {
                const auto& cmd = commands[cmd_index];
                const auto primitive_count = cmd.count == std::numeric_limits<uint32_t>::max() ? index_count : cmd.count;
                if (!ValidateCommand(cmd, primitive_count))
                    continue;;
                CreateTriangles(cmd, cmd_index, primitive_count, &indices);
            }
        }

        auto GetCount() const noexcept
        {
            return mTriangles.size();
        }

        const auto& GetTriangle(size_t index) const noexcept
        {
            return mTriangles[index];
        }
    private:
        void CreateTriangles(const DrawCommand& cmd, uint32_t cmd_index, uint32_t primitive_count,
            const IndexStream* indices)
        {
            if (cmd.type == DrawType::Triangles)
            {
                const auto triangles = primitive_count / 3;
                for (uint32_t j=0; j<triangles; ++j)
                {
                    const auto start = cmd.offset + j * 3;

                    Triangle triangle;
                    triangle.cmd_index = cmd_index;
                    triangle.i0 = indices ? indices->GetIndex(start+0) : start+0;
                    triangle.i1 = indices ? indices->GetIndex(start+1) : start+1;
                    triangle.i2 = indices ? indices->GetIndex(start+2) : start+2;
                    mTriangles.push_back(triangle);
                }
            }
            else if (cmd.type == DrawType::TriangleFan)
            {
                // the first 3 vertices form a triangle and then
                // every subsequent vertex creates another triangle with
                // the first and previous vertex.
                const uint32_t apex = indices ? indices->GetIndex(cmd.offset+0) : cmd.offset+0;

                Triangle first;
                first.cmd_index = cmd_index;
                first.i0 = indices ? indices->GetIndex(cmd.offset+0) : cmd.offset+0;
                first.i1 = indices ? indices->GetIndex(cmd.offset+1) : cmd.offset+1;
                first.i2 = indices ? indices->GetIndex(cmd.offset+2) : cmd.offset+2;
                mTriangles.push_back(first);

                for (size_t j=3; j<primitive_count; ++j)
                {
                    const auto start = cmd.offset + j;
                    Triangle triangle;
                    triangle.cmd_index = cmd_index;
                    triangle.i0 = apex;
                    triangle.i1 = indices ? indices->GetIndex(start-1) : start-1;
                    triangle.i2 = indices ? indices->GetIndex(start-0) : start-0;
                    mTriangles.push_back(triangle);
                }
            }
            else if (cmd.type == DrawType::TriangleStrip)
            {
                // the first 3 vertices form a triangle and then every subsequent
                // vertex creates another triangle with the previous two vertices.
                // the order between the last two vertices flip-flops based on the
                // index being odd or even.
                Triangle first;
                first.cmd_index = cmd_index;
                first.i0 = indices ? indices->GetIndex(cmd.offset+0) : cmd.offset+0;
                first.i1 = indices ? indices->GetIndex(cmd.offset+1) : cmd.offset+1;
                first.i2 = indices ? indices->GetIndex(cmd.offset+2) : cmd.offset+2;
                mTriangles.push_back(first);

                for (size_t j=3; j<primitive_count; ++j)
                {
                    const auto start = cmd.offset + j;
                    const uint32_t i1 = indices ? indices->GetIndex(start-1) : start-1;
                    const uint32_t i2 = indices ? indices->GetIndex(start-2) : start-2;
                    const bool is_odd = (j & 0x1) == 0x1;
                    if (is_odd)
                    {
                        Triangle triangle;
                        triangle.cmd_index = cmd_index;
                        triangle.i0 = start;
                        triangle.i1 = i1;
                        triangle.i2 = i2;
                        mTriangles.push_back(triangle);
                    }
                    else
                    {
                        Triangle triangle;
                        triangle.cmd_index = cmd_index;
                        triangle.i0 = start;
                        triangle.i1 = i2;
                        triangle.i2 = i1;
                        mTriangles.push_back(triangle);
                    }
                }
            }
        }
    private:
        static bool ValidateCommand(const DrawCommand& cmd, uint32_t primitive_count)
        {
            if (cmd.type == DrawType::Triangles)
            {
                if (primitive_count % 3)
                    return false;
            }
            else if (cmd.type == DrawType::TriangleStrip || cmd.type == DrawType::TriangleFan)
            {
                if (primitive_count < 3)
                    return false;
            }
            else if (cmd.type == DrawType::LineLoop || cmd.type == DrawType::Lines || cmd.type == DrawType::Points)
                return false;

            return true;
        }

    private:
        std::vector<Triangle> mTriangles;
    };
} // namespace