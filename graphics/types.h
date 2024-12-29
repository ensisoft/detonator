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
#  include <glm/vec4.hpp>
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

#include <cstdint>
#include <variant>
#include <unordered_map>

#include "base/assert.h"
#include "base/types.h"
#include "graphics/color4f.h"

namespace gfx
{
    enum class RenderPass {
        ColorPass,
        StencilPass,
        CustomPass = 100
    };

    // Text alignment inside a rect
    enum TextAlign {
        // Vertical text alignment
        AlignTop     = 0x1,
        AlignVCenter = 0x2,
        AlignBottom  = 0x4,
        // Horizontal text alignment
        AlignLeft    = 0x10,
        AlignHCenter = 0x20,
        AlignRight   = 0x40
    };

    enum TextProp {
        None      = 0x0,
        Underline = 0x1,
        Blinking  = 0x2
    };

    // Specify common usage hint for a GPU buffer such as
    // vertex buffer, index buffer etc.
    enum class BufferUsage {
        // The buffer is updated once and used multiple times.
        Static,
        // The buffer is updated multiple times and used once/few times.
        Stream,
        // The buffer is updated multiple times and used multiple times.
        Dynamic
    };

    // Style of the drawable's geometry determines how the geometry
    // is to be rasterized.
    enum class DrawPrimitive {
        Points, Lines, Triangles
    };

    // broad category of drawables for describing the semantic
    // meaning of the drawable. Each drawable in this category
    // has specific constraint and specific requirements in order
    // for the drawable and material to work together.
    //
    // Category    Particle Data  Tile Data  Tris Points  Lines
    //----------------------------------------------------------
    // Basic                                  *     *       *
    // Particles        *                           *       *
    // TileBatch                     *        *     *
    //
    // In other words if we're for example rendering particles
    // then the material can expect there to be per particle data
    // and the rasterizer draw primitive is either POINTS or LINES.
    //
    // If any material's shader source is written to support
    // multiple different drawable types then care must be taken
    // to ensure that the right set of varyings are exposed  in
    // the vertex-fragment shader interface. In other words if we
    // have a fragment shader that can module it's output with per
    // particle alpha but the shader is used with a non-particle
    // drawable then there's not going to be incoming per particle
    // alpha value which means that if the fragment shader has
    // 'in float vParticleAlpha' it will cause a program link error.
    enum class DrawCategory {
        // Lines, line batches, polygons, simple shapes such as rect
        // round rect, circle, 2D and 3D
        Basic,
        // Particles with per particle data.
        Particles,
        // Tiles with per tile data.
        TileBatch
    };

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