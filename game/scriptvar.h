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
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <variant>
#include <string>
#include <vector>

#include "base/assert.h"
#include "base/utility.h"
#include "data/fwd.h"

namespace game
{
    // Value supporting "arbitrary" values for scripting environments
    // such as Lua.
    class ScriptVar
    {
    public:
        static constexpr bool ReadOnly = true;
        static constexpr bool ReadWrite = false;

        template<size_t Index>
        struct ObjectReference {
            std::string id;
        };
        using EntityReference = ObjectReference<0>;
        using EntityNodeReference = ObjectReference<1>;

        // So instead of holding a single variant
        // we want to have possibility to store an "array"
        // of variables. And to that extent we have a
        // variant of arrays rather than an array of variants
        // since with variant of arrays there's no question
        // about what to do with heterogeneous arrays.
        using VariantType = std::variant<
                std::vector<bool>,
                std::vector<float>,
                std::vector<int>,
                std::vector<std::string>,
                std::vector<glm::vec2>,
                std::vector<EntityReference>,
                std::vector<EntityNodeReference>>;

        // The types of values supported by the ScriptVar.
        enum class Type {
            String,
            Integer,
            Float,
            Vec2,
            Boolean,
            EntityReference,
            EntityNodeReference
        };
        template<typename T>
        ScriptVar(std::string name, T value, bool read_only = true)
          : mId(base::RandomString(10))
          , mName(std::move(name))
          , mReadOnly(read_only)
          , mIsArray(false)
        {
              mData = std::vector<T>{std::move(value)};
        }
        template<typename T>
        ScriptVar(std::string name, std::vector<T> array, bool read_only = true)
          : mId(base::RandomString(10))
          , mName(std::move(name))
          , mReadOnly(read_only)
          , mIsArray(true)
        {
              mData = std::move(array);
        }
        ScriptVar()
          : mId(base::RandomString(10))
        {}

        // Get whether the variable is considered read-only/constant
        // in the scripting environment.
        bool IsReadOnly() const
        { return mReadOnly; }
        bool IsArray() const
        { return mIsArray; }
        // Get the type of the variable.
        Type GetType() const;
        // Get the script variable ID.
        const std::string& GetId() const
        { return mId; }
        // Get the script variable name.
        const std::string& GetName() const
        { return mName; }

        // Get the actual value. The ScriptVar *must* hold that
        // type internally for the data item.
        template<typename T>
        T GetValue() const
        {
            // returning by value because of vector<bool> returns a temporary.

            ASSERT(std::holds_alternative<std::vector<T>>(mData));
            const auto& array = std::get<std::vector<T>>(mData);
            ASSERT(array.size() == 1);
            return array[0];
        }
        template<typename T>
        std::vector<T>& GetArray() const
        {
            // see the comments below about why this function is marked
            ASSERT(std::holds_alternative<std::vector<T>>(mData));
            return std::get<std::vector<T>>(mData);
        }

        // Set a new value in the script var. The value must
        // have the same type as previously (i.e. always match the
        // type of the encapsulated value inside ScriptVar) and
        // additionally not be read only.
        // marked const to allow the value that is held to change while
        // retaining the semantical constness of being a C++ ScriptVar object.
        // The value however can change since that "constness" is expressed
        // with the boolean read-only flag.
        template<typename T>
        void SetArray(std::vector<T> values) const
        {
            ASSERT(std::holds_alternative<std::vector<T>>(mData));
            ASSERT(mReadOnly == false);
            mData = std::move(values);
        }
        template<typename T>
        void SetValue(T value) const
        {
            ASSERT(std::holds_alternative<std::vector<T>>(mData));
            ASSERT(mReadOnly == false);
            auto& array = std::get<std::vector<T>>(mData);
            ASSERT(array.size() == 1);
            array[0] = std::move(value);
        }

        void SetData(VariantType&& data)
        { mData = std::move(data); }
        void SetData(const VariantType& data)
        { mData = data; }

        template<typename T>
        void SetNewValueType(T value)
        { mData = std::vector<T> {value}; }
        template<typename T>
        void SetNewArrayType(std::vector<T> array)
        { mData = std::move(array); }
        void SetName(const std::string& name)
        { mName = name; }
        void SetReadOnly(bool read_only)
        { mReadOnly = read_only; }
        void SetArray(bool array)
        { mIsArray = array; }
        const VariantType& GetVariantValue() const
        { return mData; }

        template<typename T>
        bool HasType() const
        { return std::holds_alternative<std::vector<T>>(mData); }

        void AppendItem();
        void RemoveItem(size_t index);
        void ResizeToOne();
        void Resize(size_t size);

        // Get the hash value of the current parameters.
        size_t GetHash() const;

        size_t GetArraySize() const;

        // Serialize into JSON.
        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);

        static size_t GetHash(const VariantType& variant);
        static size_t GetArraySize(const VariantType& variant);
        static void IntoJson(const VariantType& variant, data::Writer& writer);
        static bool FromJson(const data::Reader& reader, VariantType* variant);
        static Type GetTypeFromVariant(const VariantType& variant);

        template<typename T>
        static std::vector<T>& GetVectorFromVariant(VariantType& variant)
        {
            ASSERT(std::holds_alternative<std::vector<T>>(variant));
            return std::get<std::vector<T>>(variant);
        }
        template<typename T>
        static const std::vector<T>& GetVectorFromVariant(const VariantType& variant)
        {
            ASSERT(std::holds_alternative<std::vector<T>>(variant));
            return std::get<std::vector<T>>(variant);
        }

    private:
        // ID of the script variable.
        std::string mId;
        // name of the variable in the script.
        std::string mName;
        // the actual data.
        mutable VariantType mData;
        // whether the variable is a read-only / constant in the
        // scripting environment. read only variables can be
        // shared by multiple object instances thus leading to
        // reduced memory consumption. (hence the default)
        bool mReadOnly = true;
        bool mIsArray = false;
    };
} // namespace

