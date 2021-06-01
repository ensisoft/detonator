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
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include <cmath>
#include <variant>
#include <string>
#include <optional>

#include "base/assert.h"
#include "base/types.h"

namespace game
{
    // type aliases for base types
    using FRect = base::FRect;
    using IRect = base::IRect;
    using IPoint = base::IPoint;
    using FPoint = base::FPoint;
    using FSize  = base::FSize;
    using ISize  = base::ISize;

    // Value supporting "arbitrary" values for scripting environments
    // such as Lua.
    class ScriptVar
    {
    public:
        static constexpr bool ReadOnly = true;
        static constexpr bool ReadWrite = false;

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
          : mName(std::move(name))
          , mData(std::move(value))
          , mReadOnly(read_only)
        {}

        // Get whether the variable is considered read-only/constant
        // in the scripting environment.
        bool IsReadOnly() const
        { return mReadOnly; }
        // Get the type of the variable.
        Type GetType() const;
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
        bool HasType() const
        { return std::holds_alternative<T>(mData); }

        // Get the hash value of the current parameters.
        size_t GetHash() const;

        // Serialize into JSON.
        nlohmann::json ToJson() const;

        static std::optional<ScriptVar> FromJson(const nlohmann::json& json);
    private:
        ScriptVar() = default;
        // name of the variable in the script.
        std::string mName;
        // the actual data.
        mutable std::variant<bool, float, int, std::string, glm::vec2> mData;
        // whether the variable is a read-only / constant in the
        // scripting environment. read only variables can be
        // shared by multiple object instances thus leading to
        // reduced memory consumption. (hence the default)
        bool mReadOnly = true;
    };

    // todo: refactor into base/types ?
    // Box represents a rectangular object which (unlike gfx::FRect)
    // also maintains orientation.
    class FBox
    {
    public:
        // Create a new unit box by default.
        FBox(float w = 1.0f, float h = 1.0f)
        {
            mTopLeft  = glm::vec2(0.0f, 0.0f);
            mTopRight = glm::vec2(w, 0.0f);
            mBotLeft  = glm::vec2(0.0f, h);
            mBotRight = glm::vec2(w, h);
        }
        FBox(const glm::mat4& mat, float w = 1.0f, float h = 1.0f)
        {
            mTopLeft  = ToVec2(mat * ToVec4(glm::vec2(0.0f, 0.0f)));
            mTopRight = ToVec2(mat * ToVec4(glm::vec2(w, 0.0f)));
            mBotLeft  = ToVec2(mat * ToVec4(glm::vec2(0.0f, h)));
            mBotRight = ToVec2(mat * ToVec4(glm::vec2(w, h)));
        }
        void Transform(const glm::mat4& mat)
        {
            mTopLeft  = ToVec2(mat * ToVec4(mTopLeft));
            mTopRight = ToVec2(mat * ToVec4(mTopRight));
            mBotLeft  = ToVec2(mat * ToVec4(mBotLeft));
            mBotRight = ToVec2(mat * ToVec4(mBotRight));
        }
        float GetWidth() const
        { return glm::length(mTopRight - mTopLeft); }
        float GetHeight() const
        { return glm::length(mBotLeft - mTopLeft); }
        float GetRotation() const
        {
            const auto dir = glm::normalize(mTopRight - mTopLeft);
            const auto cosine = glm::dot(glm::vec2(1.0f, 0.0f), dir);
            if (dir.y < 0.0f)
                return -std::acos(cosine);
            return std::acos(cosine);
        }
        glm::vec2 GetTopLeft() const
        { return mTopLeft; }
        glm::vec2 GetTopRight() const
        { return mTopRight; }
        glm::vec2 GetBotLeft() const
        { return mBotLeft; }
        glm::vec2 GetBotRight() const
        { return mBotRight; }
        glm::vec2 GetCenter() const
        {
            const auto diagonal = mBotRight - mTopLeft;
            return mTopLeft + diagonal * 0.5f;
        }
        glm::vec2 GetSize() const
        { return glm::vec2(GetWidth(), GetHeight()); }
        void Reset(float w = 1.0f, float h = 1.0f)
        {
            mTopLeft  = glm::vec2(0.0f, 0.0f);
            mTopRight = glm::vec2(w, 0.0f);
            mBotLeft  = glm::vec2(0.0f, h);
            mBotRight = glm::vec2(w, h);
        }
    private:
        static glm::vec4 ToVec4(const glm::vec2& vec)
        { return glm::vec4(vec.x, vec.y, 1.0f, 1.0f); }
        static glm::vec2 ToVec2(const glm::vec4& vec)
        { return glm::vec2(vec.x, vec.y); }
    private:
        // store the box as 4 2d points each
        // representing one corner of the box.
        // there are alternative ways too, such as
        // position + dim vectors and rotation
        // but this representation is quite simple.
        glm::vec2 mTopLeft;
        glm::vec2 mTopRight;
        glm::vec2 mBotLeft;
        glm::vec2 mBotRight;
    };

    inline FBox TransformBox(const FBox& box, const glm::mat4& mat)
    {
        FBox ret(box);
        ret.Transform(mat);
        return ret;
    }

} // namespace


