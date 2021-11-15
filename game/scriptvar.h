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
#include <optional>

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

        using VariantType = std::variant<bool, float, int, std::string, glm::vec2>;

        // The types of values supported by the ScriptVar.
        enum class Type {
            String,
            Integer,
            Float,
            Vec2,
            Boolean
        };
        template<typename T>
        ScriptVar(std::string name, T value, bool read_only = true)
          : mId(base::RandomString(10))
          , mName(std::move(name))
          , mData(std::move(value))
          , mReadOnly(read_only)
        {}

        // Get whether the variable is considered read-only/constant
        // in the scripting environment.
        bool IsReadOnly() const
        { return mReadOnly; }
        // Get the type of the variable.
        Type GetType() const;
        // Get the script variable ID.
        std::string GetId() const
        { return mId; }
        // Get the script variable name.
        std::string GetName() const
        { return mName; }
        // Get the actual value. The ScriptVar *must* hold that
        // type internally for the data item.
        template<typename T>
        T GetValue() const
        {
            ASSERT(std::holds_alternative<T>(mData));
            return std::get<T>(mData);
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
        void SetValue(T value) const
        {
            ASSERT(std::holds_alternative<T>(mData));
            ASSERT(mReadOnly == false);
            mData = std::move(value);
        }

        template<typename T>
        void SetNewValueType(T value)
        { mData = value; }
        void SetName(const std::string& name)
        { mName = name; }
        void SetReadOnly(bool read_only)
        { mReadOnly = read_only; }
        void SetVariantValue(VariantType value)
        { mData = value; }

        VariantType GetVariantValue() const
        { return mData; }

        template<typename T>
        bool HasType() const
        { return std::holds_alternative<T>(mData); }

        // Get the hash value of the current parameters.
        size_t GetHash() const;

        // Serialize into JSON.
        void IntoJson(data::Writer& data) const;

        static std::optional<ScriptVar> FromJson(const data::Reader& data);

        static Type GetTypeFromVariant(const VariantType& variant);
    private:
        ScriptVar() = default;
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
    };
} // namespace

