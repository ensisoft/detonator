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
#include <locale>
#include <codecvt>
#include "base/types.h"
#include "base/format.h"

#if defined(__MSVC__)
#  pragma warning(push)
#  pragma warning(disable: 4996) // deprecated use of wstring_convert
#endif

namespace base {
namespace detail {
#if defined(BASE_FORMAT_SUPPORT_GLM)
std::string ToString(const glm::mat4& m)
{
    const auto& x = m[0];
    const auto& y = m[1];
    const auto& z = m[2];
    const auto& w = m[3];

    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
       "[%.2f %.2f %.2f %.2f],"
       "[%.2f %.2f %.2f %.2f],"
       "[%.2f %.2f %.2f %.2f],"
       "[%.2f %.2f %.2f %.2f]",
       x[0], x[1], x[2], x[3],
       y[0], y[1], y[2], y[3],
       z[0], z[1], z[2], x[3],
       w[0], w[1], w[2], w[3]);
    return buff;
}
std::string ToString(const glm::mat3& m)
{
    const auto& x = m[0];
    const auto& y = m[1];
    const auto& z = m[2];
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
        "[%.2f %.2f %.2f],"
        "[%.2f %.2f %.2f],"
        "[%.2f %.2f %.2f]",
        x[0], x[1], x[2],
        y[0], y[1], y[2],
        z[0], z[1], z[2]);
    return buff;
}

std::string ToString(const glm::vec4& v)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
       "[%.2f %.2f %.2f %.2f]", v[0], v[1], v[2], v[3]);
    return buff;
}

std::string ToString(const glm::vec3& v)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
        "[%.2f %.2f %.2f]", v[0], v[1], v[2]);
    return buff;
}

std::string ToString(const glm::vec2& v)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
        "[%.2f %.2f]", v[0], v[1]);
    return buff;
}
#endif // BASE_FORMAT_SUPPORT_GLM

std::string ToString(const FRect& rect)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "x:%.2f, y:%.2f, w:%.2f, h:%.2f",
        rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight());
    return buff;
}

std::string ToString(const FSize& size)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "w:%.2f, h:%.2f",
        size.GetWidth(), size.GetHeight());
    return buff;
}

std::string ToString(const FPoint& point)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "x:%.2f, y:%.2f",
        point.GetX(), point.GetY());
    return buff;
}

std::string ToString(const Color4f& color)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "r:%.2f, g:%.2f, b:%.2f, a:%.2f",
        color.Red(), color.Green(), color.Blue(), color.Alpha());
    return buff;
}

std::string ToString(const std::wstring& s)
{
    //setup converter
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::string converted_str = converter.to_bytes(s);
    return converted_str;
}

} // detail
} // namespace


namespace base
{
std::string ToChars(float value)
{
    // c++17 has to_chars in <charconv> but GCC (stdlib) doesn't
    // yet support float conversion. gah.
    std::stringstream ss;
    std::string ret;
    ss.imbue(std::locale("C"));
    ss << std::fixed << std::setprecision(2) << value;
    ss >> ret;
    return ret;
}
} // namespace

#if defined(__MSVC__)
#  pragma warning(pop) // deprecated use of wstring_convert
#endif