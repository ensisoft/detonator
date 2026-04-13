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
#include "base/hash.h"
#include "base/format.h"
#include "graphics/linebatch.h"

#include "device.h"
#include "graphics/utility.h"
#include "graphics/shader_source.h"
#include "graphics/vertex.h"
#include "graphics/program.h"

namespace gfx
{

bool LineBatch2D::ApplyDynamicState(const Environment &environment, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device&, ProgramState &program, RasterState &state) const
{
    program.SetUniform("kProjectionMatrix",  *environment.proj_matrix);
    program.SetUniform("kModelViewMatrix", *environment.view_matrix * *environment.model_matrix);
    return true;
}

ShaderSource LineBatch2D::GetShader(const Environment& env, const Device& device) const
{
    // we're not supporting instancing.
    ASSERT(env.use_instancing == false);

    return Drawable::CreateShader(env.use_instancing, false, device, Shader::Simple2D);
}

std::string LineBatch2D::GetShaderId(const Environment& env) const
{
    // we're not supporting instancing.
    ASSERT(env.use_instancing == false);

    return Drawable::GetShaderId(env.use_instancing, false, Shader::Simple2D);
}

std::string LineBatch2D::GetShaderName(const Environment& env) const
{
    return Drawable::GetShaderName(env, Shader::Simple2D);
}

DrawGeometryHandle LineBatch2D::GetGeometry(const Environment& env, Device& device) const
{
    auto buffer = Construct(env);

    Geometry::CreateArgs args;
    args.content_hash = 0;
    args.content_name = "2D Line Batch";
    args.buffer = buffer.TransferGeometryBuffer();
    args.usage  = BufferUsage::Stream;
    return device.CreateGeometry("line-buffer-2d", std::move(args));
}

DrawGeometryBuffer LineBatch2D::Construct(const Environment& environment) const
{
    std::vector<Vertex2D> vertices;
    for (const auto& line : mLines)
    {
        Vertex2D a;
        // the -y hack exists because we're using the generic 2D vertex
        // shader that the shapes with triangle rasterization also use.
        a.aPosition = Vec2 { line.start.x, -line.start.y };
        Vertex2D b;
        b.aPosition = Vec2 { line.end.x, -line.end.y };
        vertices.push_back(a);
        vertices.push_back(b);
    }

    GeometryBuffer buffer;
    buffer.SetVertexBuffer(vertices);
    buffer.SetVertexLayout(GetVertexLayout<Vertex2D>());
    buffer.AddDrawCmd(Geometry::DrawType::Lines);
    return std::move(buffer);
}

Drawable::DrawPrimitive LineBatch2D::GetDrawPrimitive() const
{
    return DrawPrimitive::Lines;
}

SpatialMode LineBatch2D::GetSpatialMode() const
{
    return SpatialMode::Flat2D;
}

Drawable::Type LineBatch2D::GetType() const
{
    return Type::LineBatch2D;
}

bool LineBatch3D::ApplyDynamicState(const Environment& environment, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device&, ProgramState& program, RasterState& state) const
{
    program.SetUniform("kProjectionMatrix",  *environment.proj_matrix);
    program.SetUniform("kModelViewMatrix", *environment.view_matrix * *environment.model_matrix);
    return true;
}

ShaderSource LineBatch3D::GetShader(const Environment& env, const Device& device) const
{
    // we're not supporting instancing.
    ASSERT(env.use_instancing == false);

    return Drawable::CreateShader(env.use_instancing, false, device, Shader::Simple3D);
}

std::string LineBatch3D::GetShaderId(const Environment& env) const
{
    // we're not supporting instancing.
    ASSERT(env.use_instancing == false);

    return Drawable::GetShaderId(env.use_instancing, false, Shader::Simple3D);
}

std::string LineBatch3D::GetShaderName(const Environment& env) const
{
    return Drawable::GetShaderName(env, Shader::Simple3D);
}

DrawGeometryHandle LineBatch3D::GetGeometry(const Environment& env, Device& device) const
{
    auto buffer = Construct(env);

    Geometry::CreateArgs args;
    args.content_hash = 0;
    args.content_name = "3D Line Batch";
    args.buffer = buffer.TransferGeometryBuffer();
    args.usage  = BufferUsage::Stream;
    return device.CreateGeometry("line-buffer-3d", std::move(args));
}

DrawGeometryBuffer LineBatch3D::Construct(const Environment& env) const
{
    // it's also possible to draw without generating geometry by simply having
    // the two line end points as uniforms in the vertex shader and then using
    // gl_VertexID (which is not available in GL ES2) to distinguish the vertex
    // invocation and use that ID to choose the right vertex end point.
    std::vector<Vertex3D> vertices;
    for (const auto& line : mLines)
    {
        Vertex3D a;
        a.aPosition = Vec3 { line.start.x, line.start.y, line.start.z };
        Vertex3D b;
        b.aPosition = Vec3 { line.end.x, line.end.y, line.end.z };

        vertices.push_back(a);
        vertices.push_back(b);
    }

    GeometryBuffer buffer;
    buffer.SetVertexBuffer(vertices);
    buffer.SetVertexLayout(GetVertexLayout<Vertex3D>());
    buffer.AddDrawCmd(Geometry::DrawType::Lines);
    return std::move(buffer);
}

Drawable::DrawPrimitive LineBatch3D::GetDrawPrimitive() const
{
    return DrawPrimitive::Lines;
}

SpatialMode LineBatch3D::GetSpatialMode() const
{
    return SpatialMode::True3D;
}

Drawable::Type LineBatch3D::GetType() const
{
    return Type::LineBatch3D;
}

} // namespace