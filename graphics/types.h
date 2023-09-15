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

#include "base/assert.h"
#include "base/types.h"

namespace gfx
{
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


} // gfx