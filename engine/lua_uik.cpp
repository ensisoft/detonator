// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "warnpush.h"
#  include <sol/sol.hpp>
#include "warnpop.h"

#include "base/format.h"
#include "engine/lua.h"
#include "uikit/widget.h"
#include "uikit/window.h"

using namespace engine::lua;

namespace {
sol::object WidgetObjectCast(sol::this_state state, uik::Widget* widget)
{
    sol::state_view lua(state);
    const auto type = widget->GetType();
    if (type == uik::Widget::Type::Form)
        return sol::make_object(lua, uik::WidgetCast<uik::Form>(widget));
    else if (type == uik::Widget::Type::Label)
        return sol::make_object(lua, uik::WidgetCast<uik::Label>(widget));
    else if (type == uik::Widget::Type::SpinBox)
        return sol::make_object(lua, uik::WidgetCast<uik::SpinBox>(widget));
    else if (type == uik::Widget::Type::ProgressBar)
        return sol::make_object(lua, uik::WidgetCast<uik::ProgressBar>(widget));
    else if (type == uik::Widget::Type::Slider)
        return sol::make_object(lua, uik::WidgetCast<uik::Slider>(widget));
    else if (type == uik::Widget::Type::GroupBox)
        return sol::make_object(lua, uik::WidgetCast<uik::GroupBox>(widget));
    else if (type == uik::Widget::Type::PushButton)
        return sol::make_object(lua, uik::WidgetCast<uik::PushButton>(widget));
    else if (type == uik::Widget::Type::CheckBox)
        return sol::make_object(lua, uik::WidgetCast<uik::CheckBox>(widget));
    else BUG("Unhandled widget type cast.");
    return sol::make_object(lua, sol::nil);
}

// shim to help with uik::WidgetCast overload resolution.
template<typename Widget> inline
Widget* WidgetCast(uik::Widget* widget)
{ return uik::WidgetCast<Widget>(widget); }

template<typename Widget>
void BindWidgetInterface(sol::usertype<Widget>& widget)
{
    widget["GetId"]               = &Widget::GetId;
    widget["GetName"]             = &Widget::GetName;
    widget["GetHash"]             = &Widget::GetHash;
    widget["GetSize"]             = &Widget::GetSize;
    widget["GetPosition"]         = &Widget::GetPosition;
    widget["GetType"]             = [](const Widget* widget) { return base::ToString(widget->GetType()); };
    widget["SetName"]             = &Widget::SetName;
    widget["TestFlag"]            = &TestFlag<Widget>;
    widget["SetFlag"]             = &SetFlag<Widget>;
    widget["IsEnabled"]           = &Widget::IsEnabled;
    widget["IsVisible"]           = &Widget::IsVisible;
    widget["Grow"]                = &Widget::Grow;
    widget["Translate"]           = &Widget::Translate;
    widget["SetStyleProperty"]    = &Widget::SetStyleProperty;
    widget["DeleteStyleProperty"] = &Widget::DeleteStyleProperty;
    widget["GetStyleProperty"]    = [](Widget& widget, const std::string& key, sol::this_state state) {
        sol::state_view lua(state);
        if (const auto* prop = widget.GetStyleProperty(key))
            return sol::make_object(lua, *prop);
        return sol::make_object(lua, sol::nil);
    };
    widget["SetStyleMaterial"]    = &Widget::SetStyleMaterial;
    widget["DeleteStyleMaterial"] = &Widget::DeleteStyleMaterial;
    widget["GetStyleMaterial"]    = [](Widget& widget, const std::string& key, sol::this_state state) {
        sol::state_view lua(state);
        if (const auto* str = widget.GetStyleMaterial(key))
            return sol::make_object(lua, *str);
        return sol::make_object(lua, sol::nil);
    };
    widget["SetColor"]    = &Widget::SetColor;
    widget["SetMaterial"] = &Widget::SetMaterial;
    widget["SetGradient"] = &Widget::SetGradient;

    widget["SetVisible"]     = [](Widget& widget, bool on_off) {
        widget.SetFlag(Widget::Flags::VisibleInGame, on_off);
    };
    widget["Enable"] = [](Widget& widget, bool on_off) {
        widget.SetFlag(Widget::Flags::Enabled, on_off);
    };
    widget["SetSize"]        = sol::overload(
        [](Widget& widget, const uik::FSize& size)  {
            widget.SetSize(size);
        },
        [](Widget& widget, float width, float height) {
            widget.SetSize(width, height);
        });
    widget["SetPosition"] = sol::overload(
        [](Widget& widget, const uik::FPoint& point) {
            widget.SetPosition(point);
        },
        [](Widget& widget, float x, float y) {
            widget.SetPosition(x, y);
        });
}

} // namespace

namespace engine
{

void BindUIK(sol::state& L)
{
    auto table = L["uik"].get_or_create<sol::table>();

    auto widget = table.new_usertype<uik::Widget>("Widget");
    BindWidgetInterface(widget);
    widget["AsLabel"]        = &WidgetCast<uik::Label>;
    widget["AsPushButton"]   = &WidgetCast<uik::PushButton>;
    widget["AsCheckBox"]     = &WidgetCast<uik::CheckBox>;
    widget["AsGroupBox"]     = &WidgetCast<uik::GroupBox>;
    widget["AsSpinBox"]      = &WidgetCast<uik::SpinBox>;
    widget["AsProgressBar"]  = &WidgetCast<uik::ProgressBar>;
    widget["AsForm"]         = &WidgetCast<uik::Form>;
    widget["AsSlider"]       = &WidgetCast<uik::Slider>;
    widget["AsRadioButton"]  = &WidgetCast<uik::RadioButton>;

    auto form = table.new_usertype<uik::Form>("Form");
    BindWidgetInterface(form);

    auto label = table.new_usertype<uik::Label>("Label");
    BindWidgetInterface(label);
    label["GetText"] = &uik::Label::GetText;
    label["SetText"] = (void(uik::Label::*)(const std::string&))&uik::Label::SetText;

    auto checkbox = table.new_usertype<uik::CheckBox>("CheckBox");
    BindWidgetInterface(checkbox);
    checkbox["GetText"]    = &uik::CheckBox::GetText;
    checkbox["SetText"]    = (void(uik::CheckBox::*)(const std::string&))&uik::CheckBox::SetText;
    checkbox["IsChecked"]  = &uik::CheckBox::IsChecked;
    checkbox["SetChecked"] = &uik::CheckBox::SetChecked;

    auto groupbox = table.new_usertype<uik::GroupBox>("GroupBox");
    BindWidgetInterface(groupbox);
    groupbox["GetText"] = &uik::GroupBox::GetText;
    groupbox["SetText"] = (void(uik::GroupBox::*)(const std::string&))&uik::GroupBox::SetText;

    auto pushbtn = table.new_usertype<uik::PushButton>("PushButton");
    BindWidgetInterface(pushbtn);
    pushbtn["GetText"]  = &uik::PushButton::GetText;
    pushbtn["SetText"]  = (void(uik::PushButton::*)(const std::string&))&uik::PushButton::SetText;

    auto progressbar = table.new_usertype<uik::ProgressBar>("ProgressBar");
    BindWidgetInterface(progressbar);
    progressbar["SetText"]    = &uik::ProgressBar::SetText;
    progressbar["GetText"]    = &uik::ProgressBar::GetText;
    progressbar["ClearValue"] = &uik::ProgressBar::ClearValue;
    progressbar["SetValue"]   = &uik::ProgressBar::SetValue;
    progressbar["HasValue"]   = &uik::ProgressBar::HasValue;
    progressbar["GetValue"]   = [](uik::ProgressBar* progress, sol::this_state state) {
        sol::state_view view(state);
        if (progress->HasValue())
            return sol::make_object(view, progress->GetValue(0.0f));
        return sol::make_object(view, sol::nil);
    };

    auto spinbox = table.new_usertype<uik::SpinBox>("SpinBox");
    BindWidgetInterface(spinbox);
    spinbox["SetMin"]   = &uik::SpinBox::SetMin;
    spinbox["SetMax"]   = &uik::SpinBox::SetMax;
    spinbox["SetValue"] = &uik::SpinBox::SetValue;
    spinbox["GetMin"]   = &uik::SpinBox::GetMin;
    spinbox["GetMax"]   = &uik::SpinBox::GetMax;
    spinbox["GetValue"] = &uik::SpinBox::GetValue;

    auto slider = table.new_usertype<uik::Slider>("Slider");
    BindWidgetInterface(slider);
    slider["SetValue"] = &uik::Slider::SetValue;
    slider["GetValue"] = &uik::Slider::GetValue;

    auto radio = table.new_usertype<uik::RadioButton>("RadioButton");
    BindWidgetInterface(radio);
    radio["Select"]     = &uik::RadioButton::Select;
    radio["IsSelected"] = &uik::RadioButton::IsSelected;
    radio["GetText"]    = &uik::RadioButton::GetText;
    radio["SetText"]    = &uik::RadioButton::SetText;

    auto window = table.new_usertype<uik::Window>("Window",
        sol::meta_function::index, [](sol::this_state state, uik::Window* window, const char* key) {
                sol::state_view lua(state);
                auto* widget = window->FindWidgetByName(key);
                if (!widget)
                    return sol::make_object(lua, sol::nil);
                return WidgetObjectCast(state, widget);
            });
    window["GetId"]            = &uik::Window::GetId;
    window["GetName"]          = &uik::Window::GetName;
    window["GetNumWidgets"]    = &uik::Window::GetNumWidgets;
    window["FindWidgetById"]   = [](sol::this_state state, uik::Window* window, const std::string& id) {
        sol::state_view lua(state);
        auto* widget = window->FindWidgetById(id);
        if (!widget)
            return sol::make_object(lua, sol::nil);
        return WidgetObjectCast(state, widget);
    };
    window["FindWidgetByName"] =  [](sol::this_state state, uik::Window* window, const std::string& name) {
        sol::state_view lua(state);
        auto* widget = window->FindWidgetByName(name);
        if (!widget)
            return sol::make_object(lua, sol::nil);
        return WidgetObjectCast(state, widget);
    };
    window["FindWidgetParent"] = [](sol::this_state state, uik::Window* window, uik::Widget* child) {
        return WidgetObjectCast(state, window->FindParent(child));
    };
    window["GetWidget"]        = [](sol::this_state state, uik::Window* window, unsigned index) {
        if (index >= window->GetNumWidgets())
            throw GameError(base::FormatString("Widget index %1 is out of bounds", index));
        return WidgetObjectCast(state, &window->GetWidget(index));
    };

    auto action = table.new_usertype<uik::Window::WidgetAction>("Action",
    sol::meta_function::index, [&L](const uik::Window::WidgetAction* action, const char* key) {
        sol::state_view lua(L);
        if (!std::strcmp(key, "name"))
            return sol::make_object(lua, action->name);
        else if (!std::strcmp(key, "id"))
            return sol::make_object(lua, action->id);
        else if (!std::strcmp(key, "type"))
            return sol::make_object(lua, base::ToString(action->type));
        else if (!std::strcmp(key, "value")) {
            if (std::holds_alternative<int>(action->value))
                return sol::make_object(lua, std::get<int>(action->value));
            else if (std::holds_alternative<float>(action->value))
                return sol::make_object(lua, std::get<float>(action->value));
            else if (std::holds_alternative<bool>(action->value))
                return sol::make_object(lua, std::get<bool>(action->value));
            else if (std::holds_alternative<std::string>(action->value))
                return sol::make_object(lua, std::get<std::string>(action->value));
            else BUG("???");
        }
        throw GameError(base::FormatString("No such ui action index: %1", key));
    });
}

} // namespace