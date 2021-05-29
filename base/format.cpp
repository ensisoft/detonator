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

#include "config.h"

#include <cstdio>
#include <cassert>
#include "format.h"

#if defined(BASE_FORMAT_SUPPORT_GLM)

namespace glm
{
namespace detail {

std::ostream& operator << (std::ostream& out, const mat4& m)
{
    const auto& x = m[0];
    const auto& y = m[1];
    const auto& z = m[2];
    const auto& w = m[3];

    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
       "%.2f %.2f %.2f %.2f\n"
       "%.2f %.2f %.2f %.2f\n"
       "%.2f %.2f %.2f %.2f\n"
       "%.2f %.2f %.2f %.2f\n",
       x[0], y[0], z[0], w[0],
       x[1], y[1], z[1], w[1],
       x[2], y[2], z[2], w[2],
       x[3], y[3], z[3], w[3]);
    out << buff;
    return out;
}

std::ostream& operator << (std::ostream& out, const mat3& m)
{
    const auto& x = m[0];
    const auto& y = m[1];
    const auto& z = m[2];

    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
       "%.2f %.2f %.2f\n"
       "%.2f %.2f %.2f\n"
       "%.2f %.2f %.2f\n",
       x[0], y[0], z[0],
       x[1], y[1], z[1],
       x[2], y[2], z[2]);
    out << buff;
    return out;
}

std::ostream& operator<<(std::ostream& out, const glm::vec4& v)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
        "[%.2f %.2f %.2f %.2f]", v[0], v[1], v[2], v[3]);
    out << buff;
    return out;
}

std::ostream& operator<<(std::ostream& out, const glm::vec3& v)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
        "[%.2f %.2f %.2f]", v[0], v[1], v[2]);
    out << buff;
    return out;
}

std::ostream& operator<<(std::ostream& out, const glm::vec2& v)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
        "[%.2f %.2f]", v[0], v[1]);
    out << buff;
    return out;
}
} // detail
} // glm

#endif // BASE_FORMAT_SUPPORT_GLM

#if defined(BASE_FORMAT_SUPPORT_QT)
  #include "warnpush.h"
    #include <QString>
  #include "warnpop.h"

std::ostream& operator<<(std::ostream& out, const QString& str)
{
    out << str.toStdString();
    return out;
}
#endif

