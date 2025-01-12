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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#  include <glm/mat4x4.hpp>
#  include <glm/mat3x3.hpp>
#  include <glm/mat2x2.hpp>
#include "warnpop.h"

#include <string>
#include <variant>

#include "base/color4f.h"

namespace dev
{
    using UniformValType = std::variant<int, float,
            base::Color4f,
            glm::ivec2,
            glm::vec2,
            glm::vec3,
            glm::vec4,
            glm::mat2,
            glm::mat3,
            glm::mat4>;
    using UniformKeyType = std::string;

    struct Uniform {
        UniformKeyType name;
        UniformValType value;
    };

} // namespace