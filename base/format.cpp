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
#include <cctype>
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

std::string ToString(const glm::quat& q)
{
    char buff[1024];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),
        "[%.2f %.2f %.2f %.2f]", q.x, q.y, q.z, q.w);
    return buff;
}

std::string ToString(const Rotator& rotator)
{
    return ToString(rotator.GetAsQuaternion());
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

std::string ToString(const base::FRadians& angle)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "rad:%.2f | deg:%.2f", angle.ToRadians(), angle.ToDegrees());
    return buff;
}

std::string ToString(const base::FDegrees& angle)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "rad:%.2f | deg:%.2f", angle.ToRadians(), angle.ToDegrees());
    return buff;
}

} // detail
} // namespace


namespace base
{

std::string TrimString(const std::string& str)
{
    return boost::trim_copy(str);
}

std::string ToUtf8(const std::wstring& str)
{
    // this way of converting is deprecated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::string converted_str = converter.to_bytes(str);
    return converted_str;
}

std::wstring FromUtf8(const std::string& str)
{
    // this way of converting is deprecated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::wstring converted_str = converter.from_bytes(str);
    return converted_str;
}

std::wstring ToUpper(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(towupper(c));
    return ret;
}

std::wstring ToLower(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(towlower(c));
    return ret;
}

std::string ToUpperUtf8(const std::string& str)
{ return ToUtf8(ToUpper(FromUtf8(str))); }

std::string ToLowerUtf8(const std::string& str)
{ return ToUtf8(ToLower(FromUtf8(str))); }

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

std::string ToChars(int value)
{
    return std::to_string(value);
}

std::string ToChars(unsigned value)
{
    return std::to_string(value);
}

std::string ToHex(const Color4f& color)
{
    const uint8_t R = (uint8_t)math::clamp(0.0f, 255.0f, 255.0f * color.Red());
    const uint8_t G = (uint8_t)math::clamp(0.0f, 255.0f, 255.0f * color.Green());
    const uint8_t B = (uint8_t)math::clamp(0.0f, 255.0f, 255.0f * color.Blue());
    const uint8_t A = (uint8_t)math::clamp(0.0f, 255.0f, 255.0f * color.Alpha());

    char hex_string[10];

    // Format the hex string
    std::sprintf(hex_string, "#%02X%02X%02X%02X", R, G, B, A);

    return hex_string;

}

bool FromChars(const std::string& str, float* value)
{
    std::stringstream ss(str);
    ss.imbue(std::locale("C"));
    ss >> *value;
    return !ss.fail();
}

bool FromChars(const std::string& str, int* value)
{
    std::stringstream ss(str);
    ss.imbue(std::locale("C"));
    ss >> *value;
    return !ss.fail();
}

bool FromChars(const std::string& str, unsigned* value)
{
    std::stringstream ss(str);
    ss.imbue(std::locale("C"));
    ss >> *value;
    return !ss.fail();
}

bool FromHex(const std::string& str, Color4f* value)
{
    // #aabbccff or #aabbcc
    const bool length_ok = str.size() == 9 || str.size() == 7;
    if (!length_ok)
        return false;

    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;

    // Parse the hex value into the rgba components
    if (std::sscanf(str.c_str(), "#%2x%2x%2x%2x", &r, &g, &b, &a) == 4)
    {
        *value = Color4f{r, g, b, a};
        return true;
    }

    // Parse the hex value into the rgba components
    if (std::sscanf(str.c_str(), "#%2x%2x%2x", &r, &g, &b) == 3)
    {
        *value = Color4f{r, g, b, 0xff};
        return true;
    }

    return false;
}

Color4f ColorFromHex(const std::string& str, const Color4f& backup)
{
    Color4f value;
    if (!FromHex(str, &value))
        return backup;
    return value;
}

} // namespace

#if defined(__MSVC__)
#  pragma warning(pop) // deprecated use of wstring_convert
#endif
