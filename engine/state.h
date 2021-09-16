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
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <variant>
#include <string>
#include <unordered_map>

#include "base/types.h"
#include "base/color4f.h"
#include "data/fwd.h"

namespace engine
{
    class KeyValueStore
    {
    public:
        using Value = std::variant<
                bool, int, float,
                std::string,
                glm::vec2, glm::vec3, glm::vec4,
                base::FPoint, base::FRect , base::FSize,
                base::Color4f>;
        bool HasValue(const std::string& key) const
        { return mValues.find(key) != mValues.end(); }
        Value GetValue(const std::string& key) const
        {
            auto it = mValues.find(key);
            if (it == mValues.end())
                return Value();
            return it->second;
        }
        bool GetValue(const std::string& key, Value* out) const
        {
            auto it = mValues.find(key);
            if (it == mValues.end())
                return false;
            *out = it->second;
            return true;
        }
        void SetValue(const std::string& key, const Value& value)
        { mValues[key] = value; }
        void SetValue(const std::string& key, Value&& value)
        { mValues[key] = std::move(value); }
        template<typename T>
        void SetValue(const std::string& key, T value)
        { mValues[key] = Value(value); }
        void Clear()
        { mValues.clear(); }

        void Persist(data::Writer& writer) const;
        bool Restore(const data::Reader& reader);
    private:
        std::unordered_map<std::string, Value> mValues;
    };

    using GameStateStore = KeyValueStore;

} // engine
