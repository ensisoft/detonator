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
#  include <glm/glm.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "base/utility.h"
#include "base/json.h"
#include "base/hash.h"
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

void ScriptVar::IntoJson(nlohmann::json& json) const
{
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
