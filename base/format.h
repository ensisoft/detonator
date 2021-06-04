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
#  include <neargye/magic_enum.hpp> // todo: put ifndef ?
#include "warnpop.h"

#include <sstream>
#include <string>
#include <iomanip>
#include <type_traits>

#include "base/types.h"

// minimalistic string formatting. doesn't support anything fancy such as escaping.
// uses a simple "foobar %1 %2" syntax where %-digit pairs are replaced by
// template arguments converted into strings.
// for example str("hello %1", "world") returns "hello world"
// todo: move the JSON functionality here for the simple types such as Rect etc.

namespace base
{
    namespace detail {
        std::string ToString(const std::wstring& s);
        inline std::string ToString(const std::string& s)
        { return s; }
        inline std::string ToString(int value)
        { return std::to_string(value); }
        inline std::string ToString(long value)
        { return std::to_string(value); }
        inline std::string ToString(long long value)
        { return std::to_string(value); }
        inline std::string ToString(unsigned value)
        { return std::to_string(value); }
        inline std::string ToString(unsigned long value)
        { return std::to_string(value); }
        inline std::string ToString(unsigned long long value)
        { return std::to_string(value); }
        inline std::string ToString(float value)
        { return std::to_string(value); }
        inline std::string ToString(double value)
        { return std::to_string(value); }
        inline std::string ToString(long double value)
        { return std::to_string(value); }
#if defined(BASE_FORMAT_SUPPORT_GLM)
        std::string ToString(const glm::mat4& m);
        std::string ToString(const glm::mat3& m);
        std::string ToString(const glm::vec4& v);
        std::string ToString(const glm::vec3& v);
        std::string ToString(const glm::vec2& v);
#endif // BASE_FORMAT_SUPPORT_GLM
        std::string ToString(const FRect& rect);
        std::string ToString(const FSize& size);
        std::string ToString(const FPoint& point);

        template<typename Enum> inline
        std::string EnumToString(const Enum value)
        { return std::string(magic_enum::enum_name(value)); }

        template<typename T> inline
        std::string ValueToString(const T& value)
        {
            // generic version trying to use stringstream based conversion.
            std::stringstream ss;
            ss << value;
            return ss.str();
        }

        template<typename T> inline
        std::string ToString(const T& value)
        {
            if constexpr (std::is_enum<T>::value)
                return EnumToString(value);
            else return ValueToString(value);
        }

        template<typename T>
        std::string ReplaceIndex(std::size_t index, const std::string& fmt, const T& value)
        {
            // generate the key to be replaced in the string
            std::string key;
            std::stringstream ss;
            ss << "%" << index;
            ss >> key;
            // ToString can be looked up through ADL
            // so any user defined type can define a ToString in the namespace of T
            // and this implementation can use that method for the T->string conversion
            return boost::replace_all_copy(fmt, key, ToString(value));
        }

        template<typename T> inline
        std::string FormatString(std::size_t index, const std::string& fmt, const T& value)
        { return ReplaceIndex(index, fmt, value); }

        template<typename T, typename... Args> inline
        std::string FormatString(std::size_t index, const std::string& fmt, const T& value, const Args&... args)
        {  return FormatString(index + 1, ReplaceIndex(index, fmt, value), args...); }
    } // detail

    template<typename T> inline
    std::string FormatString(const std::string& fmt, const T& value)
    { return detail::FormatString(1, fmt, value); }

    template<typename T, typename... Args> inline
    std::string FormatString(const std::string& fmt, const T& value, const Args&... args)
    { return detail::FormatString(1, fmt, value, args...); }

    inline std::string FormatString(const std::string& fmt)
    { return fmt; }

    // bring ToString also into base namespace scope
    using detail::ToString;

     // format float value to a string ignoring the user's locale.
     // I.e. the format always uses . as the decimal point.
    std::string ToChars(float value);
} // base

