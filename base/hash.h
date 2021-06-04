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

#include <functional>

#include "base/types.h"
#include "base/color4f.h"
#include "base/bitflag.h"

namespace base
{

template<typename S, typename T> inline
S hash_combine(S seed, T value)
{
    const auto hash = std::hash<T>()(value);
    seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

template<typename S> inline
S hash_combine(S seed, const glm::vec2& value)
{
    seed = hash_combine(seed, value.x);
    seed = hash_combine(seed, value.y);
    return seed;
}

template<typename S> inline
S hash_combine(S seed, const glm::vec3& value)
{
    seed = hash_combine(seed, value.x);
    seed = hash_combine(seed, value.y);
    seed = hash_combine(seed, value.z);
    return seed;
}

template<typename S> inline
S hash_combine(S seed, const glm::vec4& value)
{
    seed = hash_combine(seed, value.x);
    seed = hash_combine(seed, value.y);
    seed = hash_combine(seed, value.z);
    seed = hash_combine(seed, value.w);
    return seed;
}

template<typename S, typename T> inline
S hash_combine(S seed, const Size<T>& size)
{
    seed = hash_combine(seed, size.GetWidth());
    seed = hash_combine(seed, size.GetHeight());
    return seed;
}

template<typename S, typename T> inline
S hash_combine(S seed, const Point<T>& point)
{
    seed = hash_combine(seed, point.GetX());
    seed = hash_combine(seed, point.GetY());
    return seed;
}

template<typename S, typename T> inline
S hash_combine(S seed, const Rect<T>& rect)
{
    seed = hash_combine(seed, rect.GetX());
    seed = hash_combine(seed, rect.GetY());
    seed = hash_combine(seed, rect.GetWidth());
    seed = hash_combine(seed, rect.GetHeight());
    return seed;
}

template<typename S> inline
S hash_combine(S seed, const Color4f& color)
{
    seed = hash_combine(seed, color.Red());
    seed = hash_combine(seed, color.Green());
    seed = hash_combine(seed, color.Blue());
    seed = hash_combine(seed, color.Alpha());
    return seed;
}

template<typename S, typename Enum> inline
S hash_combine(S seed, const bitflag<Enum>& bits)
{
    seed = hash_combine(seed, bits.value());
    return seed;
}

} // base
