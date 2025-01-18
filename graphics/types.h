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
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

#include <cstdint>
#include <variant>
#include <unordered_map>

#include "base/assert.h"
#include "base/types.h"
#include "graphics/enum.h"
#include "graphics/color4f.h"

namespace gfx
{

    // type aliases for base types for gfx.
    using FPoint = base::FPoint;
    using IPoint = base::IPoint;
    using UPoint = base::UPoint;
    using FSize  = base::FSize;
    using ISize  = base::ISize;
    using USize  = base::USize;
    using FRect  = base::FRect;
    using IRect  = base::IRect;
    using URect  = base::URect;
    using FCircle = base::FCircle;
    using FRadians = base::FRadians;
    using FDegrees = base::FDegrees;

    struct BasicLight {
        BasicLightType type = BasicLightType::Ambient;
        // lights position in view space. in other words the
        // result of transforming the light with the light's model
        // view matrix.
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        // light's direction vector that applies to spot and
        // directional lights.
        glm::vec3 direction = {0.0f, 0.0f, -1.0f};
        gfx::Color4f ambient_color;
        gfx::Color4f diffuse_color;
        gfx::Color4f specular_color;
        gfx::FDegrees spot_half_angle;
        float constant_attenuation = 1.0f;
        float linear_attenuation = 0.0f;
        float quadratic_attenuation = 0.0f;
    };

    struct BasicFog {
        gfx::Color4f color;
        float start_dist = 10.0f;
        float end_dist = 100.0f;
        float density = 1.0f;
        BasicFogMode mode = BasicFogMode::Linear;
    };


    struct Quad {
        glm::vec4 top_left;
        glm::vec4 bottom_left;
        glm::vec4 bottom_right;
        glm::vec4 top_right;
    };

    inline Quad TransformQuad(const Quad& q, const glm::mat4& mat)
    {
        Quad ret;
        ret.top_left     = mat * q.top_left;
        ret.bottom_left  = mat * q.bottom_left;
        ret.bottom_right = mat * q.bottom_right;
        ret.top_right    = mat * q.top_right;
        return ret;
    }

    template<unsigned>
    struct StencilValue {
        uint8_t value;
        StencilValue() = default;
        StencilValue(uint8_t val) : value(val) {}
        inline operator uint8_t () const { return value; }

        inline StencilValue operator++(int) {
            ASSERT(value < 0xff);
            auto ret = value;
            ++value;
            return StencilValue(ret);
        }
        inline StencilValue& operator++() {
            ASSERT(value < 0xff);
            ++value;
            return *this;
        }
        inline StencilValue operator--(int) {
            ASSERT(value > 0);
            auto ret = value;
            --value;
            return StencilValue(ret);
        }
        inline StencilValue& operator--() {
            ASSERT(value > 0);
            --value;
            return *this;
        }
    };

    using StencilClearValue = StencilValue<0>;
    using StencilWriteValue = StencilValue<1>;
    using StencilPassValue  = StencilValue<2>;

    // Material/Shader uniform/params
    using Uniform = std::variant<float, int,
            std::string,
            gfx::Color4f,
            glm::vec2, glm::vec3, glm::vec4>;
    using UniformMap = std::unordered_map<std::string, Uniform>;

} // gfx