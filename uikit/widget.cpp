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

#include "config.h"

#include "base/assert.h"
#include "base/utility.h"
#include "uikit/widget.h"
#include "uikit/painter.h"

namespace uik
{
namespace detail {

size_t WidgetBase::GetHash(size_t hash ) const
{
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mStyle);
    hash = base::hash_combine(hash, mPosition.GetHash());
    hash = base::hash_combine(hash, mSize.GetHash());
    hash = base::hash_combine(hash, mFlags.value());
    return hash;
}
void WidgetBase::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "id",       mId);
    base::JsonWrite(json, "name",     mName);
    base::JsonWrite(json, "style",    mStyle);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "size",     mSize);
    base::JsonWrite(json, "flags",    mFlags);
}

bool WidgetBase::FromJson(const nlohmann::json& json)
{
    if (!base::JsonReadSafe(json, "id",       &mId) ||
        !base::JsonReadSafe(json, "name",     &mName) ||
        !base::JsonReadSafe(json, "style",    &mStyle) ||
        !base::JsonReadSafe(json, "position", &mPosition) ||
        !base::JsonReadSafe(json, "size",     &mSize) ||
        !base::JsonReadSafe(json, "flags",    &mFlags))
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
    ps.painter->DrawWidgetText(ps.widgetId, p, mText, mLineHeight);
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void LabelModel::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "text", mText);
    base::JsonWrite(json, "line_height", mLineHeight);
}
bool LabelModel::FromJson(const nlohmann::json& json)
{
    if (!base::JsonReadSafe(json, "text", &mText) ||
        !base::JsonReadSafe(json, "line_height", &mLineHeight))
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
    ps.painter->DrawWidgetText(ps.widgetId, p, mText, 1.0);
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void PushButtonModel::IntoJson(nlohmann::json& json)  const
{
    base::JsonWrite(json, "text", mText);
}

bool PushButtonModel::FromJson(const nlohmann::json& json)
{
    if (!base::JsonReadSafe(json, "text", &mText))
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
    return hash;
}

void CheckBoxModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    const auto width  = paint.rect.GetWidth();
    const auto height = paint.rect.GetHeight();
    const auto pos = paint.rect.GetPosition();

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

    if (width >= 30.0f && height >= 30.0f)
    {
        FRect box;
        box.Resize(30.0f , 30.0f);
        box.Move(pos);
        box.Translate(width - 30 , (height - 30.0) * 0.5);
        p.rect = box;
        ps.painter->DrawCheckBox(ps.widgetId , p , mChecked);

        FRect text;
        text.Resize(width - 30 , height);
        text.Move(pos);
        p.rect = text;
        ps.painter->DrawWidgetText(ps.widgetId , p , mText , 1.0f);
    }

    p.rect = paint.rect;
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void CheckBoxModel::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "text", mText);
    base::JsonWrite(json, "checked", mChecked);
}
bool CheckBoxModel::FromJson(const nlohmann::json& json)
{
    if (!base::JsonReadSafe(json, "text", &mText) ||
        !base::JsonReadSafe(json, "checked", &mChecked))
        return false;
    return true;
}

WidgetAction CheckBoxModel::MouseRelease(const MouseEvent& mouse, const MouseStruct& ms)
{
    const auto width  = mouse.widget_window_rect.GetWidth();
    const auto height = mouse.widget_window_rect.GetHeight();
    const auto& pos   = mouse.widget_window_rect.GetPosition();
    if (width >= 30.0f && height >= 30.0f)
    {
        FRect box;
        box.Resize(30.0f, 30.0f);
        box.Move(pos);
        box.Translate(width-30.0f, (height-30.0f)*0.5f);
        if (!box.TestPoint(mouse.window_mouse_pos))
            return WidgetAction{};

        mChecked = !mChecked;
        WidgetAction action;
        action.type  = WidgetActionType::ValueChanged;
        action.value = mChecked;
        return action;
    }
    return WidgetAction{};
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
void GroupBoxModel::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "text", mText);
}
bool GroupBoxModel::FromJson(const nlohmann::json& json)
{
    if (!base::JsonReadSafe(json, "text", &mText))
        return false;

    return true;
}

} // namespace detail
} // namespace
