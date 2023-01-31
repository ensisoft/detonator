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

#include <type_traits>

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
bool read_primitive_array(const char* array, const data::Reader& reader, game::ScriptVar::VariantType* variant)
{
    bool ok = true;
    std::vector<PrimitiveType> arr;
    for (unsigned i=0; i<reader.GetNumItems(array); ++i)
    {
        PrimitiveType item;
        ok &= reader.Read(array, i, &item);
        arr.push_back(std::move(item));
    }
    *variant = std::move(arr);
    return ok;
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
bool read_object_array(const char* array, const data::Reader& reader, game::ScriptVar::VariantType* variant)
{
    bool ok = true;
    std::vector<ObjectType> ret;
    for (unsigned i=0; i<reader.GetNumChunks(array); ++i)
    {
        const auto& chunk = reader.GetReadChunk(array, i);
        ObjectType obj;
        ok &= chunk->Read("object", &obj);
        ret.push_back(std::move(obj));
    }
    *variant = std::move(ret);
    return ok;
}

bool read_bool_array(const char* array, const data::Reader& reader, game::ScriptVar::VariantType* variant)
{
    bool ok = true;
    std::vector<bool> arr;
    for (unsigned i=0; i<reader.GetNumItems(array); ++i)
    {
        int value = 0;
        ok &= reader.Read(array, i, &value);
        arr.push_back(value == 1 ? true : false);
    }
    *variant = std::move(arr);
    return ok;
}

template<typename T>
bool read_reference_array(const char* array, const data::Reader& reader, game::ScriptVar::VariantType* variant)
{
    bool ok = true;
    std::vector<T> ret;
    for (unsigned i=0; i<reader.GetNumItems(array); ++i)
    {
        T reference;
        ok &= reader.Read(array, i, &reference.id);
        ret.push_back(std::move(reference));
    }
    *variant = std::move(ret);
    return ok;
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
template<typename RefType>
void write_reference_array(const char* array, const game::ScriptVar::VariantType& variant, data::Writer& writer)
{
    ASSERT(std::holds_alternative<std::vector<RefType>>(variant));
    const auto& values = std::get<std::vector<RefType>>(variant);

    std::vector<std::string> ids;
    for (const auto& val : values)
        ids.push_back(val.id);

    writer.Write(array, &ids[0], ids.size());
}

} // namespace

namespace game
{
ScriptVar::Type ScriptVar::GetType() const
{ return GetTypeFromVariant(mData); }

void ScriptVar::AppendItem()
{
    std::visit([](auto& variant_value_vector) {
        const auto size = variant_value_vector.size();
        variant_value_vector.resize(size+1);
    }, mData);
}
void ScriptVar::RemoveItem(size_t index)
{
    std::visit([index](auto& variant_value_vector) {
        ASSERT(index < variant_value_vector.size());
        auto it = variant_value_vector.begin();
        std::advance(it, index);
        variant_value_vector.erase(it);
    }, mData);
}

void ScriptVar::ResizeToOne()
{
    std::visit([](auto& variant_value_vector) {
        variant_value_vector.resize(1);
    }, mData);
}

void ScriptVar::Resize(size_t size)
{
    std::visit([size](auto& variant_value_vector) {
        variant_value_vector.resize(size);
    }, mData);
}

size_t ScriptVar::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mReadOnly);
    hash = base::hash_combine(hash, mIsArray);
    hash = base::hash_combine(hash, GetHash(mData));
    return hash;
}
size_t ScriptVar::GetArraySize() const
{
    size_t ret = 0;
    std::visit([&ret](const auto& variant_value_vector) {
        ret = variant_value_vector.size();
    }, mData);
    return ret;
}

void ScriptVar::IntoJson(data::Writer& writer) const
{
    writer.Write("id",       mId);
    writer.Write("name",     mName);
    writer.Write("readonly", mReadOnly);
    writer.Write("array",    mIsArray);
    IntoJson(mData, writer);
}

bool ScriptVar::FromJson(const data::Reader& reader)
{
    bool ok = true;
    ok &= reader.Read("id",       &mId);
    ok &= reader.Read("name",     &mName);
    ok &= reader.Read("readonly", &mReadOnly);
    ok &= reader.Read("array",    &mIsArray);
    ok &= FromJson(reader,        &mData);
    return ok;
}

// static
size_t ScriptVar::GetHash(const VariantType& variant)
{
    size_t hash = 0;
    std::visit([&hash](const auto& variant_value) {
        // the type of variant_value should be a vector<T>
        using VectorType = std::decay_t<decltype(variant_value)>;

        for (const auto& val : variant_value)
        {
            using ValueType = typename VectorType::value_type;

            // WAR for emscripten shitting itself with vector<bool>
            // use a temp variable for the actual value.
            const ValueType tmp = val;
            if constexpr (std::is_same<ValueType, EntityReference>::value)
                hash = base::hash_combine(hash, tmp.id);
            else if constexpr (std::is_same<ValueType, EntityNodeReference>::value)
                hash = base::hash_combine(hash, tmp.id);
            else hash = base::hash_combine(hash, tmp);
        }
    }, variant);
    return hash;
}
// static
size_t ScriptVar::GetArraySize(const VariantType& variant)
{
    size_t size = 0;
    std::visit([&size](const auto& variant_value_vector) {
        size = variant_value_vector.size();
    },  variant);
    return size;
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
    else if (type == Type::EntityReference)
        write_reference_array<EntityReference>("entity_refs", variant, writer);
    else if (type == Type::EntityNodeReference)
        write_reference_array<EntityNodeReference>("entity_node_refs", variant, writer);
    else BUG("Unhandled script variable type.");
}
// static
bool ScriptVar::FromJson(const data::Reader& reader, VariantType* variant)
{
    // migration path from a single variant to a variant of arrays
    using OldVariant = std::variant<bool, float, int, std::string, glm::vec2>;
    OldVariant  old_data;
    if (reader.Read("value", &old_data))
    {
        if (const auto* ptr = std::get_if<int>(&old_data))
            *variant = std::vector<int> {*ptr};
        else if (const auto* ptr = std::get_if<float>(&old_data))
            *variant = std::vector<float> {*ptr};
        else if (const auto* ptr = std::get_if<bool>(&old_data))
            *variant = std::vector<bool> {*ptr};
        else if (const auto* ptr = std::get_if<std::string>(&old_data))
            *variant = std::vector<std::string> {*ptr};
        else if (const auto* ptr = std::get_if<glm::vec2>(&old_data))
            *variant = std::vector<glm::vec2> {*ptr};
        else BUG("Unhandled script variable type.");
    }
    else
    {
        if (reader.HasArray("strings"))
            return read_primitive_array<std::string>("strings", reader, variant);
        else if (reader.HasArray("ints"))
            return read_primitive_array<int>("ints", reader, variant);
        else if (reader.HasArray("floats"))
            return read_primitive_array<float>("floats", reader, variant);
        else if (reader.HasArray("bools"))
            return read_bool_array("bools", reader, variant);
        else if (reader.HasArray("vec2s"))
            return read_object_array<glm::vec2>("vec2s", reader, variant);
        else if (reader.HasArray("entity_refs"))
            return read_reference_array<EntityReference>("entity_refs", reader, variant);
        else if (reader.HasArray("entity_node_refs"))
            return read_reference_array<EntityNodeReference>("entity_node_refs", reader, variant);
        else return false;
    }
    return true;
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
    else if (std::holds_alternative<std::vector<EntityNodeReference>>(variant))
        return Type::EntityNodeReference;
    else if (std::holds_alternative<std::vector<EntityReference>>(variant))
        return Type::EntityReference;
    else BUG("Unknown ScriptVar type!");
}

} // namespace
