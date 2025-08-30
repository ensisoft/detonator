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

#include "graphics/linebatch.h"
#include "graphics/utility.h"
#include "graphics/shader_source.h"
#include "graphics/vertex.h"
#include "graphics/program.h"

namespace gfx
{

bool LineBatch2D::ApplyDynamicState(const Environment &environment, ProgramState &program, RasterState &state) const
{
    program.SetUniform("kProjectionMatrix",  *environment.proj_matrix);
    program.SetUniform("kModelViewMatrix", *environment.view_matrix * *environment.model_matrix);
    return true;
}

ShaderSource LineBatch2D::GetShader(const Environment& environment, const Device& device) const
{
    return MakeSimple2DVertexShader(device, false, false);
}

std::string LineBatch2D::GetShaderId(const Environment& environment) const
{
    return "simple-2D-vertex-shader";
}

std::string LineBatch2D::GetShaderName(const Environment& environment) const
{
    return "Simple2DVertexShader";
}

std::string LineBatch2D::GetGeometryId(const Environment &environment) const
{
    return "line-buffer-2d";
}
bool LineBatch2D::Construct(const Environment& environment, Geometry::CreateArgs& create) const
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
    create.content_name = "2D Line Batch";
    create.usage = Geometry::Usage::Stream;
    auto& geometry = create.buffer;

    geometry.SetVertexBuffer(vertices);
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
    return true;
}

bool LineBatch3D::ApplyDynamicState(const Environment& environment, ProgramState& program, RasterState& state) const
{
    program.SetUniform("kProjectionMatrix",  *environment.proj_matrix);
    program.SetUniform("kModelViewMatrix", *environment.view_matrix * *environment.model_matrix);
    return true;
}

ShaderSource LineBatch3D::GetShader(const Environment& environment, const Device& device) const
{
    return MakeSimple3DVertexShader(device, false);
}

std::string LineBatch3D::GetShaderId(const Environment& environment) const
{
    return "simple-3D-vertex-shader";
}

std::string LineBatch3D::GetShaderName(const Environment& environment) const
{
    return "Simple3DVertexShader";
}

std::string LineBatch3D::GetGeometryId(const Environment& environment) const
{
    return "line-buffer-3D";
}

bool LineBatch3D::Construct(const Environment& environment, Geometry::CreateArgs& create) const
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

    create.content_name = "3D Line Batch";
    create.usage = Geometry::Usage::Stream;
    auto& geometry = create.buffer;

    geometry.SetVertexBuffer(vertices);
    geometry.SetVertexLayout(GetVertexLayout<Vertex3D>());
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
    return true;
}


} // namespace