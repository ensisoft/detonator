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
#  include <boost/algorithm/string.hpp>
#  if defined(BASE_FORMAT_SUPPORT_GLM)
#    include <glm/glm.hpp>
#  endif
#  if defined(BASE_FORMAT_SUPPORT_MAGIC_ENUM)
#    include <neargye/magic_enum.hpp>
#  endif
#include "warnpop.h"

#include <sstream>
#include <string>
#include <iomanip>
#include <tuple>
#include <type_traits>

#include "base/bitflag.h"
#include "base/types.h"
#include "base/color4f.h"
#include "base/rotator.h"

// minimalistic string formatting. doesn't support anything fancy such as escaping.
// uses a simple "foobar %1 %2" syntax where %-digit pairs are replaced by
// template arguments converted into strings.
// for example str("hello %1", "world") returns "hello world"

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

        std::string ToString(const base::FDegrees& angle);
        std::string ToString(const base::FRadians& angle);

#if defined(BASE_FORMAT_SUPPORT_GLM)
        std::string ToString(const glm::mat4& m);
        std::string ToString(const glm::mat3& m);
        std::string ToString(const glm::vec4& v);
        std::string ToString(const glm::vec3& v);
        std::string ToString(const glm::vec2& v);
        std::string ToString(const glm::quat& q);
        // Rotator is inside here because we can't have Rotator without glm
        std::string ToString(const Rotator& rotator);
#endif // BASE_FORMAT_SUPPORT_GLM
        std::string ToString(const FRect& rect);
        std::string ToString(const FSize& size);
        std::string ToString(const FPoint& point);
        std::string ToString(const Color4f& color);

#if defined(BASE_FORMAT_SUPPORT_MAGIC_ENUM)
        template<typename Enum> inline
        std::string EnumToString(const Enum value)
        { return std::string(magic_enum::enum_name(value)); }
#endif

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
#if defined(BASE_FORMAT_SUPPORT_MAGIC_ENUM)
            if constexpr (std::is_enum<T>::value)
                return EnumToString(value);
            else return ValueToString(value);
#else
            return ValueToString(value);
#endif
        }

        template<typename Enum> inline
        std::string ToString(const bitflag<Enum>& bits)
        {
#if defined(BASE_FORMAT_SUPPORT_MAGIC_ENUM)
            std::string ret;
            for (const auto& key : magic_enum::enum_values<Enum>())
            {
                const std::string name(magic_enum::enum_name(key));
                if (bits.test(magic_enum::enum_integer(key)))
                {
                    ret += name;
                    ret += "|";
                }
            }
            if (!ret.empty())
                ret.pop_back();
            return ret;
#else
            return ValueToString(bits.value());
#endif
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
    std::string ToChars(int value);

    bool FromChars(const std::string& str, float* value);
    bool FromChars(const std::string& str, int* value);
    bool FromChars(const std::string& str, unsigned* value);

} // base

