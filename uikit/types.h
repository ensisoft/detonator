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
        MouseLeave
    };
    // check whether the widget action is a notification about some superficial
    // state change such as widget gaining keyboard focus or the mouse being
    // over the widget.
    inline bool IsNotification(WidgetActionType type) {
        if (type == WidgetActionType::FocusChange ||
            type == WidgetActionType::MouseEnter ||
            type ==WidgetActionType::MouseLeave)
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

} // namespace
