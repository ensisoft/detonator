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
{ return GetTypeFromVariant(mData); }


size_t ScriptVar::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mData);
    hash = base::hash_combine(hash, mReadOnly);
    return hash;
}

void ScriptVar::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("readonly", mReadOnly);
    data.Write("value", mData);
}

// static
void ScriptVar::IntoJson(const VariantType& variant, data::Writer& writer)
{
    writer.Write("value", variant);
}
// static
void ScriptVar::FromJson(const data::Reader& reader, VariantType* variant)
{
    reader.Read("value", variant);
}

// static
std::optional<ScriptVar> ScriptVar::FromJson(const data::Reader& data)
{
    ScriptVar ret;
    if (!data.Read("id", &ret.mId) ||
        !data.Read("name", &ret.mName) ||
        !data.Read("value", &ret.mData) ||
        !data.Read("readonly", &ret.mReadOnly))
        return std::nullopt;

    return ret;
}
// static
ScriptVar::Type ScriptVar::GetTypeFromVariant(const VariantType& variant)
{
    if (std::holds_alternative<int>(variant))
        return Type::Integer;
    else if (std::holds_alternative<float>(variant))
        return Type::Float;
    else if (std::holds_alternative<bool>(variant))
        return Type::Boolean;
    else if (std::holds_alternative<std::string>(variant))
        return Type::String;
    else if (std::holds_alternative<glm::vec2>(variant))
        return Type::Vec2;
    BUG("Unknown ScriptVar type!");
}

} // namespace
