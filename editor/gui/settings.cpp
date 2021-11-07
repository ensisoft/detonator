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

#define LOGTAG "settings"

#include "config.h"

#include "warnpush.h"
#  include <QJsonDocument>
#  include <QJsonArray>
#  include <QFile>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "settings.h"

namespace gui
{

// Construct a new settings object for reading the
// application "master" settings.
Settings::Settings(const QString& organization, const QString& application)
  : mSettings(new AppSettingsStorage(organization, application))
{}

// Construct a new settings object for reading the
// settings from a specific file. The contents are in JSON.
Settings::Settings(const QString& filename)
  : mSettings(new JsonFileSettingsStorage(filename))
{}

bool Settings::Load()
{
    return mSettings->Load();
}

bool Settings::Save()
{
    return mSettings->Save();
}

bool Settings::GetValue(const QString& module, const QString& key, std::size_t* out) const
{
    const auto& value = mSettings->GetValue(module + "/" + key);
    if (!value.isValid())
        return false;
    *out = qvariant_cast<quint64>(value);
    return true;
}
bool Settings::GetValue(const QString& module, const QString& key, std::string* out) const
{
    const auto& value = mSettings->GetValue(module + "/" + key);
    if (!value.isValid())
        return false;
    *out = app::ToUtf8(qvariant_cast<QString>(value));
    return true;
}

std::string Settings::GetValue(const QString& module, const QString& key, const std::string& defaultValue) const
{
    const QString& temp = GetValue(module, key, app::FromUtf8(defaultValue));
    return app::ToUtf8(temp);
}
std::size_t Settings::GetValue(const QString& module, const QString& key, std::size_t defaultValue) const
{
    const auto& value = mSettings->GetValue(module + "/" + key);
    if (!value.isValid())
        return defaultValue;
    return qvariant_cast<quint64>(value);
}

void Settings::SaveWidget(const QString& module, const QTableView* table)
{
    const auto* model  = table->model();
    const auto objName = table->objectName();
    const auto numCols = model->columnCount();

    for (int i=0; i<numCols-1; ++i)
    {
        const auto& key = QString("%1_column_%2").arg(objName).arg(i);
        const auto width = table->columnWidth(i);
        SetValue(module, key, width);
    }
    if (table->isSortingEnabled())
    {
        const QHeaderView* header = table->horizontalHeader();
        const auto sortColumn = header->sortIndicatorSection();
        const auto sortOrder  = header->sortIndicatorOrder();
        SetValue(module, objName + "/sort_column", sortColumn);
        SetValue(module, objName + "/sort_order", sortOrder);
    }
}
void Settings::SaveWidget(const QString& module, const gui::GfxWidget* widget)
{
    const auto name = widget->objectName();
    SetValue(module, name + "_clear_color", FromGfx(widget->getClearColor()));
}

void Settings::SaveWidget(const QString& module, const color_widgets::ColorSelector* color)
{
    SetValue(module, color->objectName(), color->color());
}
void Settings::SaveWidget(const QString& module, const QComboBox* cmb)
{
    SetValue(module, cmb->objectName(), cmb->currentText());
}
void Settings::SaveWidget(const QString& module, const QLineEdit* line)
{
    SetValue(module, line->objectName(), line->text());
}
void Settings::SaveWidget(const QString& module, const QSpinBox* spin)
{
    SetValue(module, spin->objectName(), spin->value());
}
void Settings::SaveWidget(const QString& module, const QDoubleSpinBox* spin)
{
    const auto& name = spin->objectName();
    SetValue(module, name, spin->value());
    SetValue(module, name + "_min_value", spin->minimum());
    SetValue(module, name + "_max_value", spin->maximum());
}
void Settings::SaveWidget(const QString& module, const QGroupBox* grp)
{
    SetValue(module, grp->objectName(), grp->isChecked());
}

void Settings::SaveWidget(const QString& module, const QCheckBox* chk)
{
    SetValue(module, chk->objectName(), chk->isChecked());
}
// Save the UI state of a widget.
void Settings::SaveWidget(const QString& module, const QSplitter* splitter)
{
    const auto& state = splitter->saveState();
    SetValue(module, splitter->objectName(), QString::fromLatin1(state.toBase64()));
}

void Settings::LoadWidget(const QString& module, gui::GfxWidget* widget) const
{
    const auto name = widget->objectName();
    const auto val  = GetValue(module, name + "_clear_color", FromGfx(widget->getClearColor()));
    widget->setClearColor(ToGfx(val));
}
void Settings::LoadWidget(const QString& module, QSplitter* splitter) const
{
    QString base64;
    if (!GetValue(module, splitter->objectName(), &base64))
        return;
    const auto& state = QByteArray::fromBase64(base64.toLatin1());
    if (state.isEmpty())
        return;
    splitter->restoreState(state);
}

void Settings::LoadWidget(const QString& module, QTableView* table) const
{
    const auto* model  = table->model();
    const auto objName = table->objectName();
    const auto numCols = model->columnCount();

    QSignalBlocker s(table);

    for (int i=0; i<numCols-1; ++i)
    {
        const auto& key  = QString("%1_column_%2").arg(objName).arg(i);
        const auto width = GetValue(module, key, table->columnWidth(i));
        table->setColumnWidth(i, width);
    }
    if (table->isSortingEnabled())
    {
        const QHeaderView* header = table->horizontalHeader();
        const auto column = GetValue(module, objName + "/sort_column",
                                     (int) header->sortIndicatorSection());
        const auto order = GetValue(module, objName + "/sort_order",
                                    (int) header->sortIndicatorOrder());
        table->sortByColumn(column,(Qt::SortOrder)order);
    }
}
// Load the UI state of a widget.
void Settings::LoadWidget(const QString& module, QComboBox* cmb) const
{
    QSignalBlocker s(cmb);
    const auto& text = GetValue(module, cmb->objectName(), cmb->currentText());
    const auto index = cmb->findText(text);
    if (index != -1)
        cmb->setCurrentIndex(index);
}
void Settings::LoadWidget(const QString& module, QCheckBox* chk) const
{
    const bool value = GetValue<bool>(module, chk->objectName(), chk->isChecked());
    QSignalBlocker s(chk);
    chk->setChecked(value);
}
void Settings::LoadWidget(const QString& module, QGroupBox* grp) const
{
    const bool value = GetValue<bool>(module, grp->objectName(),
            grp->isChecked());
    QSignalBlocker s(grp);
    grp->setChecked(value);
}
void Settings::LoadWidget(const QString& module, QDoubleSpinBox* spin) const
{
    const auto& name = spin->objectName();
    const double min   = GetValue<double>(module, name + "_min_value", spin->minimum());
    const double max   = GetValue<double>(module, name + "_max_value", spin->maximum());
    const double value = GetValue<double>(module, name, spin->value());
    QSignalBlocker s(spin);
    spin->setMaximum(max);
    spin->setMinimum(min);
    spin->setValue(value);
}

void Settings::LoadWidget(const QString& module, QSpinBox* spin) const
{
    const int value = GetValue<int>(module, spin->objectName(), spin->value());
    QSignalBlocker s(spin);
    spin->setValue(value);
}
void Settings::LoadWidget(const QString& module, QLineEdit* line) const
{
    const auto& str = GetValue<QString>(module, line->objectName(), line->text());
    QSignalBlocker s(line);
    line->setText(str);
}
void Settings::LoadWidget(const QString& module, color_widgets::ColorSelector* selector) const
{
    const auto& color = GetValue<QColor>(module, selector->objectName(), selector->color());
    QSignalBlocker s(selector);
    selector->setColor(color);
}

void Settings::SaveAction(const QString& module, const QAction* action)
{
    SetValue(module, action->objectName(), action->isChecked());
}

void Settings::LoadAction(const QString& module, QAction* action) const
{
    QSignalBlocker s(action);
    const auto name = action->objectName();
    const auto val  = GetValue(module, name, action->isChecked());
    action->setChecked(val);
}

bool Settings::JsonFileSettingsStorage::Load()
{
    QFile file(mFilename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open settings file: '%1'", mFilename);
        return false;
    }

    const QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
    if (doc.isNull())
    {
        ERROR("JSON parse error in file: '%1'", mFilename);
        return false;
    }
    mValues = doc.object().toVariantMap();
    return true;
}

bool Settings::JsonFileSettingsStorage::Save()
{
    QFile file(mFilename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open settings file: '%1'", mFilename);
        return false;
    }
    const QJsonObject& json = QJsonObject::fromVariantMap(mValues);
    const QJsonDocument docu(json);

    file.write(docu.toJson());
    file.close();
    return true;
}

} // namespace
