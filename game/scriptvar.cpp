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

namespace {
template<typename PrimitiveType>
void write_primitive_array(const char* array, const game::ScriptVar::VariantType& variant,  data::Writer& writer)
{
    ASSERT(std::holds_alternative<std::vector<PrimitiveType>>(variant));
    const auto& value = std::get<std::vector<PrimitiveType>>(variant);
    writer.Write(array, &value[0], value.size());
}
template<typename PrimitiveType>
void read_primitive_array(const char* array, const data::Reader& reader, game::ScriptVar::VariantType* variant)
{
    std::vector<PrimitiveType> arr;
    for (unsigned i=0; i<reader.GetNumItems(array); ++i)
    {
        PrimitiveType item;
        reader.Read(array, i, &item);
        arr.push_back(std::move(item));
    }
    *variant = std::move(arr);
}

template<typename ObjectType>
void write_object_array(const char* array, const game::ScriptVar::VariantType& variant, data::Writer& writer)
{
    ASSERT(std::holds_alternative<std::vector<ObjectType>>(variant));
    const auto& objects = std::get<std::vector<ObjectType>>(variant);
    for (auto& object : objects)
    {
        auto chunk = writer.NewWriteChunk();
        chunk->Write("object", object);
        writer.AppendChunk(array, std::move(chunk));
    }
}

template<typename ObjectType>
void read_object_array(const char* array, const data::Reader& reader, game::ScriptVar::VariantType* variant)
{
    std::vector<ObjectType> ret;
    for (unsigned i=0; i<reader.GetNumChunks(array); ++i)
    {
        const auto& chunk = reader.GetReadChunk(array, i);
        ObjectType obj;
        chunk->Read("object", &obj);
        ret.push_back(std::move(obj));
    }
    *variant = std::move(ret);
}

void read_bool_array(const char* array, const data::Reader& reader, game::ScriptVar::VariantType* variant)
{
    std::vector<bool> arr;
    for (unsigned i=0; i<reader.GetNumItems(array); ++i)
    {
        int value = 0;
        reader.Read(array, i, &value);
        arr.push_back(value == 1 ? true : false);
    }
    *variant = std::move(arr);
}

void write_bool_array(const char* array, const game::ScriptVar::VariantType& variant, data::Writer& writer)
{
    ASSERT(std::holds_alternative<std::vector<bool>>(variant));
    const auto& values = std::get<std::vector<bool>>(variant);
    std::vector<int> tmp;
    for (bool b : values)
        tmp.push_back(b ? 1 : 0);
    writer.Write(array, &tmp[0], tmp.size());
}
} // namespace

namespace game
{
ScriptVar::Type ScriptVar::GetType() const
{ return GetTypeFromVariant(mData); }

bool ScriptVar::IsArray() const
{
    bool ret = false;
    std::visit([&ret](const auto& variant_value_vector) {
        ret = variant_value_vector.size() > 1;
    }, mData);
    return ret;
}

size_t ScriptVar::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mReadOnly);
    hash = base::hash_combine(hash, GetHash(mData));
    return hash;
}

void ScriptVar::IntoJson(data::Writer& writer) const
{
    writer.Write("id", mId);
    writer.Write("name", mName);
    writer.Write("readonly", mReadOnly);
    IntoJson(mData, writer);
}

// static
size_t ScriptVar::GetHash(const VariantType& variant)
{
    size_t hash = 0;
    std::visit([&hash](const auto& variant_value) {
        // the type of variant_value should be a vector<T>
        for (const auto& val : variant_value)
            hash = base::hash_combine(hash, val);
    }, variant);
    return hash;
}

// static
void ScriptVar::IntoJson(const VariantType& variant, data::Writer& writer)
{
    // this is what we had before, just a single variant.
    // in order to support "arrays" the value is now a vector instead.
    //writer.Write("value", variant);

    const auto type = GetTypeFromVariant(variant);
    if (type == Type::String)
        write_primitive_array<std::string>("strings", variant, writer);
    else if (type == Type::Integer)
        write_primitive_array<int>("ints", variant, writer);
    else if (type == Type::Float)
        write_primitive_array<float>("floats", variant, writer);
    else if (type == Type::Boolean)
        write_bool_array("bools", variant, writer);
    else if (type == Type::Vec2)
        write_object_array<glm::vec2>("vec2s", variant, writer);
    else BUG("Unhandled script variable type.");
}
// static
void ScriptVar::FromJson(const data::Reader& reader, VariantType* variant)
{
    //reader.Read("value", variant);
    if (reader.HasArray("strings"))
        read_primitive_array<std::string>("strings", reader, variant);
    else if (reader.HasArray("ints"))
        read_primitive_array<int>("ints", reader, variant);
    else if (reader.HasArray("floats"))
        read_primitive_array<float>("floats", reader, variant);
    else if (reader.HasArray("bools"))
        read_bool_array("bools", reader, variant);
    else if (reader.HasArray("vec2s"))
        read_object_array<glm::vec2>("vec2s", reader, variant);
    else BUG("Unhandled script variable type.");
}

// static
std::optional<ScriptVar> ScriptVar::FromJson(const data::Reader& reader)
{
    ScriptVar ret;
    reader.Read("id", &ret.mId);
    reader.Read("name", &ret.mName);
    reader.Read("readonly", &ret.mReadOnly);

    // migration path from a single variant to a variant of arrays
    using OldVariant = std::variant<bool, float, int, std::string, glm::vec2>;
    OldVariant  old_data;
    if (reader.Read("value", &old_data))
    {
        if (const auto* ptr = std::get_if<int>(&old_data))
            ret.mData = std::vector<int> {*ptr};
        else if (const auto* ptr = std::get_if<float>(&old_data))
            ret.mData = std::vector<float> {*ptr};
        else if (const auto* ptr = std::get_if<bool>(&old_data))
            ret.mData = std::vector<bool> {*ptr};
        else if (const auto* ptr = std::get_if<std::string>(&old_data))
            ret.mData = std::vector<std::string> {*ptr};
        else if (const auto* ptr = std::get_if<glm::vec2>(&old_data))
            ret.mData = std::vector<glm::vec2> {*ptr};
    }
    else
    {
        FromJson(reader, &ret.mData);
    }
    return ret;
}
// static
ScriptVar::Type ScriptVar::GetTypeFromVariant(const VariantType& variant)
{
    if (std::holds_alternative<std::vector<int>>(variant))
        return Type::Integer;
    else if (std::holds_alternative<std::vector<float>>(variant))
        return Type::Float;
    else if (std::holds_alternative<std::vector<bool>>(variant))
        return Type::Boolean;
    else if (std::holds_alternative<std::vector<std::string>>(variant))
        return Type::String;
    else if (std::holds_alternative<std::vector<glm::vec2>>(variant))
        return Type::Vec2;
    BUG("Unknown ScriptVar type!");
}

} // namespace
