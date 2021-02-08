// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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

#include "warnpush.h"
#  include <glm/glm.hpp>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "base/utility.h"
#include "engine/types.h"

namespace game
{
ScriptVar::Type ScriptVar::GetType() const
{
    if (std::holds_alternative<int>(mData))
        return Type::Integer;
    else if (std::holds_alternative<float>(mData))
        return Type::Float;
    else if (std::holds_alternative<bool>(mData))
        return Type::Boolean;
    else if (std::holds_alternative<std::string>(mData))
        return Type::String;
    else if (std::holds_alternative<glm::vec2>(mData))
        return Type::Vec2;
    BUG("Unknown ScriptVar type!");
}

size_t ScriptVar::GetHash() const
{
    size_t hash = 0;
    switch (GetType())
    {
        case Type::Integer:
            hash = base::hash_combine(hash, GetValue<int>());
            break;
        case Type::Vec2:
            hash = base::hash_combine(hash, GetValue<glm::vec2>());
            break;
        case Type::Boolean:
            hash = base::hash_combine(hash, GetValue<bool>());
            break;
        case Type::String:
            hash = base::hash_combine(hash, GetValue<std::string>());
            break;
        case Type::Float:
            hash = base::hash_combine(hash, GetValue<float>());
            break;
    }
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mReadOnly);
    return hash;
}

nlohmann::json ScriptVar::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "name", mName);
    base::JsonWrite(json, "readonly", mReadOnly);
    base::JsonWrite(json, "type", GetType());
    switch (GetType())
    {
        case Type::Integer:
            base::JsonWrite(json, "value",GetValue<int>());
            break;
        case Type::Vec2:
            base::JsonWrite(json, "value", GetValue<glm::vec2>());
            break;
        case Type::Boolean:
            base::JsonWrite(json, "value",GetValue<bool>());
            break;
        case Type::String:
            base::JsonWrite(json, "value", GetValue<std::string>());
            break;
        case Type::Float:
            base::JsonWrite(json, "value", GetValue<float>());
            break;
    }
    return json;
}

template<typename T, typename Variant>
bool JsonReadValue(const nlohmann::json& json, Variant* out)
{
    T value;
    if (!base::JsonReadSafe(json, "value", &value))
        return false;
    *out = value;
    return true;
}

// static
std::optional<ScriptVar> ScriptVar::FromJson(const nlohmann::json& json)
{
    ScriptVar ret;
    ScriptVar::Type type = ScriptVar::Type::Integer;
    if (!base::JsonReadSafe(json, "name", &ret.mName) ||
        !base::JsonReadSafe(json, "type", &type) ||
        !base::JsonReadSafe(json, "readonly", &ret.mReadOnly))
        return std::nullopt;

    switch (type) {
        case Type::Integer:
            if (!JsonReadValue<int>(json, &ret.mData))
                return std::nullopt;
            break;

        case Type::Vec2:
            if (!JsonReadValue<glm::vec2>(json, &ret.mData))
                return std::nullopt;
            break;

        case Type::Float:
            if (!JsonReadValue<float>(json, &ret.mData))
                return std::nullopt;
            break;

        case Type::String:
            if (!JsonReadValue<std::string>(json, &ret.mData))
                return std::nullopt;
            break;

        case Type::Boolean:
            if (!JsonReadValue<bool>(json, &ret.mData))
                return std::nullopt;
            break;
    }
    return ret;
}


} // namespace
