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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <set>

#include "base/assert.h"
#include "base/hash.h"
#include "base/format.h"
#include "base/json.h"
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
    hash = base::hash_combine(hash, mStyleString);
    hash = base::hash_combine(hash, mAnimationString);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mSize);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mTabIndex);
    if (mStyleProperties)
    {
        std::set<std::string> keys;
        for (const auto& [key, _] : *mStyleProperties)
            keys.insert(key);

        for (const auto& key : keys)
        {
            const auto* val = base::SafeFind(*mStyleProperties, key);
            hash = base::hash_combine(hash, key);
            hash = base::hash_combine(hash, *val);
        }
    }
    if (mStyleMaterials)
    {
        std::set<std::string> keys;
        for (const auto& [key, _] : *mStyleMaterials)
            keys.insert(key);

        for (const auto& key : keys)
        {
            const auto* val = base::SafeFind(*mStyleMaterials, key);
            hash = base::hash_combine(hash, key);
            hash = base::hash_combine(hash, *val);
        }
    }
    return hash;
}
void BaseWidget::IntoJson(data::Writer& data) const
{
    data.Write("id",        mId);
    data.Write("name",      mName);
    data.Write("style",     mStyleString);
    data.Write("animation", mAnimationString);
    data.Write("position",  mPosition);
    data.Write("size",      mSize);
    data.Write("flags",     mFlags);
    data.Write("tab_index", mTabIndex);
    if (mStyleProperties)
    {
        for (const auto& [key, val] : *mStyleProperties)
        {
            auto chunk = data.NewWriteChunk();
            chunk->Write("key", key);
            chunk->Write("val", val);
            data.AppendChunk("style_properties", std::move(chunk));
        }
    }
    if (mStyleMaterials)
    {
        for (const auto& [key, val] : *mStyleMaterials)
        {
            auto chunk = data.NewWriteChunk();
            chunk->Write("key", key);
            chunk->Write("val", val);
            data.AppendChunk("style_materials", std::move(chunk));
        }
    }
}

bool BaseWidget::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",        &mId);
    ok &= data.Read("name",      &mName);
    ok &= data.Read("style",     &mStyleString);
    if (data.HasValue("animation"))
        ok &= data.Read("animation", &mAnimationString);
    ok &= data.Read("position",  &mPosition);
    ok &= data.Read("size",      &mSize);
    ok &= data.Read("flags",     &mFlags);
    ok &= data.Read("tab_index", &mTabIndex);
    if (data.HasArray("style_properties"))
    {
        StylePropertyMap props;
        for (unsigned i=0; i<data.GetNumItems("style_properties"); ++i)
        {
            const auto& chunk = data.GetReadChunk("style_properties", i);
            std::string key;
            StyleProperty val;
            ok &= chunk->Read("key", &key);
            ok &= chunk->Read("val", &val);
            props[key] = std::move(val);
        }
        mStyleProperties = std::move(props);
    }
    if (data.HasArray("style_materials"))
    {
        StyleMaterialMap materials;
        for (unsigned i=0; i<data.GetNumItems("style_materials"); ++i)
        {
            const auto& chunk = data.GetReadChunk("style_materials", i);
            std::string key;
            std::string val;
            ok &= chunk->Read("key", &key);
            ok &= chunk->Read("val", &val);
            materials[key] = std::move(val);
        }
        mStyleMaterials = std::move(materials);
    }
    return ok;
}

void BaseWidget::SetStyleProperty(const std::string& key, const StyleProperty& prop)
{
    auto map = mStyleProperties.value_or(StylePropertyMap());
    map[key] = std::move(prop);
    mStyleProperties = std::move(map);
}
const StyleProperty* BaseWidget::GetStyleProperty(const std::string& key) const
{
    if (!mStyleProperties)
        return nullptr;
    const auto& map = mStyleProperties.value();
    const auto* val = base::SafeFind(map, key);
    return val;
}

void BaseWidget::DeleteStyleProperty(const std::string& key)
{
    if (!mStyleProperties)
        return;
    auto& map = mStyleProperties.value();
    map.erase(key);
}

void BaseWidget::SetStyleMaterial(const std::string& key, const std::string& material)
{
    auto map = mStyleMaterials.value_or(StyleMaterialMap());
    map[key] = material;
    mStyleMaterials = std::move(map);
}

const std::string* BaseWidget::GetStyleMaterial(const std::string& key) const
{
    if (!mStyleMaterials)
        return nullptr;

    const auto& map = mStyleMaterials.value();
    const auto* val = base::SafeFind(map, key);
    return val;
}
void BaseWidget::DeleteStyleMaterial(const std::string& key)
{
    if (!mStyleMaterials)
        return;
    auto& map = mStyleMaterials.value();
    map.erase(key);
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;
    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    FRect slider;
    FRect knob;
    ComputeLayout(paint.rect, &slider, &knob);
    p.pressed = ps.state->GetValue(ps.widgetId + "/slider-knob-down", false);
    p.moused  = ps.state->GetValue(ps.widgetId + "/slider-knob-under-mouse", false);
    ps.painter->DrawSlider(ps.widgetId, p, knob);

    // drawing the focus rect has been baked in the DrawSlider since that
    // is simply the easiest and most pragmatic way to get a nicer
    // rendering and composition of widget components.

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
    ms.state->SetValue(ms.widgetId + "/slider-knob-down", knob.TestPoint(mouse.window_mouse_pos));
    ms.state->SetValue(ms.widgetId + "/mouse-pos", mouse.widget_mouse_pos);
    return WidgetAction {
        WidgetActionType::MouseGrabBegin
    };
}
WidgetAction SliderModel::MouseMove(const MouseEvent& mouse, const MouseStruct& ms)
{
    FRect slider, knob;
    ComputeLayout(mouse.widget_window_rect, &slider, &knob);
    ms.state->SetValue(ms.widgetId + "/slider-knob-under-mouse", knob.TestPoint(mouse.window_mouse_pos));

    const auto slider_down = ms.state->GetValue(ms.widgetId + "/slider-knob-down", false);
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
    action.type  = WidgetActionType::ValueChange;
    action.value = mValue;
    return action;
}
WidgetAction SliderModel::MouseRelease(const MouseEvent& mouse, const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/slider-knob-down", false);

    return WidgetAction {
        WidgetActionType::MouseGrabEnd
    };
}
WidgetAction SliderModel::MouseLeave(const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/slider-knob-under-mouse", false);
    return WidgetAction {};
}
WidgetAction SliderModel::KeyDown(const KeyEvent& key, const KeyStruct& ks)
{
    // todo: maybe use non-constant delta step when moving slider by keys
    if (key.key == VirtualKey::MoveUp || key.key == VirtualKey::MoveRight)
    {
        if (mValue < 1.0f)
        {
            mValue = math::clamp(0.0f, 1.0f, mValue + 0.05f);
            WidgetAction action;
            action.type = WidgetActionType::ValueChange;
            action.value = mValue;
            return action;
        }
    }
    else if (key.key == VirtualKey::MoveDown || key.key == VirtualKey::MoveLeft)
    {
        if (mValue > 0.0f)
        {
            mValue = math::clamp(0.0f, 1.0f, mValue - 0.05f);
            WidgetAction action;
            action.type = WidgetActionType::ValueChange;
            action.value = mValue;
            return action;
        }
    }
    return WidgetAction {};
}

WidgetAction SliderModel::KeyUp(const KeyEvent& key, const KeyStruct& ks)
{
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;

    FRect edt, btn_inc, btn_dec;
    ComputeBoxes(paint.rect, &btn_inc, &btn_dec, &edt);
    // check against minimum size. (see below for the size of the edit text area)
    if (edt.GetWidth() < 4.0f || edt.GetHeight() < 4.0f)
        return;

    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    p.rect = edt;
    ps.painter->DrawTextEditBox(ps.widgetId, p);
    if (p.focused)
        ps.painter->DrawWidgetFocusRect(ps.widgetId, p);

    Painter::EditableText text;
    text.text = std::to_string(mValue);
    edt.Grow(-4, -4);
    edt.Translate(2, 2);
    p.rect = edt;
    ps.painter->DrawEditableText(ps.widgetId, p, text);

    p.rect    = btn_inc;
    p.moused  = ps.state->GetValue(ps.widgetId + "/btn-inc-mouse-over", false);
    p.pressed = ps.state->GetValue(ps.widgetId + "/btn-inc-pressed", false) ||
                ps.state->GetValue(ps.widgetId + "/key-inc-pressed", false);
    p.enabled = paint.enabled && mValue < mMaxVal;
    ps.painter->DrawButton(ps.widgetId, p, Painter::ButtonIcon::ArrowUp);

    p.rect    = btn_dec;
    p.moused  = ps.state->GetValue(ps.widgetId + "/btn-dec-mouse-over", false);
    p.pressed = ps.state->GetValue(ps.widgetId + "/btn-dec-pressed", false) ||
                ps.state->GetValue(ps.widgetId + "/key-dec-pressed", false);
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
    bool ok = true;
    ok &= data.Read("value", &mValue);
    ok &= data.Read("min", &mMinVal);
    ok &= data.Read("max", &mMaxVal);
    return ok;
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

WidgetAction SpinBoxModel::KeyDown(const KeyEvent& key, const KeyStruct& ks)
{
    auto last_value = mValue;

    if (key.key == VirtualKey::MoveUp)
    {
        mValue = math::clamp(mMinVal, mMaxVal, mValue+1);
        ks.state->SetValue(ks.widgetId + "/key-inc-pressed", true);
    }
    else if (key.key == VirtualKey::MoveDown)
    {
        mValue = math::clamp(mMinVal, mMaxVal, mValue-1);
        ks.state->SetValue(ks.widgetId + "/key-dec-pressed", true);
    }
    if (last_value != mValue)
    {
        WidgetAction action;
        action.type = WidgetActionType::ValueChange;
        action.value = mValue;
        return action;
    }
    return WidgetAction {};
}
WidgetAction SpinBoxModel::KeyUp(const KeyEvent& key, const KeyStruct& ks)
{
    if (key.key == VirtualKey::MoveUp)
    {
        ks.state->SetValue(ks.widgetId + "/key-inc-pressed", false);
    }
    else if (key.key == VirtualKey::MoveDown)
    {
        ks.state->SetValue(ks.widgetId + "/key-dec-pressed", false);
    }

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

WidgetAction SpinBoxModel::UpdateValue(const std::string& id, TransientState& state)
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
        action.type = WidgetActionType::ValueChange;
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;
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
    bool ok = true;
    ok &= data.Read("text", &mText);
    ok &= data.Read("line_height", &mLineHeight);
    return ok;
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;

    ps.painter->DrawWidgetBackground(ps.widgetId, p);
    ps.painter->DrawButton(ps.widgetId, p, Painter::ButtonIcon::None);
    ps.painter->DrawStaticText(ps.widgetId, p, mText, 1.0f);
    if (p.focused)
        ps.painter->DrawWidgetFocusRect(ps.widgetId, p);

    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void PushButtonModel::IntoJson(data::Writer& data)  const
{
    data.Write("text", mText);
}

bool PushButtonModel::FromJson(const data::Reader& data)
{
    return data.Read("text", &mText);

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

WidgetAction PushButtonModel::KeyDown(const KeyEvent& key, const KeyStruct& ks)
{
    if (key.key == VirtualKey::Select)
    {
        ks.state->SetValue(ks.widgetId + "/pressed", true);
    }
    return WidgetAction {};
}
WidgetAction PushButtonModel::KeyUp(const KeyEvent& key, const KeyStruct& ks)
{
    const bool pressed = ks.state->GetValue(ks.widgetId + "/pressed", false);
    if (key.key == VirtualKey::Select && pressed)
    {
        ks.state->SetValue(ks.widgetId + "/pressed", false);
        WidgetAction action;
        action.type = WidgetActionType::ButtonPress;
        return action;
    }
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;
    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    FRect text, check;
    ComputeLayout(paint.rect, &text, &check);

    p.rect   = check;
    p.moused = ps.state->GetValue(ps.widgetId + "/mouse-over-check", false);
    ps.painter->DrawCheckBox(ps.widgetId, p, mChecked);

    p.rect = text;
    ps.painter->DrawStaticText(ps.widgetId, p, mText, 1.0f);
    if (paint.focused)
        ps.painter->DrawWidgetFocusRect(ps.widgetId, p);

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
    bool ok = true;
    ok &= data.Read("text", &mText);
    ok &= data.Read("checked", &mChecked);
    ok &= data.Read("check", &mCheck);
    return ok;
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
    action.type  = WidgetActionType::ValueChange;
    action.value = mChecked;
    return action;
}

WidgetAction CheckBoxModel::MouseLeave(const MouseStruct& ms)
{
    ms.state->SetValue(ms.widgetId + "/mouse-over-check", false);
    return WidgetAction {};
}

WidgetAction CheckBoxModel::KeyDown(const KeyEvent& key, const KeyStruct& ks)
{
    const auto state = mChecked;
    if (key.key == VirtualKey::Select)
    {
        mChecked = !mChecked;
    }
    if (state != mChecked)
    {
        WidgetAction action;
        action.type  = WidgetActionType::ValueChange;
        action.value = mChecked;
        return action;
    }
    return {};
}
WidgetAction CheckBoxModel::KeyUp(const KeyEvent& key, const KeyStruct& ks)
{
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

size_t ToggleBoxModel::GetHash(size_t hash) const
{
    hash = base::hash_combine(hash, mChecked);
    return hash;
}

void ToggleBoxModel::Update(const UpdateStruct& update) const
{
    float position = 0.0f;
    float velocity = 0.0f;
    if (!update.state->GetValue(update.widgetId + "/velocity", &velocity) ||
        !update.state->GetValue(update.widgetId + "/position", &position))
        return;

    position += velocity * update.dt;
    update.state->SetValue(update.widgetId + "/position", position);
}

void ToggleBoxModel::Paint(const PaintEvent& paint, const PaintStruct& ps) const
{
    Painter::PaintStruct p;
    p.focused = paint.focused;
    p.moused  = paint.moused;
    p.enabled = paint.enabled;
    p.rect    = paint.rect;
    p.clip    = paint.clip;
    p.time    = paint.time;
    p.pressed = false;
    p.klass   = "togglebox";
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;
    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    float knob_pos = mChecked ? 1.0f : 0.0f;
    knob_pos = ps.state->GetValue(ps.widgetId + "/position", knob_pos);
    knob_pos = math::clamp(0.0f, 1.0f, knob_pos);

    const auto widget_width  = paint.rect.GetWidth();
    const auto widget_height = paint.rect.GetHeight();
    if (widget_width > widget_height)
    {
        const auto knob_height = widget_height;
        const auto knob_width  = widget_height;
        const auto travel_distance = widget_width - knob_width;

        FRect knob;
        knob.Resize(knob_width, knob_height);
        knob.Translate(paint.rect.GetPosition());
        knob.Translate(knob_pos*travel_distance, 0.0f);
        ps.painter->DrawToggle(ps.widgetId, p, knob, mChecked);
    }

    //if (p.focused)
    //    ps.painter->DrawWidgetFocusRect(ps.widgetId, p);

    p.rect = paint.rect;
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}

void ToggleBoxModel::IntoJson(data::Writer& data) const
{
    data.Write("checked", mChecked);
}
bool ToggleBoxModel::FromJson(const data::Reader& data)
{
    return data.Read("checked", &mChecked);
}

WidgetAction ToggleBoxModel::PollAction(const PollStruct& poll)
{
    float position = 0.0f;
    float velocity = 0.0f;
    if (!poll.state->GetValue(poll.widgetId + "/velocity", &velocity) ||
        !poll.state->GetValue(poll.widgetId + "/position", &position))
        return WidgetAction {};

    bool checked = mChecked;
    if (velocity < 0.0f && position <= 0.0f)
        checked = false;
    else if (velocity > 0.0f && position >= 1.0f)
        checked = true;

    if (checked != mChecked)
    {
        poll.state->DeleteValue(poll.widgetId + "/velocity");
        poll.state->DeleteValue(poll.widgetId + "/position");
        mChecked = checked;
        WidgetAction action;
        action.type  = WidgetActionType::ValueChange;
        action.value = checked;
        return action;
    }
    return WidgetAction {};
}

WidgetAction ToggleBoxModel::MouseMove(const MouseEvent& mouse, const MouseStruct&)
{
    return WidgetAction {};
}
WidgetAction ToggleBoxModel::MouseRelease(const MouseEvent& mouse, const MouseStruct& ms)
{
    if (ms.state->HasValue(ms.widgetId + "/position"))
        return WidgetAction {};

    ms.state->SetValue(ms.widgetId + "/position", mChecked ?  1.0f : 0.0f);
    ms.state->SetValue(ms.widgetId + "/velocity", mChecked ? -4.0f : 4.0f);
    return WidgetAction {};
}
WidgetAction ToggleBoxModel::MouseLeave(const MouseStruct&)
{
    return WidgetAction {};
}
WidgetAction ToggleBoxModel::KeyDown(const KeyEvent& key, const KeyStruct& ks)
{
    if (key.key == VirtualKey::Select)
    {
        if (ks.state->HasValue(ks.widgetId + "/position"))
            return WidgetAction {};

        ks.state->SetValue(ks.widgetId + "/position", mChecked ?  1.0f : 0.0f);
        ks.state->SetValue(ks.widgetId + "/velocity", mChecked ? -4.0f : 4.0f);
    }
    return WidgetAction {};
}
WidgetAction ToggleBoxModel::KeyUp(const KeyEvent& key, const KeyStruct& ks)
{
    return WidgetAction {};
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;
    ps.painter->DrawWidgetBackground(ps.widgetId, p);

    FRect text, check;
    ComputeLayout(paint.rect, &text, &check);

    p.rect   = check;
    p.moused = ps.state->GetValue(ps.widgetId + "/mouse-over-check", false);
    ps.painter->DrawRadioButton(ps.widgetId, p, mSelected);

    p.rect = text;
    ps.painter->DrawStaticText(ps.widgetId, p, mText, 1.0f);
    if (p.focused)
        ps.painter->DrawWidgetFocusRect(ps.widgetId, p);

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
    bool ok = true;
    ok &= data.Read("text", &mText);
    ok &= data.Read("selected", &mSelected);
    ok &= data.Read("check", &mCheck);
    return ok;
}

WidgetAction RadioButtonModel::PollAction(const PollStruct& poll)
{
    if (!mRequestSelection)
        return WidgetAction {};

    mRequestSelection = false;
    mSelected = true;

    WidgetAction action;
    action.type  = WidgetActionType::ValueChange;
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

WidgetAction RadioButtonModel::KeyDown(const KeyEvent& key, const KeyStruct& ks)
{
    if (key.key == VirtualKey::Select)
    {
        if (!mSelected)
            mRequestSelection = true;
    }
    return WidgetAction {};
}
WidgetAction RadioButtonModel::KeyUp(const KeyEvent& key, const KeyStruct& ks)
{
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
    p.style_properties = ps.style_properties;
    p.style_materials  = ps.style_materials;

    ps.painter->DrawWidgetBackground(ps.widgetId, p);
    ps.painter->DrawWidgetBorder(ps.widgetId, p);
}
void GroupBoxModel::IntoJson(data::Writer& data) const
{
    data.Write("text", mText);
}
bool GroupBoxModel::FromJson(const data::Reader& data)
{
    return data.Read("text", &mText);
}

} // namespace detail

void Widget::SetColor(const std::string& key, const Color4f& color)
{
    nlohmann::json json;
    base::JsonWrite(json, "type", "Color");
    base::JsonWrite(json, "color", color);
    const auto& style_string = json.dump();
    SetStyleMaterial(key, style_string);
}

void Widget::SetMaterial(const std::string& key, const std::string& material)
{
    nlohmann::json json;
    base::JsonWrite(json, "type", "Reference");
    base::JsonWrite(json, "material", material);
    const auto& style_string = json.dump();
    SetStyleMaterial(key, style_string);
}
void Widget::SetGradient(const std::string& key,
                         const Color4f& top_left,
                         const Color4f& top_right,
                         const Color4f& bottom_left,
                         const Color4f& bottom_right)
{
    nlohmann::json json;
    base::JsonWrite(json, "type", "Gradient");
    base::JsonWrite(json, "color0", top_left);
    base::JsonWrite(json, "color1", top_right);
    base::JsonWrite(json, "color2", bottom_left);
    base::JsonWrite(json, "color3", bottom_right);
    const auto& style_string = json.dump();
    SetStyleMaterial(key, style_string);
}

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
    else if (type == Widget::Type::ToggleBox)
        return std::make_unique<uik::ToggleBox>();
    else BUG("Unhandled widget type.");
    return nullptr;
}

// static
std::string Widget::GetWidgetClassName(uik::Widget::Type type)
{
    if (type == Widget::Type::Form)
        return "form";
    if (type == Widget::Type::Label)
        return "label";
    else if (type == Widget::Type::PushButton)
        return "push-button";
    else if (type == Widget::Type::CheckBox)
        return "checkbox";
    else if (type == Widget::Type::GroupBox)
        return "groupbox";
    else if (type == Widget::Type::SpinBox)
        return "spinbox";
    else if (type == Widget::Type::Slider)
        return "slider";
    else if (type == Widget::Type::ProgressBar)
        return "progress-bar";
    else if (type == Widget::Type::RadioButton)
        return "radiobutton";
    else if (type == Widget::Type::ToggleBox)
        return "togglebox";
    else BUG("Unhandled widget type.");
    return "";
}

// static
Widget::Type Widget::GetWidgetClassType(const std::string& klass)
{
    if (klass == "form")
        return Widget::Type::Form;
    else if (klass == "label")
        return Widget::Type::Label;
    else if (klass == "push-button")
        return Widget::Type::PushButton;
    else if (klass == "checkbox")
        return Widget::Type::CheckBox;
    else if (klass == "groupbox")
        return Widget::Type::GroupBox;
    else if (klass == "spinbox")
        return Widget::Type::SpinBox;
    else if (klass == "slider")
        return Widget::Type::Slider;
    else if (klass == "progress-bar")
        return Widget::Type::ProgressBar;
    else if (klass == "radiobutton")
        return Widget::Type::RadioButton;
    else if (klass == "togglebox")
        return Widget::Type::ToggleBox;
    else BUG("No such widget class.");
    return Widget::Type::RadioButton;
}

} // namespace
