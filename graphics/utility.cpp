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

#include "warnpush.h"
#  include <glm/gtc/matrix_transform.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "graphics/utility.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"

namespace gfx
{

Program* MakeProgram(const std::string& vertex_source,
                     const std::string& fragment_source,
                     const std::string& program_name,
                     Device& device)
{
    auto* vs = device.MakeShader(program_name + "/vertex-shader");
    auto* fs = device.MakeShader(program_name + "/fragment-shader");
    vs->SetName(program_name + "/vertex-shader");
    fs->SetName(program_name + "/fragment-shader");
    if (!vs->CompileSource(vertex_source))
        return nullptr;
    if (!fs->CompileSource(fragment_source))
        return nullptr;

    auto* program = device.MakeProgram(program_name);
    program->SetName(program_name);
    if (!program->Build(vs, fs))
        return nullptr;

    return program;
}

Geometry* MakeFullscreenQuad(Device& device)
{
    if (auto* geometry = device.FindGeometry("FullscreenQuad"))
        return geometry;

    auto* geometry = device.MakeGeometry("FullscreenQuad");
    const gfx::Vertex2D verts[] = {
        { {-1,  1}, {0, 1} },
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },

        { {-1,  1}, {0, 1} },
        { { 1, -1}, {1, 0} },
        { { 1,  1}, {1, 1} }
    };
    geometry->SetName("FullscreenQuad");
    geometry->SetVertexBuffer(verts, 6, gfx::Geometry::Usage::Static);
    geometry->AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    return geometry;
}

glm::mat4 MakeOrthographicProjection(const FRect& rect)
{
    const auto left   = rect.GetX();
    const auto right  = rect.GetWidth() + left;
    const auto top    = rect.GetY();
    const auto bottom = rect.GetHeight() + top;
    return glm::ortho(left , right , bottom , top);
}

glm::mat4 MakeOrthographicProjection(float left, float top, float width, float height)
{
    return glm::ortho(left, left + width, top + height, top);
}
glm::mat4 MakeOrthographicProjection(float width, float height)
{
    return glm::ortho(0.0f, width, height, 0.0f);
}
glm::mat4 MakeOrthographicProjection(float left, float right, float top, float bottom, float near, float far)
{
    return glm::ortho(left, right, bottom, top, near, far);
}

glm::mat4 MakePerspectiveProjection(FDegrees fov, float aspect, float znear, float zfar)
{
    return glm::perspective(fov.ToRadians(),  aspect, znear, zfar);
}

glm::mat4 MakePerspectiveProjection(FRadians fov, float aspect, float znear, float zfar)
{
    return glm::perspective(fov.ToRadians(), aspect, znear, zfar);
}

} // namespace
