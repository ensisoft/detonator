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
#include <locale>
#include <codecvt>
#include <iomanip>
#include <type_traits>

#if defined(__MSVC__)
#  pragma warning(push)
#  pragma warning(disable: 4996) // deprecated use of wstring_convert
#endif

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
        // generic version trying to use stringstream based conversion.
        template<typename T> inline
        std::string ToString(const T& value)
        {
            if constexpr (std::is_enum<T>::value)
            {
                return std::string(magic_enum::enum_name(value));
            }
            else
            {
                std::stringstream ss;
                ss << value;
                return ss.str();
            }
        }
        inline std::string ToString(const std::wstring& s)
        {
            //setup converter
            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;
            std::string converted_str = converter.to_bytes(s);
            return converted_str;
        }

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

        template<typename T>
        std::string FormatString(std::size_t index, const std::string& fmt, const T& value)
        {
            return ReplaceIndex(index, fmt, value);
        }

        template<typename T, typename... Args>
        std::string FormatString(std::size_t index, const std::string& fmt, const T& value, const Args&... args)
        {
            auto ret = ReplaceIndex(index, fmt, value);

            return FormatString(index + 1, std::move(ret), args...);
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

    using detail::ToString;

     // format float value to a string ignoring the user's locale.
     // I.e. the format always uses . as the decimal point.
    inline std::string ToChars(float value)
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
} // base

#if defined(__MSVC__)
#  pragma warning(pop) // deprecated use of wstring_convert
#endif