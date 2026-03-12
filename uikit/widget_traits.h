// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include "uikit/widget.h"
#include "uikit/widget_model.h"

namespace uik
{
    namespace detail {
        struct WidgetTraits {
            static constexpr auto InitialWidth      = 100;
            static constexpr auto InitialHeight     = 30;
            static constexpr auto WantsMouseEvents  = false;
            static constexpr auto WantsKeyEvents    = false;
            static constexpr auto WantsUpdate       = false;
            static constexpr auto WantsPoll         = false;
            static constexpr auto HasClippingRect   = false;
            static constexpr auto HasViewportRect   = false;
            static constexpr auto HasChildTransform = false;
            static constexpr auto HasStyleQuery     = false;
        };

        template<typename WidgetModel>
        struct WidgetModelTraits;

        template<>
        struct WidgetModelTraits<ShapeModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::ShapeWidget;
            static constexpr auto InitialWidth  = 100;
            static constexpr auto InitialHeight = 100;
        };

        template<>
        struct WidgetModelTraits<ScrollAreaModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::ScrollArea;
            static constexpr auto InitialWidth      = 200;
            static constexpr auto InitialHeight     = 200;
            static constexpr auto WantsMouseEvents  = true;
            static constexpr auto WantsPoll         = true;
            static constexpr auto HasClippingRect   = true;
            static constexpr auto HasChildTransform = true;
            static constexpr auto HasStyleQuery     = true;
        };
        template<>
        struct WidgetModelTraits<FormModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::Form;
            static constexpr auto InitialWidth  = 1024;
            static constexpr auto InitialHeight = 768;
        };

        template<>
        struct WidgetModelTraits<LabelModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::Label;
        };
        template<>
        struct WidgetModelTraits<PushButtonModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::PushButton;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
        };
        template<>
        struct WidgetModelTraits<CheckBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::CheckBox;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
        };
        template<>
        struct WidgetModelTraits<ToggleBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::ToggleBox;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
            static constexpr auto WantsPoll        = true;
            static constexpr auto WantsUpdate      = true;
            static constexpr auto InitialWidth     = 60;
            static constexpr auto InitialHeight    = 30;
        };
        template<>
        struct WidgetModelTraits<RadioButtonModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::RadioButton;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
            static constexpr auto WantsPoll = true;
            static constexpr auto InitialWidth = 160;
        };
        template<>
        struct WidgetModelTraits<GroupBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::GroupBox;
            static constexpr auto InitialWidth  = 200;
            static constexpr auto InitialHeight = 200;
        };
        template<>
        struct WidgetModelTraits<SpinBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::SpinBox;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
            static constexpr auto WantsPoll = true;
        };
        template<>
        struct WidgetModelTraits<SliderModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::Slider;
            static constexpr auto InitialWidth     = 200;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
        };

        template<>
        struct WidgetModelTraits<ProgressBarModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::ProgressBar;
            static constexpr auto InitialWidth = 200;
        };

    } // namespace
} // namespace
