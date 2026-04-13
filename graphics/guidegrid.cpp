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

#include "base/assert.h"
#include "base/utility.h"
#include "base/format.h"
#include "base/hash.h"
#include "graphics/guidegrid.h"

#include "device.h"
#include "graphics/program.h"
#include "graphics/shader_source.h"
#include "graphics/utility.h"

namespace gfx
{

bool Grid::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device&, ProgramState& program, RasterState& state) const
{
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    return true;
}

std::string Grid::GetShaderId(const Environment& env) const
{
    // we're not supporting instancing.
    ASSERT(env.use_instancing == false);

    return Drawable::GetShaderId(env.use_instancing, false, Shader::Simple2D);
}

ShaderSource Grid::GetShader(const Environment& env, const Device& device) const
{
    // we're not supporting instancing.
    ASSERT(env.use_instancing == false);

    return Drawable::CreateShader(env.use_instancing, false, device, Shader::Simple2D);
}

std::string Grid::GetShaderName(const Environment& env) const
{
    return Drawable::GetShaderName(env, Shader::Simple2D);
}

DrawGeometryHandle Grid::GetGeometry(const Environment& env, Device& device) const
{
    // use the content properties to generate a name for the
    // gpu side geometry.
    size_t hash = 0;
    hash = base::hash_combine(hash, mNumVerticalLines);
    hash = base::hash_combine(hash, mNumHorizontalLines);
    hash = base::hash_combine(hash, mBorderLines);
    const auto id = std::to_string(hash);

    if (auto geometry = device.FindGeometry(id))
        return std::move(geometry);

    auto buffer = Construct(env);

    Geometry::CreateArgs args;
    args.buffer = buffer.TransferGeometryBuffer();
    args.usage  = BufferUsage::Static;
    args.content_hash = hash;
    args.content_name = base::FormatString("Grid %1x%2", mNumVerticalLines+1, mNumHorizontalLines+1);
    return device.CreateGeometry(id, std::move(args));
}

DrawGeometryBuffer Grid::Construct(const Environment&) const
{
    std::vector<Vertex2D> verts;

    const float yadvance = 1.0f / (mNumHorizontalLines + 1);
    const float xadvance = 1.0f / (mNumVerticalLines + 1);
    for (unsigned i=1; i<=mNumVerticalLines; ++i)
    {
        const float x = i * xadvance;
        const Vertex2D line[2] = {
            {{x,  0.0f}, {x, 0.0f}},
            {{x, -1.0f}, {x, 1.0f}}
        };
        verts.push_back(line[0]);
        verts.push_back(line[1]);
    }
    for (unsigned i=1; i<=mNumHorizontalLines; ++i)
    {
        const float y = i * yadvance;
        const Vertex2D line[2] = {
            {{0.0f, y*-1.0f}, {0.0f, y}},
            {{1.0f, y*-1.0f}, {1.0f, y}},
        };
        verts.push_back(line[0]);
        verts.push_back(line[1]);
    }
    if (mBorderLines)
    {
        const Vertex2D corners[4] = {
            // top left
            {{0.0f, 0.0f}, {0.0f, 0.0f}},
            // top right
            {{1.0f, 0.0f}, {1.0f, 0.0f}},

            // bottom left
            {{0.0f, -1.0f}, {0.0f, 1.0f}},
            // bottom right
            {{1.0f, -1.0f}, {1.0f, 1.0f}}
        };
        verts.push_back(corners[0]);
        verts.push_back(corners[1]);
        verts.push_back(corners[2]);
        verts.push_back(corners[3]);
        verts.push_back(corners[0]);
        verts.push_back(corners[2]);
        verts.push_back(corners[1]);
        verts.push_back(corners[3]);
    }
    GeometryBuffer buffer;
    buffer.SetVertexBuffer(std::move(verts));
    buffer.AddDrawCmd(Geometry::DrawType::Lines);
    buffer.SetVertexLayout(GetVertexLayout<Vertex2D>());
    return std::move(buffer);
}

Drawable::Type Grid::GetType() const
{
    return Type::GuideGrid;
}

Drawable::DrawPrimitive Grid::GetDrawPrimitive() const
{
    return DrawPrimitive::Lines;
}

SpatialMode Grid::GetSpatialMode() const
{
    return SpatialMode::Flat2D;
}

} // namespace