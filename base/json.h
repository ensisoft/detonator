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
#  include <nlohmann/json.hpp>
#  include <neargye/magic_enum.hpp>
#  include <glm/fwd.hpp>
#include "warnpop.h"

#include <tuple>
#include <sstream>
#include <string>

#include "base/bitflag.h"
#include "base/types.h"
#include "base/color4f.h"

// json utilities. the big problem with nlohmann::json is the huge size of the file
// at around 20kloc. Including json.hpp is a great way to make your build take a
// long time. So this header (and the source file) try to provide a little firewall
// around that.

// todo: refactor the templates so the json.hpp can be moved

namespace base
{

bool JsonReadSafe(const nlohmann::json& object, const char* name, float* out);
bool JsonReadSafe(const nlohmann::json& object, const char* name, int* out);
bool JsonReadSafe(const nlohmann::json& object, const char* name, unsigned* out);
bool JsonReadSafe(const nlohmann::json& object, const char* name, bool* out);
bool JsonReadSafe(const nlohmann::json& object, const char* name, std::string* out);
bool JsonReadSafe(const nlohmann::json& object, const char* name, glm::vec2* out);
bool JsonReadSafe(const nlohmann::json& object, const char* name, glm::vec3* out);
bool JsonReadSafe(const nlohmann::json& object, const char* name, glm::vec4* out);
bool JsonReadSafe(const nlohmann::json& value, float* out);
bool JsonReadSafe(const nlohmann::json& value, int* out);
bool JsonReadSafe(const nlohmann::json& value, unsigned* out);
bool JsonReadSafe(const nlohmann::json& value, bool *out);
bool JsonReadSafe(const nlohmann::json& value, std::string* out);
bool JsonReadSafe(const nlohmann::json& value, glm::vec2* out);
bool JsonReadSafe(const nlohmann::json& value, glm::vec3* out);
bool JsonReadSafe(const nlohmann::json& value, glm::vec4* out);
// todo: if needed provide wrappers for writing the the U and I types too.
bool JsonReadSafe(const nlohmann::json& json, const char* name, FRect* rect);
bool JsonReadSafe(const nlohmann::json& json, const char* name, FPoint* point);
bool JsonReadSafe(const nlohmann::json& json, const char* name, FSize* point);
bool JsonReadSafe(const nlohmann::json& json, const char* name, Color4f* color);

void JsonWrite(nlohmann::json& object, const char* name, int value);
void JsonWrite(nlohmann::json& object, const char* name, unsigned value);
void JsonWrite(nlohmann::json& object, const char* name, double value);
void JsonWrite(nlohmann::json& object, const char* name, float value);
void JsonWrite(nlohmann::json& object, const char* name, bool value);
void JsonWrite(nlohmann::json& object, const char* name, const char* str);
void JsonWrite(nlohmann::json& object, const char* name, const std::string& value);
void JsonWrite(nlohmann::json& object, const char* name, const glm::vec2& vec);
void JsonWrite(nlohmann::json& object, const char* name, const glm::vec3& vec);
void JsonWrite(nlohmann::json& object, const char* name, const glm::vec4& vec);
// todo: if needed provide wrappers for writing the the U and I types too.
void JsonWrite(nlohmann::json& json, const char* name, const FRect& rect);
void JsonWrite(nlohmann::json& json, const char* name, const FPoint& point);
void JsonWrite(nlohmann::json& json, const char* name, const FSize& point);
void JsonWrite(nlohmann::json& json, const char* name, const Color4f& color);

template<typename EnumT> inline
bool JsonReadSafeEnum(const nlohmann::json& object, const char* name, EnumT* out)
{
    std::string str;
    if (!object.contains(name) || !object[name].is_string())
        return false;

    str = object[name];
    const auto& enum_val = magic_enum::enum_cast<EnumT>(str);
    if (!enum_val.has_value())
        return false;

    *out = enum_val.value();
    return true;
}

template<typename EnumT> inline
bool JsonReadSafeEnum(const nlohmann::json& value, EnumT* out)
{
    if (!value.is_string())
        return false;

    const std::string& str = value;
    const auto& enum_val = magic_enum::enum_cast<EnumT>(str);
    if (!enum_val.has_value())
        return false;

    *out = enum_val.value();
    return true;
}

template<typename T> inline
bool JsonReadObject(const nlohmann::json& object, const char* name, T* out)
{
    if (!object.contains(name) || !object[name].is_object())
        return false;
    const std::optional<T>& ret = T::FromJson(object[name]);
    if (!ret.has_value())
        return false;
    *out = ret.value();
    return true;
}

template<typename T> inline
bool JsonReadObject(nlohmann::json& value, T* out)
{
    if (!value.is_object())
        return false;
    const std::optional<T>& ret = T::FromJson(value);
    if (!ret.has_value())
        return false;
    *out = ret.value();
    return true;
}

template<typename ValueT> inline
bool JsonReadSafe(const nlohmann::json& object, const char* name, ValueT* out)
{
    if constexpr (std::is_enum<ValueT>::value)
        return JsonReadSafeEnum(object, name, out);
    else return JsonReadObject(object, name, out);

    return false;
}
template<typename ValueT> inline
bool JsonReadSafe(const nlohmann::json&  value, ValueT* out)
{
    if constexpr (std::is_enum<ValueT>::value)
        return JsonReadSafeEnum(value,out);
    else return JsonReadObject(value, out);
    return false;
}

template<typename Enum, typename Bits>
bool JsonReadSafe(const nlohmann::json& object, const char* name, base::bitflag<Enum, Bits>* bitflag)
{
    if (!object.contains(name) || !object[name].is_object())
        return false;
    const auto& value = object[name];
    for (const auto& flag : magic_enum::enum_values<Enum>())
    {
        // for easy versioning of bits in the flag don't require
        // that all the flags exist in the object
        const std::string flag_name(magic_enum::enum_name(flag));
        if (!value.contains(flag_name))
            continue;
        bool on_off = false;
        if (!JsonReadSafe(value, flag_name.c_str(), &on_off))
            return false;
        bitflag->set(flag, on_off);
    }
    return true;
}

template<typename Enum, typename Bits>
bool JsonReadSafe(const nlohmann::json& value, base::bitflag<Enum, Bits>* bitflag)
{
    if (!value.is_object())
        return false;
    for (const auto& flag : magic_enum::enum_values<Enum>())
    {
        // for easy versioning of bits in the flag don't require
        // that all the flags exist in the object
        const std::string flag_name(magic_enum::enum_name(flag));
        if (!value.contains(flag_name))
            continue;
        bool on_off = false;
        if (!JsonReadSafe(value, flag_name.c_str(), &on_off))
            return false;
        bitflag->set(flag, on_off);
    }
    return true;
}

template<typename EnumT> inline
void JsonWriteEnum(nlohmann::json& object, const char* name, EnumT value)
{
    const std::string str(magic_enum::enum_name(value));
    object[name] = str;
}
template<typename T> inline
void JsonWriteObject(nlohmann::json& object, const char* name, const T& value)
{
    object[name] = value.ToJson();
}

template<typename JsonObject, typename ValueT> inline
void JsonWrite(JsonObject& object, const char* name, const ValueT& value)
{
    if constexpr (std::is_enum<ValueT>::value)
        JsonWriteEnum(object, name, value);
    else JsonWriteObject(object, name, value);
}


template<typename JsonObject, typename Enum, typename Bits>
void JsonWrite(JsonObject& object, const char* name, base::bitflag<Enum, Bits> bitflag)
{
    JsonObject json;
    for (const auto& flag : magic_enum::enum_values<Enum>())
    {
        const std::string flag_name(magic_enum::enum_name(flag));
        JsonWrite(json, flag_name.c_str(), bitflag.test(flag));
    }
    object[name] = std::move(json);
}

template<typename It> inline
std::tuple<bool, nlohmann::json, std::string> JsonParse(It beg, It end)
{
    // if exceptions are suppressed there are no diagnostics
    // so use this wrapper.
    std::string message;
    nlohmann::json json;
    bool ok = true;
    try
    {
        json = nlohmann::json::parse(beg, end);
    }
    catch (const nlohmann::detail::parse_error& err)
    {
        std::stringstream ss;
        ss << "JSON parse error '" << err.what() << "'";
        ss >> message;
        ok = false;
    }
    return std::make_tuple(ok, std::move(json), std::move(message));
}
std::tuple<bool, nlohmann::json, std::string> JsonParseFile(const std::string& filename);

} // base

