// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.


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

