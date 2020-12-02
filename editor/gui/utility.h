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
#  include <QPoint>
#  include <color_selector.hpp>
#include "warnpop.h"

#include <string>
#include <string_view>
#include <type_traits>

#include "base/assert.h"
#include "graphics/color4f.h"
#include "graphics/types.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/resource.h"
#include "editor/gui/gfxwidget.h"

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

inline gfx::FPoint ToGfx(const QPoint& p)
{
    return gfx::FPoint(p.x(), p.y());
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

inline void SetList(QComboBox* combo, const QStringList& list)
{
    QSignalBlocker s(combo);
    combo->clear();
    combo->addItems(list);
}

inline void SetValue(QGroupBox* group, bool on_off)
{
    QSignalBlocker s(group);
    group->setChecked(on_off);
}

inline void SetValue(QLineEdit* line, const std::string& val)
{
    QSignalBlocker s(line);
    line->setText(app::FromUtf8(val));
}
inline void SetValue(QLineEdit* line, const QString& val)
{
    QSignalBlocker s(line);
    line->setText(val);
}

inline void SetValue(QLineEdit* line, int val)
{
    QSignalBlocker s(line);
    line->setText(QString::number(val));
}
inline void SetValue(QLineEdit* line, unsigned val)
{
    QSignalBlocker s(line);
    line->setText(QString::number(val));
}

inline void SetValue(QLineEdit* line, float val)
{
    QSignalBlocker s(line);
    line->setText(QString::number(val));
}
inline void SetValue(QLineEdit* line, double val)
{
    QSignalBlocker s(line);
    line->setText(QString::number(val));
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

inline void SetValue(QSlider* slider, int value)
{
    QSignalBlocker s(slider);
    slider->setValue(value);
}

template<typename Widget, typename Value> inline
void SetUIValue(Widget* widget, const Value& value)
{
    SetValue(widget, value);
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

inline bool FileExists(const std::string& filename)
{
    return !MissingFile(filename);
}
inline bool FileExists(const QString& filename)
{
    return !MissingFile(filename);
}
inline bool FileExists(const QLineEdit* edit)
{
    return !MissingFile(edit);
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
        return cmb->currentText().trimmed();
    }
    operator int() const
    {
        const auto& str = cmb->currentText();
        return str.toInt();
    }
    operator uint() const
    {
        const auto& str = cmb->currentText();
        return str.toUInt();
    }
    const QComboBox* cmb = nullptr;
};

struct LineEditValueGetter
{
    operator QString() const
    {
        return edit->text().trimmed();
    }
    operator std::string() const
    {
        return app::ToUtf8(edit->text().trimmed());
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

struct GroupboxValueGetter
{
    operator bool() const
    { return box->isChecked(); }
    const QGroupBox* box = nullptr;
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
inline GroupboxValueGetter GetValue(const QGroupBox* box)
{ return GroupboxValueGetter { box }; }

template<typename Widget, typename Value>
void GetUIValue(Widget* widget, Value* out)
{
    *out = (Value)GetValue(widget);
}

#if defined(__MSVC__)
template<typename Value>
void GetUIValue(QComboBox* cmb, Value* out)
{
    if constexpr (std::is_enum<Value>::value)
    {
        *out = EnumFromCombo<Value>(cmb);
    }
    else
    {
        static_assert("not implemented");
    }
}
inline void GetUIValue(QComboBox* cmb, int* out)
{
    const auto& str = cmb->currentText();
    *out = str.toInt();
}
inline void GetUIValue(QComboBox* cmb, unsigned* out)
{
    const auto& str = cmb->currentText();
    *out = str.toUInt();
}
inline void GetUIValue(QComboBox* cmb, QString* out)
{
    *out = cmb->currentText().trimmed();
}
#endif // __MSVC__

template<typename Resource>
inline void SetProperty(Resource& res, const QString& name, const QVariant& prop)
{
    res.SetProperty(name, prop);
}
template<typename Resource>
inline void SetProperty(Resource& res, const QString& name, const QComboBox* cmb)
{
    res.SetProperty(name, cmb->currentText());
}
template<typename Resource>
inline void SetProperty(Resource& res, const QString& name, const QLineEdit* edit)
{
    res.SetProperty(name, edit->text());
}
template<typename Resource>
inline void SetProperty(Resource& res, const QString& name, const QDoubleSpinBox* spin)
{
    res.SetProperty(name, spin->value());
}
template<typename Resource>
inline void SetProperty(Resource& res, const QString& name, const QSpinBox* spin)
{
    res.SetProperty(name, spin->value());
}
template<typename Resource>
inline void SetProperty(Resource& res, const QString& name, const QCheckBox* chk)
{
    res.SetProperty(name, chk->isChecked());
}
template<typename Resource>
inline void SetProperty(Resource& res, const QString& name, const QGroupBox* chk)
{
    res.SetProperty(name, chk->isChecked());
}
template<typename Resource>
inline void SetProperty(Resource& res, const QString& name, const color_widgets::ColorSelector* color)
{
    res.SetProperty(name, color->color());
}



// user properties.
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const QVariant& prop)
{
    res.SetUserProperty(name, prop);
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const QComboBox* cmb)
{
    res.SetUserProperty(name, cmb->currentText());
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const QLineEdit* edit)
{
    res.SetUserProperty(name, edit->text());
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const QDoubleSpinBox* spin)
{
    res.SetUserProperty(name, spin->value());
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const QSpinBox* spin)
{
    res.SetUserProperty(name, spin->value());
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const QCheckBox* chk)
{
    res.SetUserProperty(name, chk->isChecked());
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const QGroupBox* chk)
{
    res.SetUserProperty(name, chk->isChecked());
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const color_widgets::ColorSelector* color)
{
    res.SetUserProperty(name, color->color());
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const QString& name, const gui::GfxWidget* widget)
{
    res.SetUserProperty(name + "_clear_color", FromGfx(widget->getClearColor()));
}


template<typename Resource, typename T> inline
void GetProperty(const Resource& res, const QString& name, T* out)
{
    res.GetProperty(name, out);
}

template<typename Resource>
inline void GetProperty(const Resource& res, const QString& name, QComboBox* cmb)
{
    QSignalBlocker s(cmb);

    // default to this in case either the property
    // doesn't exist or the value the property had before
    // is no longer available. (For example the combobox contains
    // resource names and the resource has been deleted)
    //cmb->setCurrentIndex(0);

    QString text;
    if (res.GetProperty(name, &text))
    {
        const auto index = cmb->findText(text);
        if (index != -1)
            cmb->setCurrentIndex(index);
    }
}
template<typename Resource>
inline void GetProperty(const Resource& res, const QString& name, QLineEdit* edit)
{
    QSignalBlocker s(edit);

    QString text;
    if (res.GetProperty(name, &text))
        edit->setText(text);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const QString& name, QDoubleSpinBox* spin)
{
    QSignalBlocker s(spin);

    double value = 0.0f;
    if (res.GetProperty(name, &value))
        spin->setValue(value);

}
template<typename Resource>
inline void GetProperty(const Resource& res, const QString& name, QSpinBox* spin)
{
    QSignalBlocker s(spin);

    int value = 0;
    if (res.GetProperty(name, &value))
        spin->setValue(value);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const QString& name, QCheckBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetProperty(name, &value))
        chk->setChecked(value);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const QString& name, QGroupBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetProperty(name, &value))
        chk->setChecked(value);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const QString& name, color_widgets::ColorSelector* color)
{
    QSignalBlocker s(color);

    QColor value;
    if (res.GetProperty(name, &value))
        color->setColor(value);
}


template<typename Resource, typename T> inline
void GetUserProperty(const Resource& res, const QString& name, T* out)
{
    res.GetUserProperty(name, out);
}

template<typename Resource>
inline void GetUserProperty(const Resource& res, const QString& name, QComboBox* cmb)
{
    QSignalBlocker s(cmb);

    // default to this in case either the property
    // doesn't exist or the value the property had before
    // is no longer available. (For example the combobox contains
    // resource names and the resource has been deleted)
    //cmb->setCurrentIndex(0);
    QString text;
    if (res.GetUserProperty(name, &text))
    {
        const auto index = cmb->findText(text);
        if (index != -1)
            cmb->setCurrentIndex(index);
    }
}
template<typename Resource>
inline void GetUserProperty(const Resource& res, const QString& name, QLineEdit* edit)
{
    QSignalBlocker s(edit);

    QString text;
    if (res.GetUserProperty(name, &text))
        edit->setText(text);
}
template<typename Resource>
inline void GetUserProperty(const Resource& res, const QString& name, QDoubleSpinBox* spin)
{
    QSignalBlocker s(spin);

    double value = 0.0f;
    if (res.GetUserProperty(name, &value))
        spin->setValue(value);

}
template<typename Resource>
inline void GetUserProperty(const Resource& res, const QString& name, QSpinBox* spin)
{
    QSignalBlocker s(spin);

    int value = 0;
    if (res.GetUserProperty(name, &value))
        spin->setValue(value);
}
template<typename Resource>
inline void GetUserProperty(const Resource& res, const QString& name, QCheckBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetUserProperty(name, &value))
        chk->setChecked(value);
}
template<typename Resource>
inline void GetUserProperty(const Resource& res, const QString& name, QGroupBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetUserProperty(name, &value))
        chk->setChecked(value);
}
template<typename Resource>
inline void GetUserProperty(const Resource& res, const QString& name, color_widgets::ColorSelector* color)
{
    QSignalBlocker s(color);

    QColor value;
    if (res.GetUserProperty(name, &value))
        color->setColor(value);
}

template<typename Resource>
inline void GetUserProperty(const Resource& res, const QString& name, gui::GfxWidget* widget)
{
    QSignalBlocker s(widget);
    QColor color = FromGfx(widget->getClearColor());
    if (res.GetUserProperty(name + "_clear_color", &color))
        widget->setClearColor(ToGfx(color));
}

template<typename Widget>
inline int GetCount(Widget* widget)
{
    return widget->count();
}

inline bool MustHaveInput(QLineEdit* line)
{
    const auto& str = line->text();
    if (str.isEmpty())
    {
        line->setFocus();
        return false;
    }
    return true;
}
inline bool MustHaveNumber(QComboBox* box)
{
    const auto& str = box->currentText();
    if (str.isEmpty())
    {
        box->setFocus();
        return false;
    }
    bool ok = false;
    str.toInt(&ok);
    return ok;
}

} // namespace
