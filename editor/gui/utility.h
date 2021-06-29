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
#  include <neargye/magic_enum.hpp>
#  include <QtWidgets>
#  include <QString>
#  include <QColor>
#  include <QSignalBlocker>
#  include <QFileInfo>
#  include <QPoint>
#  include <color_selector.hpp>
#  include <glm/glm.hpp>
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

inline glm::vec4 ToVec4(const QPoint& point)
{ return glm::vec4(point.x(), point.y(), 1.0f, 1.0f); }
inline glm::vec2 ToVec2(const QPoint& point)
{ return glm::vec2(point.x(), point.y()); }

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

inline void SetEnabled(QWidget* widget, bool enabled)
{
    QSignalBlocker s(widget);
    widget->setEnabled(enabled);
}
inline void SetEnabled(QAction* action, bool enabled)
{
    action->setEnabled(enabled);
}

inline void SetVisible(QWidget* widget, bool visible)
{
    QSignalBlocker s(widget);
    widget->setVisible(visible);
}

template<typename EnumT>
void PopulateFromEnum(QComboBox* combo, bool clear = true)
{
    QSignalBlocker s(combo);
    if (clear)
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

inline void SetValue(QAction* action, bool on_off)
{
    QSignalBlocker s(action);
    action->setChecked(on_off);
}

struct ListItemId {
    QString id;
    ListItemId(QString id) : id(id)
    {}
    ListItemId(const std::string& str) : id(app::FromUtf8(str))
    {}
};

inline void SetValue(QComboBox* combo, const QString& str)
{
    QSignalBlocker s(combo);
    const auto index = combo->findText(str);
    combo->setCurrentIndex(index);
    if (combo->isEditable())
        combo->setEditText(str);
}

template<typename T>
void SetValue(QComboBox* combo, T value)
{
    SetValue(combo, app::toString(value));
}

inline void SetValue(QComboBox* combo, const ListItemId& id)
{
    QSignalBlocker s(combo);
    combo->setCurrentIndex(-1);
    if (combo->isEditable())
        combo->clearEditText();

    for (int i=0; i<combo->count(); ++i)
    {
        const QVariant& data = combo->itemData(i);
        if (data.toString() == id.id)
        {
            combo->setCurrentIndex(i);
            if (combo->isEditable())
                combo->setEditText(combo->itemText(i));
            return;
        }
    }
    combo->setCurrentIndex(-1);
}

inline void SetValue(QComboBox* combo, int index)
{
    QSignalBlocker s(combo);
    combo->setCurrentIndex(index);
    if (index == -1 && combo->isEditable())
        combo->clearEditText();
}

// type moved from from here to app/utility.h
// so that the workspace can also use this type.
using ListItem = app::ListItem;


inline void ClearList(QListWidget* list)
{
    QSignalBlocker s(list);
    list->clear();
}

inline void UpdateList(QListWidget* list, const std::vector<ListItem>& items)
{
    // maintain the current/previous selections
    std::unordered_set<QString> selected;
    for (const auto* item : list->selectedItems())
        selected.insert(item->data(Qt::UserRole).toString());

    QSignalBlocker s(list);
    list->clear();

    for (const auto& item : items)
    {
        QListWidgetItem* li = new QListWidgetItem;
        li->setText(item.name);
        li->setData(Qt::UserRole, item.id);
        if (selected.find(item.id) != selected.end())
            li->setSelected(true);
        list->addItem(li);
    }
}

inline void SetList(QComboBox* combo, const std::vector<ListItem>& items)
{
    QSignalBlocker s(combo);
    QString current = combo->currentData(Qt::UserRole).toString();

    combo->clear();
    for (const auto& item : items)
    {
        combo->addItem(item.name, item.id);
    }
    if (current.isEmpty())
        return;

    for (int i=0; i<combo->count(); ++i)
    {
        const QVariant& data = combo->itemData(i);
        if (data.toString() == current)
        {
            combo->setCurrentIndex(i);
            if (combo->isEditable())
                combo->setEditText(combo->itemText(i));
            return;
        }
    }
}

inline void SetList(QComboBox* combo, const QStringList& items)
{
    QSignalBlocker s(combo);
    combo->clear();
    combo->addItems(items);
}

inline void SetValue(QRadioButton* btn, bool value)
{
    QSignalBlocker s(btn);
    btn->setChecked(value);
}

inline void SetValue(QGroupBox* group, bool on_off)
{
    QSignalBlocker s(group);
    group->setChecked(on_off);
}

inline void SetValue(QGroupBox* group, const QString& text)
{
    QSignalBlocker s(group);
    group->setTitle(text);
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
inline void SetValue(color_widgets::ColorSelector* color, QColor value)
{
    QSignalBlocker s(color);
    color->setColor(value);
}

inline void SetValue(QCheckBox* check, Qt::CheckState state)
{
    QSignalBlocker s(check);
    check->setTristate(true);
    check->setCheckState(state);
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

inline void SetValue(QLabel* label, const QString& str)
{
    QSignalBlocker s(label);
    label->setText(str);
}
inline void SetValue(QLabel* label, const std::string& str)
{
    QSignalBlocker s(label);
    label->setText(app::FromUtf8(str));
}

struct NormalizedFloat {
    float value = 0.0f;
    NormalizedFloat(float val) : value(val) {}
};

inline void SetValue(QSlider* slider, NormalizedFloat value)
{
    QSignalBlocker s(slider);
    const float max = slider->maximum();
    slider->setValue(value.value * max);
}

inline float GetNormalizedValue(QSlider* slider)
{
    QSignalBlocker s(slider);
    const auto min = slider->minimum();
    const auto max = slider->maximum();
    const auto val = slider->value();
    const float range = max - min;
    return (val - min) / range;
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
struct ComboBoxItemIdGetter
{
    operator std::string() const
    { return app::ToUtf8(cmb->currentData().toString()); }
    operator QString() const
    { return cmb->currentData().toString(); }
    const QComboBox* cmb = nullptr;
};

struct ListWidgetItemIdGetter
{
    operator std::string() const
    {
        if (const auto* item = list->currentItem())
            return app::ToUtf8(item->data(Qt::UserRole).toString());
        return "";
    }
    operator QString() const
    {
        if (const auto* item = list->currentItem())
            return item->data(Qt::UserRole).toString();
        return QString("");
    }
    const QListWidget* list = nullptr;
};

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
    operator std::string() const
    {
        return app::ToUtf8(cmb->currentText().trimmed());
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

struct RadiobuttonGetter
{
    operator bool() const
    { return button->isChecked(); }
    const QRadioButton* button = nullptr;
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

struct ActionValueGetter
{
    operator bool() const
    { return action->isChecked(); }
    const QAction* action = nullptr;
};
inline ActionValueGetter GetValue(const QAction* action)
{ return ActionValueGetter { action }; }
inline ComboBoxValueGetter GetValue(const QComboBox* cmb)
{ return ComboBoxValueGetter {cmb}; }
inline ComboBoxItemIdGetter GetItemId(const QComboBox* cmb)
{ return ComboBoxItemIdGetter { cmb }; }
inline ListWidgetItemIdGetter GetItemId(const QListWidget* list)
{ return ListWidgetItemIdGetter { list}; }
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
inline RadiobuttonGetter GetValue(const QRadioButton* button)
{ return RadiobuttonGetter { button }; }

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
bool GetUserProperty(const Resource& res, const QString& name, T* out)
{
    return res.GetUserProperty(name, out);
}

template<typename Resource>
inline bool GetUserProperty(const Resource& res, const QString& name, QComboBox* cmb)
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
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const QString& name, QLineEdit* edit)
{
    QSignalBlocker s(edit);

    QString text;
    if (res.GetUserProperty(name, &text))
    {
        edit->setText(text);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const QString& name, QDoubleSpinBox* spin)
{
    QSignalBlocker s(spin);

    double value = 0.0f;
    if (res.GetUserProperty(name, &value))
    {
        spin->setValue(value);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const QString& name, QSpinBox* spin)
{
    QSignalBlocker s(spin);

    int value = 0;
    if (res.GetUserProperty(name, &value))
    {
        spin->setValue(value);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const QString& name, QCheckBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetUserProperty(name, &value))
    {
        chk->setChecked(value);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const QString& name, QGroupBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetUserProperty(name, &value))
    {
        chk->setChecked(value);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const QString& name, color_widgets::ColorSelector* color)
{
    QSignalBlocker s(color);

    QColor value;
    if (res.GetUserProperty(name, &value))
    {
        color->setColor(value);
        return true;
    }
    return false;
}

template<typename Resource>
inline bool GetUserProperty(const Resource& res, const QString& name, gui::GfxWidget* widget)
{
    QSignalBlocker s(widget);
    QColor color = FromGfx(widget->getClearColor());
    if (res.GetUserProperty(name + "_clear_color", &color))
    {
        widget->setClearColor(ToGfx(color));
        return true;
    }
    return false;
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

void PopulateFontNames(QComboBox* cmb);
void PopulateFontSizes(QComboBox* cmb);
void PopulateUIStyles(QComboBox* cmb);

} // namespace
