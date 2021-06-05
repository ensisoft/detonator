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

#include <unordered_map>
#include <any>
#include <string>

#include "base/assert.h"

namespace uik
{
    // Bitbag for the transient state that only exists when the
    // widget system is reacting to events such as mouse input etc.
    // Normally this state is discarded when the UI is no longer needed
    // and cleared/re-initialized when the UI is launched.
    // This state is separated because with this design we can avoid having
    // to ponder things such as "why is this value not assigned to in a
    // ctor or assignment op?" or "why are these fields not written into JSON?"
    // rather the division of state between persistent state and transient state
    // is very clear.
    class State
    {
    public:
        bool HasValue(const std::string& key) const
        {
            auto it = mState.find(key);
            if (it == mState.end())
                return false;
            return true;
        }
        template<typename T>
        T GetValue(const std::string& key, const T& backup) const
        {
            auto it = mState.find(key);
            if (it == mState.end())
                return backup;
            return std::any_cast<T>(it->second);
        }
        template<typename T>
        void GetValue(const std::string& key, T** out) const
        {
            auto it = mState.find(key);
            if (it == mState.end())
                return;
            const auto& any = it->second;
            if (any.type() == typeid(T*))
                *out = std::any_cast<T*>(any);
            else if (any.type() == typeid(const T*))
                *out = const_cast<T*>(std::any_cast<const T*>(any));
            else BUG("???");
        }
        template<typename T>
        void GetValue(const std::string& key, const T** out) const
        {
            auto it = mState.find(key);
            if (it == mState.end())
                return;
            const auto& any = it->second;
            if (any.type() == typeid(T*))
                *out = std::any_cast<T*>(any);
            else if (any.type() == typeid(const T*))
                *out = std::any_cast<const T*>(any);
            else BUG("???");
        }
        template<typename T>
        void SetValue(const std::string& key, const T& value)
        { mState[key] = value; }
        template<typename T>
        void SetValue(const std::string& key, const T* value)
        { mState[key] = const_cast<T*>(value); }
        template<typename T>
        void SetValue(const std::string& key, T* value)
        { mState[key] = value; }

        void Clear()
        { mState.clear(); }
    private:
        std::unordered_map<std::string, std::any> mState;
    };

} // namespace
