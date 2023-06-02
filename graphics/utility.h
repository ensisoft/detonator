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

#include "graphics/types.h"

// random helper and utility functions

namespace gfx
{
class Program;
class Device;
class Geometry;

Program* MakeProgram(const std::string& vertex_source,
                     const std::string& fragment_source,
                     const std::string& program_name,
                     Device& device);

Geometry* MakeFullscreenQuad(Device& device);

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

} // namespace

