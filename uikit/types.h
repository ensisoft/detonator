// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include <variant>
#include <unordered_map>
#include <string>
#include <any>

#include "base/assert.h"
// todo: refactor the include away.
#include "graphics/types.h"
#include "graphics/color4f.h"

namespace uik
{
    // provide type aliases for now for these types so that we can
    // use them as if they weren't in graphics where they shouldn't
    // be for most of the use in engine code. (i.e. not related to
    // graphics in any way)
    // todo: eventually should refactor them out of graphics/ into base/
    using FRect = gfx::FRect;
    using IRect = gfx::IRect;
    using IPoint = gfx::IPoint;
    using FPoint = gfx::FPoint;
    using FSize  = gfx::FSize;
    using ISize  = gfx::ISize;

    using Color4f = gfx::Color4f;
    using Color = gfx::Color;

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
