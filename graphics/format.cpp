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

#include "config.h"

#include <cstdio>

#include "graphics/format.h"

namespace gfx
{

std::string ToString(const Vec2& vec)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),"[%.2f %.2f]", vec.x, vec.y);
    return buff;
}
std::string ToString(const Vec3& vec)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),"[%.2f %.2f %.2f]", vec.x, vec.y, vec.z);
    return buff;
}
std::string ToString(const Vec4& vec)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "[%.2f %.2f %.2f %.2f]", vec.x, vec.y, vec.z, vec.w);
    return buff;
}

} // namespace