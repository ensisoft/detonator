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
#  include <QMainWindow>
#  include <QString>
#  include <QColor>
#  include <QSignalBlocker>
#  include <QFileInfo>
#  include <QPoint>
#  include <QPixmap>
#  include <QLineEdit>
#  include <QComboBox>
#  include <QSpinBox>
#  include <QDoubleSpinBox>
#  include <QFontComboBox>
#  include <color_selector.hpp>
#  include <glm/glm.hpp>
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <algorithm>

#include "base/assert.h"
#include "base/math.h"
#include "base/types.h"
#include "graphics/bitmap.h"
#include "graphics/color4f.h"
#include "graphics/types.h"
#include "editor/app/types.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/resource.h"
#include "editor/gui/gfxwidget.h"
#include "editor/gui/timewidget.h"
#include "editor/gui/collapsible_widget.h"
#include "editor/gui/uniform.h"
#include "editor/gui/spinboxwidget.h"
#include "editor/gui/doubleslider.h"
#include "editor/gui/vector3.h"

// general dumping ground for utility type of functionality
// related to the GUI and GUI types.

namespace gui
{

template<typename EnumT>
std::string TranslateEnum(EnumT value)
{
    return std::string(magic_enum::enum_name(value));
}

class AutoHider  {
public:
    AutoHider(QWidget* widget)
      : mWidget(widget)
    {
        QSignalBlocker s(mWidget);
        mWidget->setVisible(true);
    }
   ~AutoHider()
    {
        QSignalBlocker s(mWidget);
        mWidget->setVisible(false);
    }

    AutoHider(const AutoHider&) = delete;
    AutoHider& operator=(const AutoHider&) = delete;
private:
    QWidget* mWidget = nullptr;
};

class AutoEnabler {
public:
    AutoEnabler(QWidget* widget)
      : mWidget(widget)
    {
        QSignalBlocker s(widget);
        widget->setEnabled(false);
    }
    ~AutoEnabler()
    {
        if (mWidget)
        {
            QSignalBlocker s(mWidget);
            mWidget->setEnabled(true);
        }
    }
    void Release()
    {
        mWidget = nullptr;
    }

    AutoEnabler(const AutoEnabler&) = delete;
    AutoEnabler& operator=(const AutoEnabler&) = delete;
private:
    QWidget* mWidget = nullptr;

};

template<typename T>
void Connect(QTableView* view, T* recv, void (T::*selection_changed)(const QItemSelection&, const QItemSelection&))
{
    QObject::connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, recv, selection_changed);
}


inline glm::vec4 ToVec4(const QPoint& point)
{ return glm::vec4(point.x(), point.y(), 1.0f, 1.0f); }
inline glm::vec2 ToVec2(const QPoint& point)
{ return glm::vec2(point.x(), point.y()); }
inline glm::vec2 ToVec2(const QSize& size)
{ return glm::vec2(size.width(), size.height()); }
inline glm::vec2 ToVec2(const QPointF& point)
{ return glm::vec2(point.x(), point.y()); }

inline gfx::FRect ToGfx(const QRectF& rect)
{
    return gfx::FRect(rect.x(), rect.y(), rect.width(), rect.height());
}

inline gfx::Color4f ToGfx(const QColor& color)
{
    const float a  = color.alphaF();
    const float r  = color.redF();
    const float g  = color.greenF();
    const float b  = color.blueF();
    return gfx::Color4f(r, g, b, a);
}
inline gfx::FPoint ToGfx(const glm::vec2& p)
{
    return gfx::FPoint(p.x, p.y);
}
inline gfx::FPoint ToGfx(const QPoint& p)
{
    return gfx::FPoint(p.x(), p.y());
}

inline QColor FromGfx(const gfx::Color4f& color)
{
    return QColor::fromRgbF(color.Red(), color.Green(), color.Blue(), color.Alpha());
}

inline bool Increment(QProgressBar* bar, int value = 1)
{
    QSignalBlocker s(bar);
    int val = bar->value() + value;
    bar->setValue(val);
    if (val % 10)
        return false;

    return true;
}

inline void Increment(QSpinBox* spin, int value = 1)
{
    QSignalBlocker s(spin);
    spin->setValue(spin->value() + value);
}
inline void Increment(QDoubleSpinBox* spin, float value = 1.0f)
{
    QSignalBlocker s(spin);
    spin->setValue(spin->value() + value);
}

inline void Decrement(QSpinBox* spin, int value = 1.0)
{
    QSignalBlocker s(spin);
    spin->setValue(spin->value() - value);
}
inline void Decrement(QDoubleSpinBox* spin, float value = 1.0f)
{
    QSignalBlocker s(spin);
    spin->setValue(spin->value() - value);
}

inline bool CanZoomIn(QDoubleSpinBox* zoom)
{
    const float max = zoom->maximum();
    const float val = zoom->value();
    return val < max;
}
inline bool CanZoomOut(QDoubleSpinBox* zoom)
{
    const float min = zoom->minimum();
    const float val = zoom->value();
    return val > min;
}

inline void SetWindowTitle(QMainWindow* window, const app::AnyString& title)
{
    window->setWindowTitle(title);
}

inline void SetWindowTitle(QDialog* dialog, const app::AnyString& title)
{
    dialog->setWindowTitle(title);
}

inline void SetImage(QLabel* label, const QPixmap& pixmap)
{
    const auto lbl_width = label->width();
    const auto lbl_height = label->height();
    const auto pix_width = pixmap.width();
    const auto pix_height = pixmap.height();
    if (pix_width > pix_height)
        label->setPixmap(pixmap.scaledToWidth(lbl_width));
    else label->setPixmap(pixmap.scaledToHeight(lbl_height));
}

inline bool SetImage(QLabel* label, const gfx::IBitmap& bitmap)
{
    if (!bitmap.IsValid())
        return false;
    const auto width  = bitmap.GetWidth();
    const auto height = bitmap.GetHeight();
    const auto depth  = bitmap.GetDepthBits();
    QImage img;
    if (depth == 8)
        img = QImage((const uchar*)bitmap.GetDataPtr(), width, height, width, QImage::Format_Alpha8);
    else if (depth == 24)
        img = QImage((const uchar*)bitmap.GetDataPtr(), width, height, width * 3, QImage::Format_RGB888);
    else if (depth == 32)
        img = QImage((const uchar*)bitmap.GetDataPtr(), width, height, width * 4, QImage::Format_RGBA8888);
    else return false;
    QPixmap pix;
    pix.convertFromImage(img);
    SetImage(label, pix);
    return true;
}

inline QList<QListWidgetItem*> GetSelection(const QListWidget* list)
{
    return list->selectedItems();
}

inline QListWidgetItem* GetSelectedItem(const QListWidget* list)
{
    const auto& selection = GetSelection(list);
    if (selection.isEmpty())
        return nullptr;

    return selection[0];
}

// Get the selection index list mapped to the source
inline QModelIndexList GetSelection(const QTableView* view)
{
    const auto* data_model = view->model();
    const auto& selection  = view->selectionModel()->selectedRows();
    if (const auto* proxy = qobject_cast<const QSortFilterProxyModel*>(data_model))
    {
        QModelIndexList ret;
        for (auto index : selection)
            ret.append(proxy->mapToSource(index));
        return ret;
    }
    return selection;
}

inline void SetSelection(QTableView* view, const QModelIndexList& selection)
{
    QSignalBlocker f(view);

    auto* data_model = view->model();
    auto* selection_model = view->selectionModel();
    selection_model->clearSelection();
    if (auto* proxy = qobject_cast<QSortFilterProxyModel*>(data_model))
    {
        for (const auto& item : selection) {
            selection_model->select(proxy->mapFromSource(item), QItemSelectionModel::ClearAndSelect |
                                                                QItemSelectionModel::SelectionFlag::Rows);
        }
    } else {
        for (const auto& item : selection) {
            selection_model->select(item, QItemSelectionModel::SelectionFlag::ClearAndSelect |
                                          QItemSelectionModel::SelectionFlag::Rows);
        }
    }
}
inline void SetCurrent(QTableView* view, const QModelIndex& index)
{
    QSignalBlocker s(view);

    auto* data_model = view->model();
    auto* selection_model = view->selectionModel();

    if (auto* proxy = qobject_cast<QSortFilterProxyModel*>(data_model))
    {
        selection_model->setCurrentIndex(proxy->mapFromSource(index),
                                         QItemSelectionModel::SelectionFlag::ClearAndSelect |
                                         QItemSelectionModel::SelectionFlag::Rows);

    }
    else
    {
        selection_model->setCurrentIndex(index, QItemSelectionModel::SelectionFlag::ClearAndSelect |
                                                QItemSelectionModel::SelectionFlag::Rows);

    }
}

inline QModelIndex GetSelectedIndex(const QTableView* view)
{
    const auto& list = GetSelection(view);
    if (list.isEmpty())
        return QModelIndex();
    return list[0];
}

// Get the selected row in the view. (not mapped)
inline int GetSelectedRow(const QTableView* view)
{
    const auto& selection  = view->selectionModel()->selectedRows();
    if (selection.isEmpty())
        return -1;
    return selection[0].row();
}

inline void SelectRow(QTableView* view, int row)
{
    QSignalBlocker s(view);
    if (row == -1)
        view->clearSelection();
    else view->selectRow(row);
}
inline void SelectLastRow(QTableView* view)
{
    QSignalBlocker s(view);
    const auto count = view->model()->rowCount();
    view->selectRow(count-1);
}
inline void ClearSelection(QTableView* view)
{
    QSignalBlocker s(view);
    view->clearSelection();
}
inline int GetCurrentRow(QTableView* view)
{
    return view->currentIndex().row();
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
void PopulateFromEnum(QComboBox* combo, bool clear = true, bool sort = false)
{
    QSignalBlocker s(combo);
    if (clear)
        combo->clear();

    combo->setProperty("__is_enum__", true);

    struct Enum {
        unsigned value;
        std::string str;
    };

    std::vector<Enum> enums;

    constexpr auto& values = magic_enum::enum_values<EnumT>();
    for (const auto& val : values)
    {
        Enum e;
        e.value = magic_enum::enum_integer(val);
        e.str   = TranslateEnum(val);
        enums.push_back(e);
    }
    if (sort)
    {
        std::sort(enums.begin(), enums.end(), [](const auto& a, const auto& b) {
            return a.str < b.str;
        });
    }
    for (const auto& e : enums)
    {
        combo->addItem(QString::fromStdString(e.str), QVariant((uint)e.value));
    }
}

template<typename EnumT>
EnumT EnumFromCombo(const QComboBox* combo)
{
    ASSERT(combo->property("__is_enum__").toBool());

    const auto& value = combo->currentData().toUInt();
    const auto& val = magic_enum::enum_cast<EnumT>(value);
    ASSERT(val.has_value());
    return val.value();
}

inline void SetValue(gui::Vector3* vec, const glm::vec3& value)
{
    QSignalBlocker s(vec);
    vec->SetValue(value);
}

inline void SetValue(gui::DoubleSpinBox* spin, double value)
{
    QSignalBlocker s(spin);
    spin->SetValue(value);
}

inline void SetValue(QProgressBar* bar, int value)
{
    QSignalBlocker s(bar);
    bar->setValue(value);
}
inline void SetValue(QProgressBar* bar, const app::AnyString& fmt)
{
    QSignalBlocker s(bar);
    bar->setTextVisible(true);
    bar->setFormat(fmt);
}

inline void SetValue(QAction* action, bool on_off)
{
    QSignalBlocker s(action);
    action->setChecked(on_off);
}

inline void SetValue(QPushButton* btn, bool checked)
{
    QSignalBlocker s(btn);
    ASSERT(btn->isCheckable());
    btn->setChecked(checked);
}

inline void SetPlaceholderText(QComboBox* cmb, const app::AnyString& text)
{
    QSignalBlocker s(cmb);
    cmb->setPlaceholderText(text);
    if (auto* edit = cmb->lineEdit())
        edit->setPlaceholderText(text);
}

inline void SetPlaceholderText(QLineEdit* edit, const QString& text)
{
    QSignalBlocker s(edit);
    edit->setPlaceholderText(text);
}

inline void SetRange(QDoubleSpinBox* spin, double min, double max)
{
    QSignalBlocker s(spin);
    spin->setRange(min, max);
}

inline void SetRange(QSpinBox* spin, int min, int max)
{
    QSignalBlocker s(spin);
    spin->setRange(min, max);
}

inline void SetRange(QProgressBar* bar, int min, int max)
{
    QSignalBlocker s(bar);
    bar->setRange(min, max);
}

struct ListItemId {
    QString id;
    ListItemId(const QString& id) : id(id)
    {}
    ListItemId(const std::string& str) : id(app::FromUtf8(str))
    {}
    ListItemId(const app::AnyString& str) : id(str)
    {}
    ListItemId(const char* id) : id(id)
    {}
};

struct ListItemTag {
    ListItemTag(const app::AnyString& tag)
      : tag(tag)
    {}
    app::AnyString tag;
};

inline void SetValue(QFontComboBox* cmb, const QFont& font)
{
    QSignalBlocker s(cmb);
    cmb->setFont(font);
}

inline void SetValue(QFontComboBox* combo, const QString& str)
{
    QSignalBlocker s(combo);
    QFont font;
    if (font.fromString(str))
        combo->setCurrentFont(font);
}

inline void SetValue(QComboBox* combo, const QString& str)
{
    ASSERT(combo->property("__is_enum__").toBool() == false);

    QSignalBlocker s(combo);

    const auto id_list = combo->property("__is_id_list__").toBool();
    if (id_list)
    {
        if (combo->isEditable())
        {
            combo->setEditText(str);
            combo->lineEdit()->setCursorPosition(0);
        }
    }
    else
    {
        //ASSERT(combo->count() == 0 ||
        //       combo->property("__is_string_list__").toBool());
        const auto index = combo->findText(str);
        combo->setCurrentIndex(index);
        if (index == -1 && combo->isEditable())
        {
            combo->setEditText(str);
            combo->lineEdit()->setCursorPosition(0);
        }
    }
}


template<typename T>
void SetValue(QComboBox* combo, T value)
{
    if constexpr (std::is_enum<T>::value)
    {
        // must have been populated by a call to PopulateFromEnum
        // then it has the strings and the enum values and the
        // property that marks it as such.
        ASSERT(combo->property("__is_enum__").toBool());

        QSignalBlocker s(combo);

        // look for the item that has the value that matches the
        // enum value we want to set.
        for (int i=0; i<combo->count(); ++i)
        {
            const auto item_value = combo->itemData(i).toUInt();
            const auto enum_value = magic_enum::enum_integer(value);
            if (item_value == enum_value)
            {
                combo->setCurrentIndex(i);
                return;
            }
        }
        BUG("Bug on setting enum value on combo.");
    }
    else
    {
        SetValue(combo, app::toString(value));
    }
}

inline bool SetValue(QComboBox* combo, const ListItemId& id)
{
    ASSERT(combo->property("__is_enum__").toBool() == false);
    ASSERT(combo->property("__is_id_list__").toBool() == true);

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
            return true;
        }
    }
    combo->setCurrentIndex(-1);
    return false;
}

inline void SetValue(QComboBox* combo, int index)
{
    QSignalBlocker s(combo);
    combo->setCurrentIndex(index);
    if (index == -1 && combo->isEditable())
        combo->clearEditText();
}

inline void SetText(QComboBox* combo, const ListItemId& id, const QString& text)
{
    ASSERT(combo->property("__is_enum__").toBool() == false);

    QSignalBlocker s(combo);
    for (int i=0; i<combo->count(); ++i)
    {
        const QVariant& data = combo->itemData(i);
        if (data.toString() == id.id)
        {
            combo->setItemText(i, text);
            return;
        }
    }
}


// type moved from here to app/utility.h
// so that the workspace can also use this type.
using ResourceListItem = app::ResourceListItem;
using ResourceList = app::ResourceList;
using PropertyKey = app::PropertyKey;
using Bytes = app::Bytes;

struct ListItem {
    app::AnyString name;
    app::AnyString id;
};
using ItemList = std::vector<ListItem>;

inline void ClearList(QListWidget* list)
{
    QSignalBlocker s(list);
    list->clear();
}
inline void ClearList(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();
}

inline void SelectItem(QListWidget* list, unsigned row)
{
    QSignalBlocker s(list);
    for (int i=0; i<list->count(); ++i)
    {
        auto* item = list->item(i);
        item->setSelected(i == row);
    }
}

inline void SelectItem(QListWidget* list,  const ListItemId& id)
{
    QSignalBlocker s(list);
    for (int i=0; i<list->count(); ++i)
    {
        auto* item = list->item(i);
        const auto& data = item->data(Qt::UserRole);
        if (data.toString() == id.id)
            item->setSelected(true);
        else item->setSelected(false);
    }
}

inline void SelectLastItem(QListWidget* list)
{
    QSignalBlocker s(list);
    const auto count = list->count();
    if (count == 0)
        return;
    for (int i=0; i<count; ++i)
        list->item(i)->setSelected(false);
    list->item(count-1)->setSelected(true);
}

inline void SetCurrentRow(QListWidget* list, int row)
{
    QSignalBlocker s(list);
    list->setCurrentRow(row, QItemSelectionModel::SelectionFlag::ClearAndSelect);
}

inline int GetCurrentRow(QListWidget* list)
{
    QSignalBlocker s(list);
    return list->currentRow();
}

inline void ClearTable(QTableWidget* table)
{
    QSignalBlocker s(table);
    table->clearContents();
}

inline void ResizeTable(QTableWidget* table, unsigned rows, unsigned cols)
{
    QSignalBlocker s(table);
    table->setRowCount(rows);
    table->setColumnCount(cols);
}

inline QTableWidgetItem* SetTableItem(QTableWidget* table, unsigned row, unsigned col, QString text)
{
    QSignalBlocker s(table);
    QTableWidgetItem* item = table->item(row, col);
    if (item == nullptr)
    {
        item = new QTableWidgetItem(text);
        table->setItem(row, col, item);
    }
    else
    {
        item->setText(text);
    }

    return item;
}

template<typename T>
inline QTableWidgetItem* SetTableItem(QTableWidget* table, unsigned row, unsigned col, T value)
{
    QSignalBlocker s(table);
    QTableWidgetItem* item = table->item(row, col);
    if (item == nullptr)
    {
        item = new QTableWidgetItem(app::toString(value));
        table->setItem(row, col, item);
    }
    else
    {
        item->setText(app::toString(value));
    }
    return item;
}

inline void SelectTableRow(QTableWidget* table, unsigned row, bool selected)
{
    ASSERT(row < table->rowCount());
    QSignalBlocker s(table);
    for (int i=0; i<table->columnCount(); ++i)
    {
        QTableWidgetItem* item = table->item(static_cast<int>(row), i);
        if (!item)
            continue;
        item->setSelected(selected);
    }
}


inline QListWidgetItem* AddItem(QListWidget* list, const QString& text)
{
    QSignalBlocker s(list);
    QListWidgetItem* item = new QListWidgetItem();
    item->setText(text);
    list->addItem(item);
    return item;
}

inline void SetList(QListWidget* list, const ResourceList& items)
{
    // maintain the current/previous selections
    std::unordered_set<QString> selected;
    for (const auto* item : list->selectedItems())
        selected.insert(item->data(Qt::UserRole+0).toString());

    QSignalBlocker s(list);
    list->clear();

    for (const auto& item : items)
    {
        QListWidgetItem* li = new QListWidgetItem;
        li->setText(item.name);
        li->setData(Qt::UserRole+0, item.id);
        li->setData(Qt::UserRole+1, item.tag);
        li->setIcon(item.icon);
        list->addItem(li);
        // ffs, the selection must be *done* after adding the
        // item to the list otherwise it doesn't work. lalalalaal!
        if (item.selected)
            li->setSelected(true);
        else if (!item.selected)
            li->setSelected(false);
        else  li->setSelected(selected.find(item.id) != selected.end());
    }
}

template<typename Type>
void SetList(QComboBox* combo, const std::vector<Type>& list)
{
    QSignalBlocker s(combo);
    QString current = combo->currentData(Qt::UserRole).toString();
    combo->setProperty("__is_id_list__", true);

    combo->clear();
    for (const auto& item : list)
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
            return;
        }
    }
}

inline void SetIndex(QComboBox* combo, int index)
{
    QSignalBlocker s(combo);
    combo->setCurrentIndex(index);
}

inline int GetIndex(QComboBox* combo)
{
    return combo->currentIndex();
}

inline void SetList(QComboBox* combo, const QStringList& items)
{
    QSignalBlocker s(combo);
    combo->setProperty("__is_string_list__", true);
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

inline void SetValue(QGroupBox* group, const app::AnyString& text)
{
    QSignalBlocker s(group);
    group->setTitle(text);
}

inline void SetValue(QLineEdit* line, const app::AnyString& val)
{
    QSignalBlocker s(line);
    line->setText(val);
    line->setCursorPosition(0);
}

inline void SetValue(QLineEdit* line, int val)
{
    QSignalBlocker s(line);
    line->setText(QString::number(val));
    line->setCursorPosition(0);
}
inline void SetValue(QLineEdit* line, unsigned val)
{
    QSignalBlocker s(line);
    line->setText(QString::number(val));
    line->setCursorPosition(0);
}

inline void SetValue(QLineEdit* line, float val)
{
    QSignalBlocker s(line);
    line->setText(QString::number(val));
    line->setCursorPosition(0);
}
inline void SetValue(QLineEdit* line, double val)
{
    QSignalBlocker s(line);
    line->setText(QString::number(val));
    line->setCursorPosition(0);
}

inline void SetValue(QLineEdit* line, const app::Bytes& bytes)
{
    QSignalBlocker s(line);
    line->setText(app::toString(bytes));
    line->setCursorPosition(0);
}

inline void SetValue(QPlainTextEdit* edit, const app::AnyString& value)
{
    QSignalBlocker s(edit);
    edit->setPlainText(value);
}

inline void SetValue(QDoubleSpinBox* spin, base::FDegrees value)
{
    QSignalBlocker s(spin);
    spin->setValue(value.ToDegrees());
}

inline void SetValue(QDoubleSpinBox* spin, base::FRadians value)
{
    QSignalBlocker s(spin);
    spin->setValue(value.ToDegrees());
}

inline void SetValue(QDoubleSpinBox* spin, float val)
{
    QSignalBlocker s(spin);
    if (spin->objectName() == "zoom")
    {
        spin->setMinimum(10.0f);
        spin->setMaximum(500.0f);
        spin->setSingleStep(1.0f);
        spin->setDecimals(0);
        spin->setSuffix(" %");
        spin->setValue(100 * val);
        spin->setProperty("zoom_value", true);
        return;
    }
    spin->setValue(val);
}
inline void SetSuffix(QDoubleSpinBox* spin, const app::AnyString& suffix)
{
    QSignalBlocker s(spin);
    spin->setSuffix(suffix);
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
inline void SetSuffix(QSpinBox* spin, const app::AnyString& suffix)
{
    QSignalBlocker s(spin);
    spin->setSuffix(suffix);
}


inline void SetMin(QDoubleSpinBox* spin, double min)
{
    QSignalBlocker s(spin);
    spin->setMinimum(min);
}
inline void SetMax(QDoubleSpinBox* spin, double min)
{
    QSignalBlocker s(spin);
    spin->setMinimum(min);
}
inline void SetMinMax(QDoubleSpinBox* spin, double min, double max)
{
    QSignalBlocker s(spin);
    spin->setMinimum(min);
    spin->setMaximum(max);
}

inline void SetValue(gui::DoubleSlider* slider, double value)
{
    QSignalBlocker s(slider);
    slider->SetValue(value);
}

inline void SetValue(QSlider* slider, int value)
{
    QSignalBlocker s(slider);
    slider->setValue(value);
}

inline void SetValue(QLabel* label, const app::AnyString& value)
{
    QSignalBlocker s(label);
    label->setText(value);
}

inline void SetValue(gui::TimeWidget* time, unsigned value)
{
    QSignalBlocker s(time);
    time->SetTime(value);
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

inline void SetValue(Uniform* uniform, float value, const app::AnyString& suffix = "")
{
    QSignalBlocker s(uniform);
    uniform->SetType(Uniform::Type::Float, suffix);
    uniform->SetValue(value);
}

inline void SetValue(Uniform* uniform, const glm::vec2& value, const app::AnyString& suffix = "")
{
    QSignalBlocker s(uniform);
    uniform->SetType(Uniform::Type::Vec2, suffix);
    uniform->SetValue(value);
}

inline void SetValue(Uniform* uniform, const glm::vec3& value, const app::AnyString& suffix = "")
{
    QSignalBlocker s(uniform);
    uniform->SetType(Uniform::Type::Vec3, suffix);
    uniform->SetValue(value);
}

inline void SetValue(Uniform* uniform, const glm::vec4& value, const app::AnyString& suffix = "")
{
    QSignalBlocker s(uniform);
    uniform->SetType(Uniform::Type::Vec4, suffix);
    uniform->SetValue(value);
}
inline void SetValue(Uniform* uniform, const QString& value)
{
    QSignalBlocker s(uniform);
    uniform->SetType(Uniform::Type::String);
    uniform->SetValue(value);
}
inline void SetValue(Uniform* uniform, const QColor& value)
{
    QSignalBlocker s(uniform);
    uniform->SetType(Uniform::Type::Color);
    uniform->SetValue(value);
}
inline void SetValue(Uniform* uniform, const gfx::Color4f& color)
{
    QSignalBlocker s(uniform);
    uniform->SetType(Uniform::Type::Color);
    uniform->SetValue(FromGfx(color));
}
inline void SetValue(Uniform* uniform, const std::string& value)
{
    QSignalBlocker s(uniform);
    uniform->SetType(Uniform::Type::String);
    uniform->SetValue(app::FromUtf8(value));
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

inline float GetAngle(const QDoubleSpinBox* spin)
{
    return qDegreesToRadians((float)spin->value());
}
inline void SetAngle(QDoubleSpinBox* spin, float value)
{
    SetValue(spin, qRadiansToDegrees(value));
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

inline bool MissingFile(const app::AnyString& filename)
{
    return !QFileInfo(filename).exists();
}

using app::FileExists;

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
    operator app::AnyString() const
    { return cmb->currentData().toString(); }
    const QComboBox* cmb = nullptr;
};

struct ListWidgetItemTextGetter
{
    operator std::string() const
    {
        if (item)
            return app::ToUtf8(item->text());
        return "";
    }
    operator QString() const
    {
        if (item)
            return item->text();
        return QString("");
    }
    operator app::AnyString() const
    {
        if (item)
            return item->text();
        return QString("");
    }
    const QListWidgetItem* item = nullptr;
};


struct ListWidgetItemIdGetter
{
    operator std::string() const
    {
        if (item)
            return app::ToUtf8(item->data(Qt::UserRole+0).toString());
        return "";
    }
    operator QString() const
    {
        if (item)
            return item->data(Qt::UserRole+0).toString();
        return QString("");
    }
    operator app::AnyString() const
    {
        if (item)
            return item->data(Qt::UserRole+0).toString();
        return QString("");
    }
    const QListWidgetItem* item = nullptr;
};

struct ListWidgetItemTagGetter
{
    operator std::string() const
    {
        if (item)
            return app::ToUtf8(item->data(Qt::UserRole+1).toString());
        return "";
    }
    operator QString() const
    {
        if (item)
            return item->data(Qt::UserRole+1).toString();
        return QString("");
    }
    operator app::AnyString() const
    {
        if (item)
            return item->data(Qt::UserRole+1).toString();
        return QString("");
    }
    const QListWidgetItem* item = nullptr;
};


struct FontComboValueGetter
{
    operator QFont() const
    {
        return cmb->currentFont();
    }
    operator QString() const
    {
        return cmb->currentFont().toString();
    }

    const QFontComboBox* cmb = nullptr;
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
    operator app::AnyString() const
    {
        return app::AnyString(cmb->currentText().trimmed());
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
    operator app::AnyString() const
    {
        return edit->text().trimmed();
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
    {
        if (spin->objectName() == "zoom" && spin->property("zoom_value").isValid())
            return spin->value() / 100.0f;
        return spin->value();
    }
    operator double() const
    {
        if (spin->objectName() == "zoom" && spin->property("zoom_value").isValid())
            return spin->value() / 100.0;
        return spin->value();
    }
    operator base::FDegrees() const
    {
        return base::FDegrees(spin->value());
    }
    operator base::FRadians() const
    {
        return base::FRadians(qDegreesToRadians(spin->value()));
    }

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
struct SliderValueGetter
{
    operator int() const
    { return slider->value(); }
    const QSlider* slider = nullptr;
};

struct DoubleSliderValueGetter
{
    operator double() const
    { return slider->GetValue(); }
    const gui::DoubleSlider* slider = nullptr;
};

struct Vec3ValueGetter
{
    operator glm::vec3() const
    { return vector->GetValue(); }
    const gui::Vector3* vector = nullptr;
};

inline Vec3ValueGetter GetValue(const gui::Vector3* vector)
{ return Vec3ValueGetter { vector }; }
    inline FontComboValueGetter GetValue(QFontComboBox* cmb)
{ return FontComboValueGetter { cmb }; }
inline DoubleSliderValueGetter GetValue(const gui::DoubleSlider* slider)
{ return DoubleSliderValueGetter { slider }; }
inline SliderValueGetter GetValue(const QSlider* slider)
{ return SliderValueGetter { slider }; }
inline ActionValueGetter GetValue(const QAction* action)
{ return ActionValueGetter { action }; }
inline ComboBoxValueGetter GetValue(const QComboBox* cmb)
{ return ComboBoxValueGetter {cmb}; }
inline ComboBoxItemIdGetter GetItemId(const QComboBox* cmb)
{
    ASSERT(cmb->property("__is_enum__").toBool() == false);
    ASSERT(cmb->property("__is_string_list__").toBool() == false);
    if (cmb->count())
    {
        ASSERT(cmb->property("__is_id_list__").toBool());
    }

    return ComboBoxItemIdGetter { cmb };
}
inline ListWidgetItemIdGetter GetItemId(const QListWidget* list)
{ return ListWidgetItemIdGetter { GetSelectedItem(list) }; }
inline ListWidgetItemIdGetter GetItemId(const QListWidgetItem* item)
{ return ListWidgetItemIdGetter { item }; }
inline ListWidgetItemTextGetter GetItemText(const QListWidgetItem* item)
{ return ListWidgetItemTextGetter { item }; }
inline ListWidgetItemTagGetter GetItemTag(const QListWidget* list)
{ return ListWidgetItemTagGetter { GetSelectedItem(list) }; }
inline ListWidgetItemTagGetter GetItemTag(const QListWidgetItem* item)
{ return ListWidgetItemTagGetter { item }; }
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
inline unsigned GetValue(const gui::TimeWidget* time)
{ return time->GetTime(); }

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
inline void SetProperty(Resource& res, const PropertyKey& key, const gui::CollapsibleWidget* widget)
{ res.SetProperty(key + "_collapsed", widget->IsCollapsed()); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QComboBox* cmb)
{ res.SetProperty(key, cmb->currentText()); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QLineEdit* edit)
{ res.SetProperty(key, edit->text()); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QDoubleSpinBox* spin)
{ res.SetProperty(key, spin->value()); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QSpinBox* spin)
{ res.SetProperty(key, spin->value()); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QCheckBox* chk)
{ res.SetProperty(key, chk->isChecked()); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QGroupBox* chk)
{ res.SetProperty(key, chk->isChecked()); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const color_widgets::ColorSelector* color)
{ res.SetProperty(key, color->color()); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const app::AnyString& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const std::string& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QString& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const char* value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QJsonObject& json)
{ res.SetProperty(key, json); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QByteArray& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QColor& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QVariantMap& map)
{ res.SetProperty(key, map); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, int value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, unsigned value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, float value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, double value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, quint64 value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QPoint& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const QSize& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const glm::vec2& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const glm::vec3& value)
{ res.SetProperty(key, value); }
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, const glm::vec4& value)
{ res.SetProperty(key, value); }

#if !defined(__MSVC__)
// this overload cannot happen on 64bit MSVC build because size_t is unsigned __int64 which
// is the same as quint64
template<typename Resource>
inline void SetProperty(Resource& res, const PropertyKey& key, size_t value)
{ res.SetProperty(key, quint64(value)); }
#endif

// user properties.
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const gui::DoubleSpinBox* spin)
{
    if (spin->HasValue())
        res.SetUserProperty(key + "_value", spin->GetValue().value());
    else res.DeleteUserProperty(key + "_value");
}

template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const gui::CollapsibleWidget* widget)
{ res.SetUserProperty(key + "_collapsed", widget->IsCollapsed()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QSplitter* splitter)
{ res.SetUserProperty(key, splitter->saveState()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QComboBox* cmb)
{ res.SetUserProperty(key, cmb->currentText()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QLineEdit* edit)
{ res.SetUserProperty(key, edit->text()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QDoubleSpinBox* spin)
{ res.SetUserProperty(key, spin->value()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QSpinBox* spin)
{ res.SetUserProperty(key, spin->value()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QCheckBox* chk)
{ res.SetUserProperty(key, chk->isChecked()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QAction* chk)
{ res.SetUserProperty(key, chk->isChecked()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QGroupBox* chk)
{ res.SetUserProperty(key, chk->isChecked()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const color_widgets::ColorSelector* color)
{ res.SetUserProperty(key, color->color()); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const gui::GfxWidget* widget)
{
    if (const auto* color = widget->GetClearColor())
        res.SetUserProperty(key + "_clear_color", FromGfx(*color));
}
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const app::AnyString& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QString& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const std::string& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const char* value)
{ res.SetUserProperty(key, QString(value)); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QJsonObject& json)
{ res.SetUserProperty(key, json); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QByteArray& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QVariantMap& map)
{ res.SetUserProperty(key, map); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QColor& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, int value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, unsigned value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, float value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, double value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, quint64 value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QPoint& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const QSize& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const glm::vec2& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const glm::vec3& value)
{ res.SetUserProperty(key, value); }
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, const glm::vec4& value)
{ res.SetUserProperty(key, value); }

#if !defined(__MSVC__)
// this overload cannot happen on 64bit MSVC build because size_t is unsigned __int64 which
// is the same as quint64
template<typename Resource>
inline void SetUserProperty(Resource& res, const PropertyKey& key, size_t value)
{ res.SetUserProperty(key, quint64(value)); }
#endif

template<typename Resource, typename T> inline
bool GetProperty(const Resource& res, const PropertyKey& key, T* out)
{
    return res.GetProperty(key, out);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, QComboBox* cmb)
{
    QSignalBlocker s(cmb);

    // default to this in case either the property
    // doesn't exist or the value the property had before
    // is no longer available. (For example the combobox contains
    // resource names and the resource has been deleted)
    //cmb->setCurrentIndex(0);

    QString text;
    if (res.GetProperty(key, &text))
    {
        const auto index = cmb->findText(text);
        if (index != -1)
            cmb->setCurrentIndex(index);
    }
}
template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, QLineEdit* edit)
{
    QSignalBlocker s(edit);

    QString text;
    if (res.GetProperty(key, &text))
        edit->setText(text);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, QDoubleSpinBox* spin)
{
    QSignalBlocker s(spin);

    double value = 0.0f;
    if (res.GetProperty(key, &value))
        spin->setValue(value);

}
template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, QSpinBox* spin)
{
    QSignalBlocker s(spin);

    int value = 0;
    if (res.GetProperty(key, &value))
        spin->setValue(value);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, QCheckBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetProperty(key, &value))
        chk->setChecked(value);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, QGroupBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetProperty(key, &value))
        chk->setChecked(value);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, color_widgets::ColorSelector* color)
{
    QSignalBlocker s(color);

    QColor value;
    if (res.GetProperty(key, &value))
        color->setColor(value);
}
template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, std::string* str)
{
    *str = res.GetProperty(key, std::string(""));
}

template<typename Resource>
inline void GetProperty(const Resource& res, const PropertyKey& key, size_t* value)
{
    *value = res.GetProperty(key, quint64(0));
}

template<typename Resource>
inline void GetUserProperty(const Resource& res, const PropertyKey& key, size_t* value)
{
    *value = res.GetUserProperty(key, quint64(0));
}

template<typename Resource, typename T> inline
bool GetUserProperty(const Resource& res, const PropertyKey& key, T* out)
{
    return res.GetUserProperty(key, out);
}

template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, QSplitter* splitter)
{
    QSignalBlocker s(splitter);
    QByteArray state;
    if (res.GetUserProperty(key, &state))
    {
        splitter->restoreState(state);
        return true;
    }
    return false;
}

template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, QComboBox* cmb)
{
    QSignalBlocker s(cmb);

    // default to this in case either the property
    // doesn't exist or the value the property had before
    // is no longer available. (For example the combobox contains
    // resource names and the resource has been deleted)
    //cmb->setCurrentIndex(0);
    QString text;
    if (res.GetUserProperty(key, &text))
    {
        const auto index = cmb->findText(text);
        if (index != -1)
            cmb->setCurrentIndex(index);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, QLineEdit* edit)
{
    QSignalBlocker s(edit);

    QString text;
    if (res.GetUserProperty(key, &text))
    {
        edit->setText(text);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, QDoubleSpinBox* spin)
{
    QSignalBlocker s(spin);

    double value = 0.0f;
    if (res.GetUserProperty(key, &value))
    {
        spin->setValue(value);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, QSpinBox* spin)
{
    QSignalBlocker s(spin);

    int value = 0;
    if (res.GetUserProperty(key, &value))
    {
        spin->setValue(value);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, QCheckBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetUserProperty(key, &value))
    {
        chk->setChecked(value);
        return true;
    }
    return false;
}

template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, QAction* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetUserProperty(key, &value))
    {
        chk->setChecked(value);
        return true;
    }
    return false;
}

template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, QGroupBox* chk)
{
    QSignalBlocker s(chk);

    bool value = false;
    if (res.GetUserProperty(key, &value))
    {
        chk->setChecked(value);
        return true;
    }
    return false;
}
template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, color_widgets::ColorSelector* color)
{
    QSignalBlocker s(color);

    QColor value;
    if (res.GetUserProperty(key, &value))
    {
        color->setColor(value);
        return true;
    }
    return false;
}

template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, gui::DoubleSpinBox* widget)
{
    double value = 0.0;
    if (res.GetUserProperty(key + "_value", &value))
    {
        widget->SetValue(value);
        return true;
    }
    return false;
}

template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, gui::CollapsibleWidget* widget)
{
    bool collapsed = false;
    if (res.GetUserProperty(key + "_collapsed", &collapsed))
    {
        widget->Collapse(collapsed);
        return true;
    }
    return false;
}


template<typename Resource>
inline bool GetUserProperty(const Resource& res, const PropertyKey& key, gui::GfxWidget* widget)
{
    QColor color;
    if (res.GetUserProperty(key + "_clear_color", &color))
    {
        QSignalBlocker s(widget);
        widget->SetClearColor(ToGfx(color));
        return true;
    }
    return false;
}

template<typename Widget>
inline int GetCount(const Widget* widget)
{
    return widget->count();
}

inline int GetCount(const QTableView* view)
{
    const auto* data_model = view->model();
    if (const auto* proxy = qobject_cast<const QSortFilterProxyModel*>(data_model))
        return proxy->rowCount();
    return data_model->rowCount();
}

inline bool MustHaveInput(QComboBox* box)
{
    if (box->currentIndex() == -1)
    {
        box->setFocus();
        return false;
    }
    return true;
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



QPixmap ToGrayscale(QPixmap pixmap);

std::vector<QString> ListWSFonts(const QString& workspace_dir);

// List the font's installed with the application.
// returns a list of font uris, i.e. app://fonts/foo.otf
std::vector<QString> ListAppFonts();

std::vector<ResourceListItem> ListParticles();
std::vector<ResourceListItem> ListShaders();
std::vector<ResourceListItem> ListPresetParticles();

void PopulateShaderList(QComboBox* cmb, const QString& filter);
void PopulatePresetParticleList(QComboBox* cmb);
void PopulateParticleList(QComboBox* cmb);
void PopulateFontNames(QComboBox* cmb);
void PopulateFontSizes(QComboBox* cmb);
void PopulateUIStyles(QComboBox* cmb);
void PopulateUIKeyMaps(QComboBox* cmb);
void PopulateQtStyles(QComboBox* cmb);

} // namespace
