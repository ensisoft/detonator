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

#include "warnpop.h"

#include <variant>
#include <unordered_map>
#include <string>
#include <any>

#include "base/assert.h"
#include "base/types.h"
#include "base/color4f.h"

namespace uik
{
    using FRect = base::FRect;
    using IRect = base::IRect;
    using IPoint = base::IPoint;
    using FPoint = base::FPoint;
    using FSize  = base::FSize;
    using ISize  = base::ISize;
    using Color4f = base::Color4f;
    using Color   = base::Color;

    enum class MouseButton {
        None, Left, Wheel, WheelUp, WheelDown, Right
    };
    enum class VirtualKey {

    };

    enum class WidgetActionType {
        None,
        ButtonPress,
        ValueChanged,
        SingleItemSelected
    };
    struct ListItem {
        std::string text;
        unsigned index = 0;
    };

    using WidgetActionValue = std::variant<int, float, bool, std::string, ListItem>;

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
