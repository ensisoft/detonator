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

#include <memory>

#include "uikit/widget.h"
#include "uikit/widget_model.h"
#include "uikit/widget_base.h"

namespace uik
{
    using Label       = detail::BasicModelWidget<detail::LabelModel>;
    using PushButton  = detail::BasicModelWidget<detail::PushButtonModel>;
    using CheckBox    = detail::BasicModelWidget<detail::CheckBoxModel>;
    using ToggleBox   = detail::BasicModelWidget<detail::ToggleBoxModel>;
    using SpinBox     = detail::BasicModelWidget<detail::SpinBoxModel>;
    using Slider      = detail::BasicModelWidget<detail::SliderModel>;
    using ProgressBar = detail::BasicModelWidget<detail::ProgressBarModel>;
    using GroupBox    = detail::BasicModelWidget<detail::GroupBoxModel>;
    using Form        = detail::BasicModelWidget<detail::FormModel>;
    using RadioButton = detail::BasicModelWidget<detail::RadioButtonModel>;
    using ScrollArea  = detail::BasicModelWidget<detail::ScrollAreaModel>;
    using ShapeWidget = detail::BasicModelWidget<detail::ShapeModel>;

    template<typename WidgetType>
    WidgetType* WidgetCast(Widget* widget)
    {
        if (widget->GetType() == WidgetType::RuntimeWidgetType)
            return static_cast<WidgetType*>(widget);
        return nullptr;
    }
    template<typename WidgetType>
    const WidgetType* WidgetCast(const Widget* widget)
    {
        if (widget->GetType() == WidgetType::RuntimeWidgetType)
            return static_cast<const WidgetType*>(widget);
        return nullptr;
    }

    template<typename WidgetType>
    WidgetType* WidgetCast(std::unique_ptr<Widget>& widget)
    {
        return WidgetCast<WidgetType>(widget.get());
    }

    template<typename WidgetType>
    const WidgetType* WidgetCast(const std::unique_ptr<Widget>& widget)
    {
        return WidgetCast<WidgetType>(widget.get());
    }

    std::unique_ptr<Widget> CreateWidget(uik::Widget::Type type);

} // namespace