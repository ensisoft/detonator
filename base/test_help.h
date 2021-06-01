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

// this file should only be included in unit test files.

#include "warnpush.h"
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include "base/test_float.h"
#include "base/test_minimal.h"
#include "base/color4f.h"
#include "base/types.h"

namespace base {

static bool operator==(const Color4f& lhs, const Color4f& rhs)
{
    return real::equals(lhs.Red(), rhs.Red()) &&
           real::equals(lhs.Green(), rhs.Green()) &&
           real::equals(lhs.Blue(), rhs.Blue()) &&
           real::equals(lhs.Alpha(), rhs.Alpha());
}
static bool operator!=(const Color4f& lhs, const Color4f& rhs)
{ return !(lhs == rhs); }

static bool operator==(const FRect& lhs, const FRect& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
           real::equals(lhs.GetY(), rhs.GetY()) &&
           real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
           real::equals(lhs.GetHeight(), rhs.GetHeight());
}
static bool operator!=(const FRect& lhs, const FRect& rhs)
{ return !(lhs == rhs); }

static bool operator==(const FSize& lhs, const FSize& rhs)
{
    return real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
           real::equals(lhs.GetHeight(), rhs.GetHeight());
}
static bool operator!=(const FSize& lhs, const FSize& rhs)
{ return !(lhs == rhs); }

static bool operator==(const FPoint& lhs, const FPoint& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
           real::equals(lhs.GetY(), rhs.GetY());
}
static bool operator!=(const FPoint& lhs, const FPoint& rhs)
{ return !(lhs == rhs); }

} // base

namespace glm {
static bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{ return real::equals(lhs.x, rhs.x) && real::equals(lhs.y, rhs.y); }
static bool operator!=(const glm::vec2& lhs, const glm::vec2& rhs)
{ return !(lhs == rhs); }

} // glm