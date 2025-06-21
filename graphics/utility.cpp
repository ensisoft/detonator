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
#include "graphics/shader_source.h"

namespace {
    static const char* vertex_base = {
#include "shaders/vertex_shader_base.glsl"
    };
} // namespace

namespace gfx
{

ProgramPtr MakeProgram(const std::string& vertex_source,
                       const std::string& fragment_source,
                       const std::string& program_name,
                       Device& device)
{
    Shader::CreateArgs vertex_args;
    vertex_args.name   = program_name + "/VertexShader";
    vertex_args.source = vertex_source;

    Shader::CreateArgs fragment_args;
    fragment_args.name   = program_name + "/FragmentShader";
    fragment_args.source = fragment_source;

    auto vs = device.CreateShader(program_name + "/vs", vertex_args);
    auto fs = device.CreateShader(program_name + "/fs", fragment_args);
    if (!vs->IsValid())
        return nullptr;
    if (!fs->IsValid())
        return nullptr;

    Program::CreateArgs args;
    args.name = program_name;
    args.vertex_shader = vs;
    args.fragment_shader = fs;

    auto program = device.CreateProgram(program_name, args);
    if (!program->IsValid())
        return nullptr;

    return program;
}

GeometryPtr MakeFullscreenQuad(Device& device)
{
    if (auto geometry = device.FindGeometry("FullscreenQuad"))
        return geometry;

    const gfx::Vertex2D verts[] = {
        { {-1,  1}, {0, 1} },
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },

        { {-1,  1}, {0, 1} },
        { { 1, -1}, {1, 0} },
        { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.usage = Geometry::Usage::Static;
    args.content_name = "FullscreenQuad";
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.SetVertexLayout(GetVertexLayout<Vertex2D>());
    args.buffer.AddDrawCmd(Geometry::DrawType::Triangles);
    return device.CreateGeometry("FullscreenQuad", std::move(args));
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

gfx::ShaderSource MakeSimple2DVertexShader(const gfx::Device& device, bool use_instancing)
{
    // the varyings vParticleRandomValue, vParticleAlpha and vParticleTime
    // are used to support per particle features.
    // This shader doesn't provide that data but writes these varyings
    // nevertheless so that it's possible to use a particle shader enabled
    // material also with this shader.

    // the vertex model space  is defined in the lower right quadrant in
    // NDC (normalized device coordinates) (x grows right to 1.0 and
    // y grows up to 1.0 to the top of the screen).

    static const char* shader = {
#include "shaders/vertex_2d_simple_shader.glsl"
    };

    ShaderSource source;
    source.SetType(gfx::ShaderSource::Type::Vertex);
    if (use_instancing)
    {
        source.AddPreprocessorDefinition("INSTANCED_DRAW");
    }

    source.LoadRawSource(vertex_base);
    source.LoadRawSource(shader);
    source.AddShaderName("2D Vertex Shader");
    source.AddShaderSourceUri("shaders/vertex_base.glsl");
    source.AddShaderSourceUri("shaders/vertex_2d_simple_shader.glsl");
    source.AddDebugInfo("Instanced", use_instancing ? "YES" : "NO");
    return source;
}

gfx::ShaderSource MakeSimple3DVertexShader(const gfx::Device& device, bool use_instancing)
{
    static const char* shader = {
#include "shaders/vertex_3d_simple_shader.glsl"
    };

    ShaderSource source;
    source.SetVersion(gfx::ShaderSource::Version::GLSL_300);
    source.SetType(ShaderSource::Type::Vertex);
    if (use_instancing)
    {
        source.AddPreprocessorDefinition("INSTANCED_DRAW");
    }
    source.LoadRawSource(vertex_base);
    source.LoadRawSource(shader);
    source.AddShaderName("3D Vertex Shader");
    source.AddShaderSourceUri("shaders/vertex_base.glsl");
    source.AddShaderSourceUri("shaders/vertex_3d_simple_shader.glsl");
    source.AddDebugInfo("Instanced", use_instancing ? "YES" : "NO");
    return source;
}

gfx::ShaderSource MakeModel3DVertexShader(const gfx::Device& device, bool use_instancing)
{
    static const char* shader = {
#include "shaders/vertex_3d_model_shader.glsl"
    };

    ShaderSource source;
    source.SetVersion(gfx::ShaderSource::Version::GLSL_300);
    source.SetType(ShaderSource::Type::Vertex);
    if (use_instancing)
    {
        source.AddPreprocessorDefinition("INSTANCED_DRAW");
    }
    source.LoadRawSource(vertex_base);
    source.LoadRawSource(shader);
    source.AddShaderName("3D Model Shader");
    source.AddShaderSourceUri("shaders/vertex_base.glsl");
    source.AddShaderSourceUri("shaders/vertex_3d_model_shader.glsl");
    source.AddDebugInfo("Instanced", use_instancing ? "YES" : "NO");
    return source;
}

gfx::ShaderSource MakePerceptual3DVertexShader(const gfx::Device& device, bool use_instancing)
{
    static const char* shader = {
#include "shaders/vertex_perceptual_3d_shader.glsl"
    };

    ShaderSource source;
    source.SetVersion(gfx::ShaderSource::Version::GLSL_300);
    source.SetType(ShaderSource::Type::Vertex);
    if (use_instancing)
    {
        source.AddPreprocessorDefinition("INSTANCED_DRAW");
    }
    source.LoadRawSource(vertex_base);
    source.LoadRawSource(shader);
    source.AddShaderName("Perceptual 3D Shader");
    source.AddShaderSourceUri("shaders/vertex_base.glsl");
    source.AddShaderSourceUri("shaders/vertex_perceptual_3d_shader.glsl");
    source.AddDebugInfo("Instanced", use_instancing ? "YES" : "NO");
    return source;

}

} // namespace
