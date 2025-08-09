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

#include <string>
#include <variant>
#include <unordered_map>

#include "base/box.h"
#include "base/types.h"
#include "base/color4f.h"
#include "base/rotator.h"

namespace game
{
    // type aliases for base types
    using FBox      = base::FBox;
    using FRect     = base::FRect;
    using IRect     = base::IRect;
    using URect     = base::URect;
    using USize     = base::USize;
    using IPoint    = base::IPoint;
    using FPoint    = base::FPoint;
    using FSize     = base::FSize;
    using ISize     = base::ISize;
    using Color4f   = base::Color4f;
    using Rotator   = base::Rotator;
    using FRadians  = base::FRadians;
    using FDegrees  = base::FDegrees;
    using Color     = base::Color;
    using FVector2D = base::FVector2D;
    using Float2    = base::Float2;

    using LightParam = std::variant<float,
            glm::vec2, glm::vec3, glm::vec4,
            Color4f>;
    using LightParamMap = std::unordered_map<std::string, LightParam>;


} // namespace


