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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <fstream>

#include "base/json.h"

namespace base
{
bool JsonReadSafe(const nlohmann::json& object, const char* name, float* out)
{
    if (!object.contains(name) || !object[name].is_number_float())
        return false;
    *out = object[name];
    return true;
}
bool JsonReadSafe(const nlohmann::json& object, const char* name, int* out)
{
    if (!object.contains(name) || !object[name].is_number_integer())
        return false;
    *out = object[name];
    return true;
}
bool JsonReadSafe(const nlohmann::json& object, const char* name, unsigned* out)
{
    if (!object.contains(name) || !object[name].is_number_unsigned())
        return false;
    *out = object[name];
    return true;
}
bool JsonReadSafe(const nlohmann::json& object, const char* name, bool* out)
{
    if (!object.contains(name) || !object[name].is_boolean())
        return false;
    *out = object[name];
    return true;
}
bool JsonReadSafe(const nlohmann::json& object, const char* name, std::string* out)
{
    if  (!object.contains(name) || !object[name].is_string())
        return false;
    *out = object[name];
    return true;
}

bool JsonReadSafe(const nlohmann::json& object, const char* name, glm::vec2* out)
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
bool JsonReadSafe(const nlohmann::json& object, const char* name, glm::vec3* out)
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
bool JsonReadSafe(const nlohmann::json& object, const char* name, glm::vec4* out)
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

bool JsonReadSafe(const nlohmann::json& value, float*out)
{
    if (!value.is_number_float())
        return false;
    *out = value;
    return true;
}
bool JsonReadSafe(const nlohmann::json& value, int* out)
{
    if (!value.is_number_integer())
        return false;
    *out = value;
    return true;
}
bool JsonReadSafe(const nlohmann::json& value, unsigned* out)
{
    if (!value.is_number_unsigned())
        return false;
    *out = value;
    return true;
}
bool JsonReadSafe(const nlohmann::json& value, std::string* out)
{
    if (!value.is_string())
        return false;
    *out = value;
    return true;
}
bool JsonReadSafe(const nlohmann::json& value, bool *out)
{
    if (!value.is_boolean())
        return false;
    *out = value;
    return true;
}
bool JsonReadSafe(const nlohmann::json& value, glm::vec2* out)
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
bool JsonReadSafe(const nlohmann::json& value, glm::vec3* out)
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
bool JsonReadSafe(const nlohmann::json& value, glm::vec4* out)
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

void JsonWrite(nlohmann::json& object, const char* name, int value)
{ object[name] = value; }

void JsonWrite(nlohmann::json& object, const char* name, unsigned value)
{ object[name] = value; }

void JsonWrite(nlohmann::json& object, const char* name, double value)
{ object[name] = value; }

void JsonWrite(nlohmann::json& object, const char* name, float value)
{ object[name] = value; }

void JsonWrite(nlohmann::json& object, const char* name, const std::string& value)
{ object[name] = value; }

void JsonWrite(nlohmann::json& object, const char* name, bool value)
{ object[name] = value; }

void JsonWrite(nlohmann::json& object, const char* name, const char* str)
{ object[name] = str; }

void JsonWrite(nlohmann::json& object, const char* name, const glm::vec2& vec)
{
    nlohmann::json json;
    JsonWrite(json, "x", vec.x);
    JsonWrite(json, "y", vec.y);
    object[name] = std::move(json);
}

void JsonWrite(nlohmann::json& object, const char* name, const glm::vec3& vec)
{
    nlohmann::json json;
    JsonWrite(json, "x", vec.x);
    JsonWrite(json, "y", vec.y);
    JsonWrite(json, "z", vec.z);
    object[name] = std::move(json);
}

void JsonWrite(nlohmann::json& object, const char* name, const glm::vec4& vec)
{
    nlohmann::json json;
    JsonWrite(json, "x", vec.x);
    JsonWrite(json, "y", vec.y);
    JsonWrite(json, "z", vec.z);
    JsonWrite(json, "w", vec.w);
    object[name] = std::move(json);
}

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

} // namespace