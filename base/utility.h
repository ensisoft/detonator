// Copyright (c) 2016 Sami Väisänen, Ensisoft
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
#  include <neargye/magic_enum.hpp>
#  include <nlohmann/json.hpp>
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <functional> // for hash
#include <chrono>
#include <memory> // for unique_ptr
#include <string>
#include <locale>
#include <codecvt>
#include <cwctype>
#include <cstring>
#include <random>
#include <filesystem>
#include <fstream>
#include <tuple>

#include "base/math.h"
#include "base/assert.h"
#include "base/platform.h"
#include "base/bitflag.h"

#if defined(__MSVC__)
#  pragma warning(push)
#  pragma warning(disable: 4996) // deprecated use of wstring_convert
#endif

namespace base
{

inline bool Contains(const std::string& str, const std::string& what)
{
    return str.find(what) != std::string::npos;
}

inline bool StartsWith(const std::string& str, const std::string& what)
{
    return str.find(what) == 0;
}

inline std::string RandomString(size_t len)
{
    static const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUWXYZ"
        "abdefghijlkmnopqrstuvwxyz"
        "1234567890";
    static unsigned max_len = std::strlen(alphabet) - 1;
    static std::random_device rd;
    std::uniform_int_distribution<unsigned> dist(0, max_len);

    std::string ret;
    for (size_t i=0; i<len; ++i)
    {
        const auto letter = dist(rd);
        ret.push_back(alphabet[letter]);
    }
    return ret;
}

template<typename S, typename T> inline
S hash_combine(S seed, T value)
{
    const auto hash = std::hash<T>()(value);
    seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

template<typename S> inline
S hash_combine(S seed, const glm::vec2& value)
{
    seed = hash_combine(seed, value.x);
    seed = hash_combine(seed, value.y);
    return seed;
}

template<typename S> inline
S hash_combine(S seed, const glm::vec3& value)
{
    seed = hash_combine(seed, value.x);
    seed = hash_combine(seed, value.y);
    seed = hash_combine(seed, value.z);
    return seed;
}

template<typename S> inline
S hash_combine(S seed, const glm::vec4& value)
{
    seed = hash_combine(seed, value.x);
    seed = hash_combine(seed, value.y);
    seed = hash_combine(seed, value.z);
    seed = hash_combine(seed, value.w);
    return seed;
}

inline double GetRuntimeSec()
{
    using steady_clock = std::chrono::steady_clock;
    static const auto start = steady_clock::now();
    const auto now = steady_clock::now();
    const auto gone = now - start;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(gone);
    return ms.count() / 1000.0;
}

template<typename T, typename Deleter>
std::unique_ptr<T, Deleter> MakeUniqueHandle(T* ptr, Deleter del)
{
    return std::unique_ptr<T, Deleter>(ptr, del);
}

inline
std::string ToUtf8(const std::wstring& str)
{
    // this way of converting is depcreated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::string converted_str = converter.to_bytes(str);
    return converted_str;
}

inline
std::wstring FromUtf8(const std::string& str)
{
    // this way of converting is deprecated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::wstring converted_str = converter.from_bytes(str);
    return converted_str;
}

inline
std::wstring ToUpper(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(std::towupper(c));
    return ret;
}

inline
std::wstring ToLower(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(std::towlower(c));
    return ret;
}

// convert std::string to wide string representation
// without any regard to encoding.
inline
std::wstring Widen(const std::string& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(c);
    return ret;
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

inline
std::tuple<bool, nlohmann::json, std::string> JsonParseFile(const std::string& filename)
{
#if defined(WINDOWS_OS)
    std::ifstream stream(base::FromUtf8(filename), std::ios::in);
#elif defined(POSIX_OS)
    std::ifstream stream(filename, std::ios::in);
#else
#  error unimplemented function
#endif
    const std::string contents(std::istreambuf_iterator<char>(stream), {});
    return JsonParse(contents.begin(), contents.end());
}

// works with nlohmann::json
// using a template here simply to avoid having to specify a type
// that would require dependency to a library in this header.
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, float* out)
{
    if (!object.contains(name) || !object[name].is_number_float())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonValue> inline
bool JsonReadSafe(const JsonValue& value, float*out)
{
    if (!value.is_number_float())
        return false;
    *out = value;
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, int* out)
{
    if (!object.contains(name) || !object[name].is_number_integer())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonValue> inline
bool JsonReadSafe(const JsonValue& value, int* out)
{
    if (!value.is_number_integer())
        return false;
    *out = value;
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, unsigned* out)
{
    if (!object.contains(name) || !object[name].is_number_unsigned())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonValue> inline
bool JsonReadSafe(const JsonValue& value, unsigned* out)
{
    if (!value.is_number_unsigned())
        return false;
    *out = value;
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, std::string* out)
{
    if  (!object.contains(name) || !object[name].is_string())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, std::wstring* out)
{
    if (!object.contains(name) || !object[name].is_string())
        return false;
    const std::string& str = object[name];
    *out = base::FromUtf8(str);
    return true;
}
template<typename JsonValue> inline
bool JsonReadSafe(const JsonValue& value, std::string* out)
{
    if (!value.is_string())
        return false;
    *out = value;
    return true;
}
template<typename JsonValue> inline
bool JsonReadSafe(const JsonValue& value, std::wstring* out)
{
    if (!value.is_string())
        return false;
    const std::string& str = value;
    *out = base::FromUtf8(str);
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, bool* out)
{
    if (!object.contains(name) || !object[name].is_boolean())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonValue> inline
bool JsonReadSafe(const JsonValue& value, bool *out)
{
    if (!value.is_boolean())
        return false;
    *out = value;
    return true;
}
template<typename JsonObject, typename EnumT> inline
bool JsonReadSafeEnum(const JsonObject& object, const char* name, EnumT* out)
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

template<typename JsonValue, typename EnumT> inline
bool JsonReadSafeEnum(const JsonValue& value, EnumT* out)
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

template<typename JsonObject, typename T> inline
bool JsonReadObject(const JsonObject& object, const char* name, T* out)
{
    if (!object.contains(name) || !object[name].is_object())
        return false;
    const std::optional<T>& ret = T::FromJson(object[name]);
    if (!ret.has_value())
        return false;
    *out = ret.value();
    return true;
}
template<typename JsonValue, typename T> inline
bool JsonReadObject(const JsonValue& value, T* out)
{
    if (!value.is_object())
        return false;
    const std::optional<T>& ret = T::FromJson(value);
    if (!ret.has_value())
        return false;
    *out = ret.value();
    return true;
}

template<typename JsonObject, typename ValueT> inline
bool JsonReadSafe(const JsonObject& object, const char* name, ValueT* out)
{
    if constexpr (std::is_enum<ValueT>::value)
        return JsonReadSafeEnum(object, name, out);
    else return JsonReadObject(object, name, out);

    return false;
}
template<typename JsonValue, typename ValueT> inline
bool JsonReadSafe(const JsonValue&  value, ValueT* out)
{
     if constexpr (std::is_enum<ValueT>::value)
        return JsonReadSafeEnum(value,out);
    else return JsonReadObject(value, out);
    return false;
}

template<typename JsonObject>
bool JsonReadSafe(const JsonObject& object, const char* name, glm::vec2* out)
{
    if (!object.contains(name) || !object[name].is_object())
        return false;
    const auto& vector = object[name];
    if (!vector.contains("x") || !vector["x"].is_number_float())
        return false;
    if (!vector.contains("y") || !vector["y"].is_number_float())
        return false;
    out->x = vector["x"];
    out->y = vector["y"];
    return true;
}

template<typename JsonObject>
bool JsonReadSafe(const JsonObject& object, const char* name, glm::vec3* out)
{
    if (!object.contains(name) || !object[name].is_object())
        return false;
    const auto& vector = object[name];
    if (!vector.contains("x") || !vector["x"].is_number_float())
        return false;
    if (!vector.contains("y") || !vector["y"].is_number_float())
        return false;
    if (!vector.contains("z") || !vector["z"].is_number_float())
        return false;
    out->x = vector["x"];
    out->y = vector["y"];
    out->z = vector["z"];
    return true;
}
template<typename JsonObject>
bool JsonReadSafe(const JsonObject& object, const char* name, glm::vec4* out)
{
    if (!object.contains(name) || !object[name].is_object())
        return false;
    const auto& vector = object[name];
    if (!vector.contains("x") || !vector["x"].is_number_float())
        return false;
    if (!vector.contains("y") || !vector["y"].is_number_float())
        return false;
    if (!vector.contains("z") || !vector["z"].is_number_float())
        return false;
    if (!vector.contains("w") || !vector["w"].is_number_float())
        return false;
    out->x = vector["x"];
    out->y = vector["y"];
    out->z = vector["z"];
    out->w = vector["w"];
    return true;
}

template<typename JsonValue>
bool JsonReadSafe(const JsonValue& value, glm::vec2* out)
{
    if (!value.is_object())
        return false;
    if (!value.contains("x") || !value["x"].is_number_float())
        return false;
    if (!value.contains("y") || !value["y"].is_number_float())
        return false;
    out->x = value["x"];
    out->y = value["y"];
    return true;
}
template<typename JsonValue>
bool JsonReadSafe(const JsonValue& value, glm::vec3* out)
{
    if (!value.is_object())
        return false;
    if (!value.contains("x") || !value["x"].is_number_float())
        return false;
    if (!value.contains("y") || !value["y"].is_number_float())
        return false;
    if (!value.contains("z") || !value["z"].is_number_float())
        return false;
    out->x = value["x"];
    out->y = value["y"];
    out->z = value["z"];
    return true;
}
template<typename JsonValue>
bool JsonReadSafe(const JsonValue& value, glm::vec4* out)
{
    if (!value.is_object())
        return false;
    if (!value.contains("x") || !value["x"].is_number_float())
        return false;
    if (!value.contains("y") || !value["y"].is_number_float())
        return false;
    if (!value.contains("z") || !value["z"].is_number_float())
        return false;
    if (!value.contains("w") || !value["w"].is_number_float())
        return false;
    out->x = value["x"];
    out->y = value["y"];
    out->z = value["z"];
    out->w = value["w"];
    return true;
}

template<typename JsonObject, typename Enum, typename Bits>
bool JsonReadSafe(const JsonObject& object, const char* name, base::bitflag<Enum, Bits>* bitflag)
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

template<typename JsonValue, typename Enum, typename Bits>
bool JsonReadSafe(const JsonValue& value, base::bitflag<Enum, Bits>* bitflag)
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

template<typename JsonValue, typename T> inline
void JsonAppendObject(JsonValue& json, const T& value)
{
    json.push_back(value.ToJson());
}
template<typename JsonValue, typename EnumT> inline
void JsonAppendEnum(JsonValue& json, EnumT value)
{
    const std::string str(magic_enum::enum_name(value));
    json.push_back(str);
}

template<typename JsonValue> inline
void JsonAppend(JsonValue& json, int value)
{ json.push_back(value); }

template<typename JsonValue> inline
void JsonAppend(JsonValue& json, unsigned value)
{ json.push_back(value); }

template<typename JsonValue> inline
void JsonAppend(JsonValue& json, double value)
{ json.push_back(value); }

template<typename JsonValue> inline
void JsonAppend(JsonValue& json, float value)
{ json.push_back(value); }

template<typename JsonValue> inline
void JsonAppend(JsonValue& json, const std::string& value)
{ json.push_back(value); }

template<typename JsonValue> inline
void JsonAppend(JsonValue& json, const std::wstring& value)
{ json.push_back(base::ToUtf8(value)); }

template<typename JsonValue> inline
void JsonAppend(JsonValue& json, bool value)
{ json.push_back(value); }

template<typename JsonValue> inline
void JsonAppend(JsonValue& json, const char* str)
{ json.push_back(str); }

template<typename JsonValue, typename ValueT> inline
void JsonAppend(JsonValue& json, const ValueT& value)
{
    if constexpr (std::is_enum<ValueT>::value)
        JsonAppendEnum(json, value);
    else JsonAppendObject(json, value);
}

template<typename JsonValue>
void JsonAppend(JsonValue& json, const glm::vec2& vec)
{
    nlohmann::json obj;
    obj["x"] = vec.x;
    obj["y"] = vec.y;
    json.push_back(std::move(obj));
}
template<typename JsonValue>
void JsonAppend(JsonValue& json, const glm::vec3& vec)
{
    nlohmann::json obj;
    obj["x"] = vec.x;
    obj["y"] = vec.y;
    obj["z"] = vec.z;
    json.push_back(std::move(obj));
}
template<typename JsonValue>
void JsonAppend(JsonValue& json, const glm::vec4& vec)
{
    nlohmann::json obj;
    obj["x"] = vec.x;
    obj["y"] = vec.y;
    obj["z"] = vec.z;
    obj["w"] = vec.w;
    json.push_back(std::move(obj));
}

template<typename JsonObject, typename EnumT> inline
void JsonWriteEnum(JsonObject& object, const char* name, EnumT value)
{
    const std::string str(magic_enum::enum_name(value));
    object[name] = str;
}
template<typename JsonObject, typename T> inline
void JsonWriteObject(JsonObject& object, const char* name, const T& value)
{
    object[name] = value.ToJson();
}
template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, int value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, unsigned value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, double value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, float value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, const std::string& value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, const std::wstring& value)
{ object[name] = base::ToUtf8(value); }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, bool value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, const char* str)
{ object[name] = str; }

template<typename JsonObject, typename ValueT> inline
void JsonWrite(JsonObject& object, const char* name, const ValueT& value)
{
    if constexpr (std::is_enum<ValueT>::value)
        JsonWriteEnum(object, name, value);
    else JsonWriteObject(object, name, value);
}

template<typename JsonObject>
void JsonWrite(JsonObject& object, const char* name, const glm::vec2& vec)
{
    JsonObject json;
    JsonWrite(json, "x", vec.x);
    JsonWrite(json, "y", vec.y);
    object[name] = std::move(json);
}
template<typename JsonObject>
void JsonWrite(JsonObject& object, const char* name, const glm::vec3& vec)
{
    JsonObject json;
    JsonWrite(json, "x", vec.x);
    JsonWrite(json, "y", vec.y);
    JsonWrite(json, "z", vec.z);
    object[name] = std::move(json);
}
template<typename JsonObject>
void JsonWrite(JsonObject& object, const char* name, const glm::vec4& vec)
{
    JsonObject json;
    JsonWrite(json, "x", vec.x);
    JsonWrite(json, "y", vec.y);
    JsonWrite(json, "z", vec.z);
    JsonWrite(json, "w", vec.w);
    object[name] = std::move(json);
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

inline bool FileExists(const std::string& filename)
{
#if defined(WINDOWS_OS)
    const std::filesystem::path p(base::FromUtf8(filename));
    return std::filesystem::exists(p);
#elif defined(POSIX_OS)
    const std::filesystem::path p(filename);
    return std::filesystem::exists(p);
#endif
}

inline std::ifstream OpenBinaryInputStream(const std::string& filename)
{
#if defined(WINDOWS_OS)
    std::ifstream in(base::FromUtf8(filename), std::ios::in | std::ios::binary);
#elif defined(POSIX_OS)
    std::ifstream in(filename, std::ios::in | std::ios::binary);
#else
#  error unimplemented function
#endif
    return in;
}

inline bool OverwriteTextFile(const std::string& file, const std::string& text)
{
#if defined(WINDOWS_OS)
    std::ofstream out(base::FromUtf8(file), std::ios::out);
#elif defined(POSIX_OS)
    std::ofstream out(file, std::ios::out | std::ios::trunc);
#else
#  error unimplemented function.
#endif
    if (!out.is_open())
        return false;
    out << text;
    if (out.bad() || out.fail())
        return false;
    return true;
}

} // base

#if defined(__MSVC__)
#  pragma warning(pop) // deprecated use of wstring_convert
#endif
