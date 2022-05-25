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

#include "config.h"

#include "base/assert.h"
#include "base/hash.h"
#include "base/format.h"
#include "data/reader.h"
#include "data/writer.h"
#include "uikit/widget.h"
#include "uikit/painter.h"
#include "uikit/state.h"

namespace uik
{
namespace detail {

size_t BaseWidget::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mStyle);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mSize);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}
void BaseWidget::IntoJson(data::Writer& data) const
{
    data.Write("id",       mId);
    data.Write("name",     mName);
    data.Write("style",    mStyle);
    data.Write("position", mPosition);
    data.Write("size",     mSize);
    data.Write("flags",    mFlags);
}

bool BaseWidget::FromJson(const data::Reader& data)
{
    if (!data.Read("id",       &mId) ||
        !data.Read("name",     &mName) ||
        !data.Read("style",    &mStyle) ||
        !data.Read("position", &mPosition) ||
        !data.Read("size",     &mSize) ||
        !data.Read("flags",    &mFlags))
        return false;
    return true;
}

void FormModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.enabled = paint.enabled;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.time    = paint.time;
    p.clip    = paint.clip;
    p.rect    = paint.rect;
    p.pressed = false;
    p.klass   = "form";
    ps.painter->DrawWidgetBackground(ps.widgetId, p);
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

std::size_t ProgressBarModel::GetHash(size_t hash) const
{
    if (mValue)
        hash = base::hash_combine(hash, mValue.value());
    hash  =base::hash_combine(hash, mValue.has_value());
    hash = base::hash_combine(hash, mText);
    return hash;
}
void ProgressBarModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.enabled = paint.enabled;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.time    = paint.time;
    p.clip    = paint.clip;
    p.rect    = paint.rect;
    p.pressed = false;
    p.klass   = "progress-bar";
    ps.painter->DrawWidgetBackground(ps.widgetId, p);
    p.moused = false;
    p.pressed = false;
    ps.painter->DrawProgressBar(ps.widgetId, p, mValue);

    auto text = mText;
    if (mValue.has_value())
    {
        const float val = mValue.value();
        const int percent = 100 * val;
        text = base::FormatString(mText, percent);
    }
    ps.painter->DrawStaticText(ps.widgetId, p, text, 1.0f);

    p.moused = paint.moused;
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void ProgressBarModel::IntoJson(data::Writer& data) const
{
    if (mValue.has_value())
        data.Write("value", mValue.value());
    data.Write("text", mText);
}
bool ProgressBarModel::FromJson(const data::Reader& data)
{
    float value;
    if (data.Read("value", &value))
        mValue = value;
    return data.Read("text", &mText);
}

std::size_t SliderModel::GetHash(size_t hash) const
{
    return base::hash_combine(hash, mValue);
}
void SliderModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.enabled = paint.enabled;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.time    = paint.time;
    p.clip    = paint.clip;
    p.rect    = paint.rect;
    p.pressed = false;
    p.klass   = "slider";
    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    FRect slider;
    FRect knob;
    ComputeLayout(paint.rect, &slider, &knob);
    p.pressed = ps.state->GetValue(ps.widgetId + "/slider-down", false);
    p.moused  = ps.state->GetValue(ps.widgetId + "/slider-under-mouse", false);
    ps.painter->DrawSlider(ps.widgetId, p, knob);

    p.pressed = false;
    p.moused  = false;
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}
void SliderModel::IntoJson(data::Writer& data) const
{
    data.Write("value", mValue);
}
bool SliderModel::FromJson(const data::Reader& data)
{
    return data.Read("value", &mValue);
}
WidgetAction SliderModel::MouseEnter(const MouseStruct&)
{
    return WidgetAction {};
}
WidgetAction SliderModel::MousePress(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect slider, knob;
    ComputeLayout(mouse.widget_window_rect, &slider, &knob);
    ms.state->SetValue(ms.widgetId + "/slider-down", knob.TestPoint(mouse.window_mouse_pos));
    ms.state->SetValue(ms.widgetId + "/mouse-pos", mouse.widget_mouse_pos);
    return WidgetAction {};
}
WidgetAction SliderModel::MouseMove(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect slider, knob;
    ComputeLayout(mouse.widget_window_rect, &slider, &knob);
    ms.state->SetValue(ms.widgetId + "/slider-under-mouse", knob.TestPoint(mouse.window_mouse_pos));

    const auto slider_down = ms.state->GetValue(ms.widgetId + "/slider-down", false);
    if (!slider_down)
        return WidgetAction {};

    const auto slider_distance = slider.GetWidth() - knob.GetWidth();
    const auto& mouse_before = ms.state->GetValue(ms.widgetId + "/mouse-pos", mouse.widget_mouse_pos);
    const auto& mouse_delta  = mouse.widget_mouse_pos - mouse_before;
    const auto delta = mouse_delta.GetX();
    const auto dx = delta / slider_distance;
    mValue = math::clamp(0.0f, 1.0f, mValue + dx);

    ms.state->SetValue(ms.widgetId + "/mouse-pos", mouse.widget_mouse_pos);

    WidgetAction action;
    action.type  = WidgetActionType::ValueChanged;
    action.value = mValue;
    return action;
}
WidgetAction SliderModel::MouseRelease(const MouseEvent& mouse, const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/slider-down", false);

    return WidgetAction {};
}
WidgetAction SliderModel::MouseLeave(const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/slider-down", false);
    ms.state->SetValue(ms.widgetId + "/slider-under-mouse", false);
    return WidgetAction {};
}

void SliderModel::ComputeLayout(const FRect& rect, FRect* slider, FRect* knob) const
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto min_side = std::min(width, height);
    const auto knob_size = min_side;

    const auto slide_distance = width - knob_size;
    const auto slide_pos = math::clamp(0.0f, 1.0f, mValue);

    knob->SetWidth(min_side);
    knob->SetHeight(min_side);
    knob->Move(rect.GetPosition());
    knob->Translate(slide_distance * slide_pos, 0.0f);
    *slider = rect;
}

SpinBoxModel::SpinBoxModel()
{
    mMinVal = std::numeric_limits<int>::min();
    mMaxVal = std::numeric_limits<int>::max();
}

std::size_t SpinBoxModel::GetHash(size_t hash) const
{
    hash = base::hash_combine(hash, mValue);
    hash = base::hash_combine(hash, mMinVal);
    hash = base::hash_combine(hash, mMaxVal);
    return hash;
}

void SpinBoxModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.enabled = paint.enabled;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.time    = paint.time;
    p.clip    = paint.clip;
    p.rect    = paint.rect;
    p.pressed = false;
    p.klass   = "spinbox";

    FRect edt, btn_inc, btn_dec;
    ComputeBoxes(paint.rect, &btn_inc, &btn_dec, &edt);
    // check against minimum size. (see below for the size of the edit text area)
    if (edt.GetWidth() < 4.0f || edt.GetHeight() < 4.0f)
        return;

    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    p.rect = edt;
    ps.painter->DrawTextEditBox(ps.widgetId, p);

    Painter::EditableText text;
    text.text = std::to_string(mValue);
    edt.Grow(-4, -4);
    edt.Translate(2, 2);
    p.rect = edt;
    ps.painter->DrawEditableText(ps.widgetId, p, text);

    p.rect    = btn_inc;
    p.moused  = ps.state->GetValue(ps.widgetId + "/btn-inc-mouse-over", false);
    p.pressed = ps.state->GetValue(ps.widgetId + "/btn-inc-pressed", false);
    p.enabled = paint.enabled && mValue < mMaxVal;
    ps.painter->DrawButton(ps.widgetId, p, Painter::ButtonIcon::ArrowUp);

    p.rect    = btn_dec;
    p.moused  = ps.state->GetValue(ps.widgetId + "/btn-dec-mouse-over", false);
    p.pressed = ps.state->GetValue(ps.widgetId + "/btn-dec-pressed", false);
    p.enabled = paint.enabled && mValue > mMinVal;
    ps.painter->DrawButton(ps.widgetId, p, Painter::ButtonIcon::ArrowDown);

    p.rect    = paint.rect;
    p.moused  = paint.moused;
    p.enabled = paint.enabled;
    p.pressed = false;
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}
void SpinBoxModel::IntoJson(data::Writer& data) const
{
    data.Write("value", mValue);
    data.Write("min", mMinVal);
    data.Write("max", mMaxVal);
}
bool SpinBoxModel::FromJson(const data::Reader& data)
{
    if (!data.Read("value", &mValue) ||
        !data.Read("min", &mMinVal) ||
        !data.Read("max", &mMaxVal))
        return false;
    return true;
}

WidgetAction SpinBoxModel::PollAction(const PollStruct& poll)
{
    // if the mouse button is keeping either the increment
    // or the decrement button down then generate continuous
    // value update events.

    WidgetAction ret;
    float time = poll.state->GetValue(poll.widgetId + "/btn-interval", 0.3f);
    time -= poll.dt;
    if (time <= 0.0f)
    {
        ret = UpdateValue(poll.widgetId, *poll.state);
        // run a little bit faster next time.
        time = math::clamp(0.01f, 0.3f, time * 0.8f);
    }
    poll.state->SetValue(poll.widgetId + "/btn-interval", time);
    return ret;
}

WidgetAction SpinBoxModel::MouseEnter(const MouseStruct&)
{ return WidgetAction {}; }

WidgetAction SpinBoxModel::MousePress(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect btn_inc, btn_dec;
    ComputeBoxes(mouse.widget_window_rect, &btn_inc, &btn_dec);
    ms.state->SetValue(ms.widgetId + "/btn-inc-pressed", btn_inc.TestPoint(mouse.window_mouse_pos));
    ms.state->SetValue(ms.widgetId + "/btn-dec-pressed", btn_dec.TestPoint(mouse.window_mouse_pos));
    ms.state->SetValue(ms.widgetId + "/btn-interval", 0.3f);
    return UpdateValue(ms.widgetId, *ms.state);
}

WidgetAction SpinBoxModel::MouseMove(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect btn_inc, btn_dec;
    ComputeBoxes(mouse.widget_window_rect, &btn_inc, &btn_dec);
    const auto btn_inc_under_mouse = btn_inc.TestPoint(mouse.window_mouse_pos);
    const auto btn_dec_under_mouse = btn_dec.TestPoint(mouse.window_mouse_pos);
    ms.state->SetValue(ms.widgetId + "/btn-inc-mouse-over", btn_inc_under_mouse);
    ms.state->SetValue(ms.widgetId + "/btn-dec-mouse-over", btn_dec_under_mouse);
    if (!btn_inc_under_mouse)
        ms.state->SetValue(ms.widgetId + "/btn-inc-pressed", false);
    if (!btn_dec_under_mouse)
        ms.state->SetValue(ms.widgetId + "/btn-dec-pressed", false);
    return WidgetAction {};
}
WidgetAction SpinBoxModel::MouseRelease(const MouseEvent& mouse, const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/btn-inc-pressed", false);
    ms.state->SetValue(ms.widgetId + "/btn-dec-pressed", false);
    return WidgetAction {};
}

WidgetAction SpinBoxModel::MouseLeave(const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/btn-inc-mouse-over", false);
    ms.state->SetValue(ms.widgetId + "/btn-dec-mouse-over", false);
    ms.state->SetValue(ms.widgetId + "/btn-inc-pressed", false);
    ms.state->SetValue(ms.widgetId + "/btn-dec-pressed", false);
    return WidgetAction {};
}

void SpinBoxModel::ComputeBoxes(const FRect& rect, FRect* btn_inc, FRect* btn_dec, FRect* edit) const
{
    const auto widget_width  = rect.GetWidth();
    const auto widget_height = rect.GetHeight();
    const auto edit_width    = widget_width * 0.8;
    const auto edit_height   = widget_height;
    const auto button_width  = widget_width - edit_width;
    const auto button_height = widget_height * 0.5;

    FRect edt;
    edt.SetWidth(edit_width);
    edt.SetHeight(edit_height);
    edt.Move(rect.GetPosition());
    if (edit) *edit = edt;

    FRect btn;
    btn.SetWidth(button_width);
    btn.SetHeight(button_height);
    btn.Move(rect.GetPosition());
    btn.Translate(edit_width, 0);
    *btn_inc = btn;

    btn.Translate(0, button_height);
    *btn_dec = btn;
}

WidgetAction SpinBoxModel::UpdateValue(const std::string& id, State& state)
{
    const auto inc_down = state.GetValue(id + "/btn-inc-pressed", false);
    const auto dec_down = state.GetValue(id + "/btn-dec-pressed", false);
    auto value = mValue;
    if (inc_down)
        value = math::clamp(mMinVal, mMaxVal, value + 1);
    else if (dec_down)
        value = math::clamp(mMinVal, mMaxVal, value - 1);

    if (mValue != value)
    {
        mValue = value;
        WidgetAction action;
        action.type = WidgetActionType::ValueChanged;
        action.value = value;
        return action;
    }
    return WidgetAction {};
}


std::size_t LabelModel::GetHash(size_t hash) const
{
    hash = base::hash_combine(hash, mText);
    hash = base::hash_combine(hash, mLineHeight);
    return hash;
}
void LabelModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.enabled = paint.enabled;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.time    = paint.time;
    p.rect    = paint.rect;
    p.clip    = paint.clip;
    p.pressed = false;
    p.klass   = "label";
    ps.painter->DrawWidgetBackground(ps.widgetId, p);
    ps.painter->DrawStaticText(ps.widgetId, p, mText, mLineHeight);
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void LabelModel::IntoJson(data::Writer& data) const
{
    data.Write("text", mText);
    data.Write("line_height", mLineHeight);
}
bool LabelModel::FromJson(const data::Reader& data)
{
    if (!data.Read("text", &mText) ||
        !data.Read("line_height", &mLineHeight))
        return false;
    return true;
}

size_t PushButtonModel::GetHash(size_t hash) const
{
    return base::hash_combine(hash, mText);
}

void PushButtonModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.enabled = paint.enabled;
    p.rect    = paint.rect;
    p.time    = paint.time;
    p.clip    = paint.clip;
    p.pressed = ps.state->GetValue(ps.widgetId + "/pressed", false);
    p.klass   = "push-button";
    ps.painter->DrawWidgetBackground(ps.widgetId, p);
    ps.painter->DrawButton(ps.widgetId, p, Painter::ButtonIcon::None);
    ps.painter->DrawStaticText(ps.widgetId, p, mText, 1.0f);
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void PushButtonModel::IntoJson(data::Writer& data)  const
{
    data.Write("text", mText);
}

bool PushButtonModel::FromJson(const data::Reader& data)
{
    if (!data.Read("text", &mText))
        return false;
    return true;
}

WidgetAction PushButtonModel::MousePress(const MouseEvent& mouse, const MouseStruct& ms)
{
    if (mouse.button == MouseButton::Left)
        ms.state->SetValue(ms.widgetId + "/pressed", true);

    return WidgetAction{};
}

WidgetAction PushButtonModel::MouseRelease(const MouseEvent& mouse, const MouseStruct& ms)
{
    const bool pressed = ms.state->GetValue(ms.widgetId + "/pressed", false);
    if (mouse.button == MouseButton::Left && pressed)
    {
        ms.state->SetValue(ms.widgetId + "/pressed", false);
        WidgetAction action;
        action.type = WidgetActionType::ButtonPress;
        return action;
    }
    return WidgetAction {};
}

WidgetAction PushButtonModel::MouseLeave(const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/pressed", false);
    return WidgetAction {};
}

size_t CheckBoxModel::GetHash(size_t hash) const
{
    hash = base::hash_combine(hash, mText);
    hash = base::hash_combine(hash, mChecked);
    hash = base::hash_combine(hash, mCheck);
    return hash;
}

void CheckBoxModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.enabled = paint.enabled;
    p.rect    = paint.rect;
    p.clip    = paint.clip;
    p.time    = paint.time;
    p.pressed = false;
    p.klass   = "checkbox";
    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    FRect text, check;
    ComputeLayout(paint.rect, &text, &check);

    p.rect   = check;
    p.moused = ps.state->GetValue(ps.widgetId + "/mouse-over-check", false);
    ps.painter->DrawCheckBox(ps.widgetId, p, mChecked);

    p.rect = text;
    ps.painter->DrawStaticText(ps.widgetId, p, mText, 1.0f);

    p.rect = paint.rect;
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void CheckBoxModel::IntoJson(data::Writer& data) const
{
    data.Write("text", mText);
    data.Write("checked", mChecked);
    data.Write("check", mCheck);
}
bool CheckBoxModel::FromJson(const data::Reader& data)
{
    if (!data.Read("text", &mText) ||
        !data.Read("checked", &mChecked) ||
        !data.Read("check", &mCheck))
        return false;
    return true;
}

WidgetAction CheckBoxModel::MouseMove(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect text, check;
    ComputeLayout(mouse.widget_window_rect, &text, &check);
    ms.state->SetValue(ms.widgetId + "/mouse-over-check", check.TestPoint(mouse.window_mouse_pos));
    return WidgetAction{};
}

WidgetAction CheckBoxModel::MouseRelease(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect text, check;
    ComputeLayout(mouse.widget_window_rect, &text, &check);
    if (!check.TestPoint(mouse.window_mouse_pos))
        return WidgetAction {};

    mChecked = !mChecked;
    WidgetAction action;
    action.type  = WidgetActionType::ValueChanged;
    action.value = mChecked;
    return action;
}

WidgetAction CheckBoxModel::MouseLeave(const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/mouse-over-check", false);
    return WidgetAction {};
}

void CheckBoxModel::ComputeLayout(const FRect& rect, FRect* text, FRect* check) const
{
    const auto width = rect.GetWidth();
    const auto height = rect.GetHeight();
    if (width > height && mCheck == Check::Right)
    {
        const auto check_size = height;
        const auto check_x = width - check_size;
        check->SetWidth(check_size);
        check->SetHeight(check_size);
        check->Move(rect.GetPosition());
        check->Translate(check_x, 0.0f);

        text->SetWidth(width-check_size);
        text->SetHeight(height);
        text->Move(rect.GetPosition());
    }
    else if (width > height && mCheck == Check::Left)
    {
        const auto check_size = height;
        const auto check_x = 0.0f;
        const auto text_x  = check_size;
        check->SetWidth(check_size);
        check->SetHeight(check_size);
        check->Move(rect.GetPosition());

        text->SetWidth(width-check_size);
        text->SetHeight(height);
        text->Move(rect.GetPosition());
        text->Translate(text_x, 0.0f);
    }
    else
    {
        *check = rect;
    }
}

std::size_t RadioButtonModel::GetHash(size_t hash) const
{
    hash = base::hash_combine(hash, mText);
    hash = base::hash_combine(hash, mSelected);
    hash = base::hash_combine(hash, mCheck);
    return hash;
}

void RadioButtonModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.enabled = paint.enabled;
    p.rect    = paint.rect;
    p.clip    = paint.clip;
    p.time    = paint.time;
    p.pressed = false;
    p.klass   = "radiobutton";
    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    FRect text, check;
    ComputeLayout(paint.rect, &text, &check);

    p.rect   = check;
    p.moused = ps.state->GetValue(ps.widgetId + "/mouse-over-check", false);
    ps.painter->DrawRadioButton(ps.widgetId, p, mSelected);

    p.rect = text;
    ps.painter->DrawStaticText(ps.widgetId, p, mText, 1.0f);

    p.rect = paint.rect;
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void RadioButtonModel::IntoJson(data::Writer& data) const
{
    data.Write("text", mText);
    data.Write("selected", mSelected);
    data.Write("check", mCheck);
}
bool RadioButtonModel::FromJson(const data::Reader& data)
{
    data.Read("text", &mText);
    data.Read("selected", &mSelected);
    data.Read("check", &mCheck);
    return true;
}

WidgetAction RadioButtonModel::PollAction(const PollStruct& poll)
{
    if (!mRequestSelection)
        return WidgetAction {};

    mRequestSelection = false;
    mSelected = true;

    WidgetAction action;
    action.type  = WidgetActionType::ValueChanged;
    action.value = true;
    return action;
}

WidgetAction RadioButtonModel::MouseMove(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect text, check;
    ComputeLayout(mouse.widget_window_rect, &text, &check);
    ms.state->SetValue(ms.widgetId + "/mouse-over-check", check.TestPoint(mouse.window_mouse_pos));
    return WidgetAction{};
}
WidgetAction RadioButtonModel::MouseRelease(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect text, check;
    ComputeLayout(mouse.widget_window_rect, &text, &check);
    if (!check.TestPoint(mouse.window_mouse_pos))
        return WidgetAction {};

    if (!mSelected)
        mRequestSelection = true;

    return WidgetAction {};
}
WidgetAction RadioButtonModel::MouseLeave(const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/mouse-over-check", false);
    return WidgetAction {};
}

void RadioButtonModel::ComputeLayout(const FRect& rect, FRect* text, FRect* check) const
{
    const auto width = rect.GetWidth();
    const auto height = rect.GetHeight();
    if (width > height && mCheck == Check::Right)
    {
        const auto check_size = height;
        const auto check_x = width - check_size;
        check->SetWidth(check_size);
        check->SetHeight(check_size);
        check->Move(rect.GetPosition());
        check->Translate(check_x, 0.0f);

        text->SetWidth(width-check_size);
        text->SetHeight(height);
        text->Move(rect.GetPosition());
    }
    else if (width > height && mCheck == Check::Left)
    {
        const auto check_size = height;
        const auto check_x = 0.0f;
        const auto text_x  = check_size;
        check->SetWidth(check_size);
        check->SetHeight(check_size);
        check->Move(rect.GetPosition());

        text->SetWidth(width-check_size);
        text->SetHeight(height);
        text->Move(rect.GetPosition());
        text->Translate(text_x, 0.0f);
    }
    else
    {
        *check = rect;
    }
}

std::size_t GroupBoxModel::GetHash(size_t hash) const
{
    hash = base::hash_combine(hash, mText);
    return hash;
}
void GroupBoxModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.focused = paint.focused;
    p.pressed = paint.focused;
    p.moused  = paint.moused;
    p.enabled = paint.enabled;
    p.rect    = paint.rect;
    p.clip    = paint.clip;
    p.time    = paint.time;
    p.pressed = false;
    p.klass   = "groupbox";
    ps.painter->DrawWidgetBackground(ps.widgetId, p);
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}
void GroupBoxModel::IntoJson(data::Writer& data) const
{
    data.Write("text", mText);
}
bool GroupBoxModel::FromJson(const data::Reader& data)
{
    if (!data.Read("text", &mText))
        return false;

    return true;
}

} // namespace detail

std::unique_ptr<Widget> CreateWidget(uik::Widget::Type type)
{
    if (type == Widget::Type::Form)
        return std::make_unique<uik::Form>();
    if (type == Widget::Type::Label)
        return std::make_unique<uik::Label>();
    else if (type == Widget::Type::PushButton)
        return std::make_unique<uik::PushButton>();
    else if (type == Widget::Type::CheckBox)
        return std::make_unique<uik::CheckBox>();
    else if (type == Widget::Type::GroupBox)
        return std::make_unique<uik::GroupBox>();
    else if (type == Widget::Type::SpinBox)
        return std::make_unique<uik::SpinBox>();
    else if (type == Widget::Type::Slider)
        return std::make_unique<uik::Slider>();
    else if (type == Widget::Type::ProgressBar)
        return std::make_unique<uik::ProgressBar>();
    else if (type == Widget::Type::RadioButton)
        return std::make_unique<uik::RadioButton>();
    else BUG("Unhandled widget type.");
    return nullptr;
}

} // namespace
