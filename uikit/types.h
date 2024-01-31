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
#include <string>
#include <unordered_map>

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
        None,
        Left,
        Wheel,
        WheelUp,
        WheelDown,
        Right
    };
    enum class VirtualKey {
        None,
        FocusNext,
        FocusPrev,
        MoveDown,
        MoveUp,
        MoveLeft,
        MoveRight,
        Select
    };

    enum class WidgetActionType {
        None,
        FocusChange,
        ButtonPress,
        ValueChange,
        RadioButtonSelect,
        SingleItemSelect,
        MouseEnter,
        MouseLeave,
        MouseGrabBegin,
        MouseGrabEnd
    };
    // check whether the widget action is a notification about some superficial
    // state change such as widget gaining keyboard focus or the mouse being
    // over the widget.
    inline bool IsNotification(WidgetActionType type) {
        if (type == WidgetActionType::FocusChange ||
            type == WidgetActionType::MouseEnter ||
            type == WidgetActionType::MouseLeave ||
            type == WidgetActionType::MouseGrabBegin ||
            type == WidgetActionType::MouseGrabEnd)
            return true;
        return false;
    }

    struct ListItem {
        std::string text;
        unsigned index = 0;
    };

    using WidgetActionValue = std::variant<int, float, bool, std::string, ListItem>;

    using StyleProperty = std::variant<int, float, bool, std::string, Color4f>;
    // maps property key such as 'text-color' to a property value.
    using StylePropertyMap = std::unordered_map<std::string, StyleProperty>;
    // maps material key such as 'border' to a material definition string
    using StyleMaterialMap = std::unordered_map<std::string, std::string>;


    // WidgetAction defines the response to some event.
    // For example when a push button receives a mouse button
    // down followed by a release it'll generate a ButtonPress
    // widget action. The caller can then choose the appropriate
    // action to take on such a response.
    // Not all events result in an action in which case the type
    // will be None.
    struct WidgetAction {
        // Name of the widget that generated the action.
        // Note that this isn't necessarily unique but depends on entirely
        // how the widgets have been named. If you're relying on this
        // for identifying the source of the event (such as OK button)
        // then make sure to use unique names.
        std::string name;
        // ID of the widget that generated the action. Unlike the name
        // the widget IDs are always created uniquely when a widget is
        // created (except when they're copied bitwise!). In terms of
        // identifying which widget generated the action this is more
        // reliable than the name.
        std::string id;
        // The action that is happening, for example ButtonPress,
        // ValueChange, ItemSelectionChanged etc.
        WidgetActionType type = WidgetActionType::None;
        // The actual value of the action if any. Depends on the
        // type of the action. see uikit/types.h for more details.
        WidgetActionValue value;
    };

} // namespace
