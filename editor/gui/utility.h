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
#  include <QFileInfo>
#  include <color_selector.hpp>
#include "warnpop.h"

#include <string>
#include <string_view>
#include <type_traits>

#include "base/assert.h"
#include "graphics/color4f.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/resource.h"

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
    line->setText(app::FromUtf8(val));
}

inline void SetValue(QPlainTextEdit* edit, const std::string& val)
{
    QSignalBlocker s(edit);
    edit->setPlainText(app::FromUtf8(val));
}
inline void SetValue(QPlainTextEdit* edit,const QString& val)
{
    QSignalBlocker s(edit);
    edit->setPlainText(val);
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
inline void SetValue(QSpinBox* spin, int value)
{
    QSignalBlocker s(spin);
    spin->setValue(value);
}

inline bool MissingFile(const QLineEdit* edit)
{
    const QString& text = edit->text();
    if (text.isEmpty())
        return false;
    return !QFileInfo(text).exists();
}

inline bool MissingFile(const QString& filename)
{
    return !QFileInfo(filename).exists();
}

inline bool MissingFile(const std::string& filename)
{
    return MissingFile(app::FromUtf8(filename));
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

struct PlainTextEditValueGetter
{
    operator QString() const
    {
        return edit->toPlainText();
    }
    operator std::string() const
    {
        return app::ToUtf8(edit->toPlainText());
    }
    const QPlainTextEdit* edit = nullptr;
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
inline PlainTextEditValueGetter GetValue(const QPlainTextEdit* edit)
{ return PlainTextEditValueGetter { edit }; }
inline DoubleSpinBoxValueGetter GetValue(const QDoubleSpinBox* spin)
{ return DoubleSpinBoxValueGetter { spin }; }
inline ColorGetter GetValue(color_widgets::ColorSelector* selector)
{ return ColorGetter { selector }; }
inline CheckboxGetter GetValue(const QCheckBox* check)
{ return CheckboxGetter { check }; }
inline SpinBoxValueGetter GetValue(const QSpinBox* spin)
{ return SpinBoxValueGetter { spin }; }

inline void SetProperty(app::Resource& res, const QString& name, const QVariant& prop)
{
    res.SetProperty(name, prop);
}
inline void SetProperty(app::Resource& res, const QString& name, const QComboBox* cmb)
{
    res.SetProperty(name, cmb->currentText());
}
inline void SetProperty(app::Resource& res, const QString& name, const QLineEdit* edit)
{
    res.SetProperty(name, edit->text());
}
inline void SetProperty(app::Resource& res, const QString& name, const QDoubleSpinBox* spin)
{
    res.SetProperty(name, spin->value());
}
inline void SetProperty(app::Resource& res, const QString& name, const QSpinBox* spin)
{
    res.SetProperty(name, spin->value());
}
inline void SetProperty(app::Resource& res, const QString& name, const QCheckBox* chk)
{
    res.SetProperty(name, chk->isChecked());
}
inline void SetProperty(app::Resource& res, const QString& name, const QGroupBox* chk)
{
    res.SetProperty(name, chk->isChecked());
}
inline void SetProperty(app::Resource& res, const QString& name, const color_widgets::ColorSelector* color)
{
    res.SetProperty(name, color->color());
}

template<typename T> inline
void GetProperty(const app::Resource& res, const QString& name, T* out)
{
    res.GetProperty(name, out);
}

inline void GetProperty(const app::Resource& res, const QString& name, QComboBox* cmb)
{
    QSignalBlocker s(cmb);

    // default to this in case either the property
    // doesn't exist or the value the property had before
    // is no longer available. (For example the combobox contains
    // resource names and the resource has been deleted)
    cmb->setCurrentIndex(0);

    QString text;
    if (res.GetProperty(name, &text))
    {
        const auto index = cmb->findText(text);
        if (index != -1)
            cmb->setCurrentIndex(index);
    }
}
inline void GetProperty(const app::Resource& res, const QString& name, QLineEdit* edit)
{
    QSignalBlocker s(edit);

    QString text;
    if (res.GetProperty(name, &text))
        edit->setText(text);
}
inline void GetProperty(const app::Resource& res, const QString& name, QDoubleSpinBox* spin)
{
    QSignalBlocker s(spin);

    double value = 0.0f;
    if (res.GetProperty(name, &value))
        spin->setValue(value);

}
inline void GetProperty(const app::Resource& res, const QString& name, QSpinBox* spin)
{
    QSignalBlocker s(spin);

    int value = 0;
    if (res.GetProperty(name, &value))
        spin->setValue(value);
}
inline void GetProperty(const app::Resource& res, const QString& name, QCheckBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetProperty(name, &value))
        chk->setChecked(value);
}
inline void GetProperty(const app::Resource& res, const QString& name, QGroupBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetProperty(name, &value))
        chk->setChecked(value);
}
inline void GetProperty(const app::Resource& res, const QString& name, color_widgets::ColorSelector* color)
{
    QSignalBlocker s(color);

    QColor value;
    if (res.GetProperty(name, &value))
        color->setColor(value);
}

} // namespace
