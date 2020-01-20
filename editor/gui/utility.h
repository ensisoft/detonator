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
#  include <neargye/magic_enum.hpp>
#  include <QtWidgets>
#  include <QString>
#  include <QColor>
#  include <QSignalBlocker>
#  include <color_selector.hpp>
#include "warnpop.h"

#include <string>
#include <string_view>
#include <type_traits>

#include "base/assert.h"
#include "graphics/color4f.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"

// general dumping ground for utility type of functionality
// related to the GUI and GUI types.

namespace gui
{

inline gfx::Color4f ToGfx(const QColor& color)
{
    const float a  = color.alphaF();
    const float r  = color.redF();
    const float g  = color.greenF();
    const float b  = color.blueF();
    return gfx::Color4f(r, g, b, a);
}

inline QColor FromGfx(const gfx::Color4f& color)
{
    return QColor::fromRgbF(color.Red(), color.Green(), color.Blue(), color.Alpha());
}

template<typename EnumT>
void PopulateFromEnum(QComboBox* combo)
{
    combo->clear();
    constexpr auto& values = magic_enum::enum_values<EnumT>();
    for (const auto& val : values)
    {
        const std::string name(magic_enum::enum_name(val));
        combo->addItem(QString::fromStdString(name));
    }
}

template<typename EnumT>
EnumT EnumFromCombo(const QComboBox* combo)
{
    const auto& text = combo->currentText();
    const auto& name = text.toStdString();
    const auto& val = magic_enum::enum_cast<EnumT>(name);
    ASSERT(val.has_value());
    return val.value();
}

template<typename T>
void SetValue(QComboBox* combo, T value)
{
    if constexpr (std::is_enum<T>::value)
    {
        const std::string name(magic_enum::enum_name(value));
        const auto index = combo->findText(QString::fromStdString(name));
        ASSERT(index != -1);
        QSignalBlocker s(combo);
        combo->setCurrentIndex(index);
    }
    else
    {
        const QString& str = app::toString(value);
        const auto index = combo->findText(str);
        ASSERT(index != -1);
        QSignalBlocker s(combo);
        combo->setCurrentIndex(index);
    }
}

inline void SetValue(QLineEdit* line, const std::string& val)
{
    QSignalBlocker s(line);
    line->setText(QString::fromStdString(val));
}

inline void SetValue(QDoubleSpinBox* spin, float val)
{
    QSignalBlocker s(spin);
    spin->setValue(val);
}

inline void SetValue(color_widgets::ColorSelector* color, const gfx::Color4f& col)
{
    QSignalBlocker s(color);
    color->setColor(FromGfx(col));
}

inline void SetValue(QCheckBox* check, bool val)
{
    QSignalBlocker s(check);
    check->setChecked(val);
}

struct ComboBoxValueGetter
{
    template<typename T>
    operator T () const
    {
        if constexpr (std::is_enum<T>::value)
        {
            return EnumFromCombo<T>(cmb);
        }
        else
        {
            // arbitrary conversion from QSTring to T
            ASSERT(!"not implemented");
        }
    }
    operator QString() const
    {
        return cmb->currentText();
    }
    const QComboBox* cmb = nullptr;
};

struct LineEditValueGetter
{
    operator QString() const
    {
        return edit->text();
    }
    operator std::string() const
    {
        return app::ToUtf8(edit->text());
    }
    const QLineEdit* edit = nullptr;
};

struct DoubleSpinBoxValueGetter
{
    operator float() const
    { return spin->value(); }
    operator double() const
    { return spin->value(); }
    const QDoubleSpinBox* spin = nullptr;
};

struct ColorGetter
{
    operator gfx::Color4f() const
    { return ToGfx(selector->color()); }
    operator QColor() const
    { return selector->color(); }
    const color_widgets::ColorSelector* selector = nullptr;
};

struct CheckboxGetter
{
    operator bool() const
    { return check->isChecked(); }
    const QCheckBox* check = nullptr;
};

struct SpinBoxValueGetter
{
    operator int() const
    { return spin->value(); }
    const QSpinBox* spin = nullptr;
};

inline ComboBoxValueGetter GetValue(const QComboBox* cmb)
{ return ComboBoxValueGetter {cmb}; }
inline LineEditValueGetter GetValue(const QLineEdit* edit)
{ return LineEditValueGetter { edit }; }
inline DoubleSpinBoxValueGetter GetValue(const QDoubleSpinBox* spin)
{ return DoubleSpinBoxValueGetter { spin }; }
inline ColorGetter GetValue(color_widgets::ColorSelector* selector)
{ return ColorGetter { selector }; }
inline CheckboxGetter GetValue(const QCheckBox* check)
{ return CheckboxGetter { check }; }
inline SpinBoxValueGetter GetValue(const QSpinBox* spin)
{ return SpinBoxValueGetter { spin }; }


} // namespace
