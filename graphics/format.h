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

#include <cstdio>
#include <string>

#include "graphics/types.h"

namespace gfx
{
// basic formatting functionality for some simple types.
// todo: move the JSON functionality here.
inline std::string ToString(const FRect& rect)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),"x:%.2f, y:%.2f, w:%.2f, h:%.2f",
                  rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight());
    return buff;
}

inline std::string ToString(const FSize& size)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "w:%.2f, h:%.2f",
                  size.GetWidth(), size.GetHeight());
    return buff;
}

inline std::string ToString(const FPoint& point)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "x:%.2f, y:%.2f",
                  point.GetX(), point.GetY());
    return buff;
}

} // namespace