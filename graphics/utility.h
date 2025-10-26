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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

#include <string>

#include "base/assert.h"
#include "graphics/types.h"
#include "graphics/program.h"
#include "graphics/geometry.h"

// random helper and utility functions

namespace gfx
{
class Device;
class Geometry;
class ShaderSource;
class Texture;

struct Vec4;

Texture* PackDataTexture(const std::string& texture_id,
                         const std::string& texture_name,
                         const Vec4* data, size_t count,
                         Device& device);

template<typename Struct>
Texture* PackDataTexture(const std::string& texture_id,
                         const std::string& texture_name,
                         const Struct* data, size_t count,
                         Device& device)
{
    constexpr auto struct_size = sizeof(Struct);
    static_assert(struct_size % sizeof(Vec4) == 0);

    const auto byte_count = count * struct_size;
    const auto vec4_count = byte_count / sizeof(Vec4);
    ASSERT((byte_count % struct_size) == 0);

    return PackDataTexture(texture_id, texture_name,
                           reinterpret_cast<const Vec4*>(data), vec4_count, device);
}

ProgramPtr MakeProgram(const std::string& vertex_source,
                       const std::string& fragment_source,
                       const std::string& program_name,
                       Device& device);

GeometryPtr MakeFullscreenQuad(Device& device);

// Create an orthographic (parallel axis-aligned) projection matrix.
// Essentially this creates a logical viewport (and coordinate
// transformation) nto the scene to be rendered such that objects
// that are placed within the rectangle defined by the top left and
// bottom right coordinates are visible in the rendered scene.
// For example if left = 0.0f and width = 10.0f an object A that is
// 5.0f in width and is at coordinate -5.0f would not be shown.
// While an object B that is at 1.0f and is 2.0f units wide would be
// visible in the scene.
glm::mat4 MakeOrthographicProjection(const FRect& rect);
glm::mat4 MakeOrthographicProjection(float left, float top, float width, float height);
glm::mat4 MakeOrthographicProjection(float width, float height);
// Create an orthographic axis aligned projection matrix.
// Similar to the other overloads except that also maps the
// Z (distance from the camera) to a depth value.
// Remember that near and far should be positive values indicating
// the distance from the camera for the near/far planes. Not Z axis values.
glm::mat4 MakeOrthographicProjection(float left, float right, float top, float bottom, float near, float far);

// this allows ambiguous conversion on purpose so that code that uses
// a raw literal or raw floating point won't compile without explicitly
// mentioning the desired type
glm::mat4 MakePerspectiveProjection(FDegrees fov, float aspect, float znear, float zfar);
glm::mat4 MakePerspectiveProjection(FRadians fov, float aspect, float znear, float zfar);

ShaderSource MakeSimple2DVertexShader(const gfx::Device& device, bool use_instancing);
ShaderSource MakeSimple3DVertexShader(const gfx::Device& device, bool use_instancing);
ShaderSource MakeModel3DVertexShader(const gfx::Device& device, bool use_instancing);
ShaderSource MakePerceptual3DVertexShader(const gfx::Device& device, bool use_instancing);

} // namespace

