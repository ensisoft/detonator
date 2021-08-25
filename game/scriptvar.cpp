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
#include "data/writer.h"
#include "data/reader.h"
#include "base/hash.h"
#include "game/scriptvar.h"

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
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mData);
    hash = base::hash_combine(hash, mReadOnly);
    return hash;
}

void ScriptVar::IntoJson(data::Writer& data) const
{
    data.Write("name", mName);
    data.Write("readonly", mReadOnly);
    data.Write("type", GetType());
    switch (GetType())
    {
        case Type::Integer:
            data.Write("value",GetValue<int>());
            break;
        case Type::Vec2:
            data.Write("value", GetValue<glm::vec2>());
            break;
        case Type::Boolean:
            data.Write("value",GetValue<bool>());
            break;
        case Type::String:
            data.Write("value", GetValue<std::string>());
            break;
        case Type::Float:
            data.Write("value", GetValue<float>());
            break;
    }
}

template<typename T, typename Variant>
bool JsonReadValue(const data::Reader& data, Variant* out)
{
    T value;
    if (!data.Read("value", &value))
        return false;
    *out = value;
    return true;
}

// static
std::optional<ScriptVar> ScriptVar::FromJson(const data::Reader& data)
{
    ScriptVar ret;
    ScriptVar::Type type = ScriptVar::Type::Integer;
    if (!data.Read("name", &ret.mName) ||
        !data.Read("type", &type) ||
        !data.Read("readonly", &ret.mReadOnly))
        return std::nullopt;

    switch (type) {
        case Type::Integer:
            if (!JsonReadValue<int>(data, &ret.mData))
                return std::nullopt;
            break;

        case Type::Vec2:
            if (!JsonReadValue<glm::vec2>(data, &ret.mData))
                return std::nullopt;
            break;

        case Type::Float:
            if (!JsonReadValue<float>(data, &ret.mData))
                return std::nullopt;
            break;

        case Type::String:
            if (!JsonReadValue<std::string>(data, &ret.mData))
                return std::nullopt;
            break;

        case Type::Boolean:
            if (!JsonReadValue<bool>(data, &ret.mData))
                return std::nullopt;
            break;
    }
    return ret;
}


} // namespace
