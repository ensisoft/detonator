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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <boost/algorithm/string.hpp>
#  if defined(BASE_FORMAT_SUPPORT_GLM)
#    include <glm/glm.hpp>
#  endif
#include "warnpop.h"

#include <sstream>
#include <iosfwd>

// minimalistic string formatting. doesn't support anything fancy such as escaping.
// uses a simple "foobar %1 %2" syntax where %-digit pairs are replaced by
// template arguments converted into strings.
// for example str("hello %1", "world") returns "hello world"

#if defined(BASE_FORMAT_SUPPORT_GLM)
namespace glm {
    namespace detail {

    std::ostream& operator << (std::ostream& out, const glm::mat4& m);
    std::ostream& operator << (std::ostream& out, const glm::mat3& m);
    std::ostream& operator << (std::ostream& out, const glm::vec4& v);
    std::ostream& operator << (std::ostream& out, const glm::vec3& v);
    std::ostream& operator << (std::ostream& out, const glm::vec2& v);

    } // detail
} // glm
#endif // BASE_FORMAT_SUPPORT_GLM

#if defined(BASE_FORMAT_SUPPORT_QT)
   class QString;
   std::ostream& operator << (std::ostream& out, const QString& str);
#endif // BASE_FORMAT_SUPPORT_QT

namespace base
{
    namespace detail {

        template<typename T>
        std::string replace(std::size_t index, const std::string& fmt, const T& value)
        {
            std::string key;
            std::stringstream ss;
            ss << "%" << index;
            ss >> key;

            ss.str("");
            ss.clear();
            ss << value;
            return boost::replace_all_copy(fmt, key, ss.str());
        }

        inline
        std::string replace(std::size_t index, const std::string& fmt, const std::string& val)
        {
            std::string key;
            std::stringstream ss;
            ss << "%" << index;
            ss >> key;
            return boost::replace_all_copy(fmt, key, val);
        }

        template<typename T>
        std::string FormatString(std::size_t index, const std::string& fmt, const T& value)
        {
            return replace(index, fmt, value);
        }

        template<typename T, typename... Args>
        std::string FormatString(std::size_t index, const std::string& fmt, const T& value, const Args&... args)
        {
            return FormatString(index + 1, replace(index, fmt, value), args...);
        }

    } // detail

    template<typename T>
    std::string FormatString(const std::string& fmt, const T& value)
    {
        return detail::FormatString(1, fmt, value);
    }

    template<typename T, typename... Args>
    std::string FormatString(const std::string& fmt, const T& value, const Args&... args)
    {
        return detail::FormatString(1, fmt, value, args...);
    }

    inline
    std::string FormatString(const std::string& fmt)
    {
        return fmt;
    }

} // base