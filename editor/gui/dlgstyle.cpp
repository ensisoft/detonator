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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QFile>
#  include <QMessageBox>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/gui/dlgstyle.h"
#include "editor/gui/utility.h"

namespace gui
{
DlgStyleEditor::DlgStyleEditor(QWidget* parent)  : QDialog(parent)
{
    mUI.setupUi(this);

    std::vector<ListItem> items;
    items.push_back({"Widget", "widget"});
    items.push_back({"  Checkbox", "checkbox"});
    items.push_back({"  Form", "form"});
    items.push_back({"  Groupbox", "groupbox"});
    items.push_back({"  Label", "label"});
    items.push_back({"  PushButton", "push-button"});
    SetList(mUI.cmbWidget, items);
    SetValue(mUI.cmbWidget, "Widget");
    SetEnabled(mUI.cmbWidget, false);
    SetEnabled(mUI.tabStyles, false);
    SetEnabled(mUI.btnSave, false);
    SetEnabled(mUI.btnSaveAs, false);
    SetEnabled(mUI.btnOpen, true);
    SetEnabled(mUI.btnClose, true);

    mUI.normal->SetPropertySelector("");
    mUI.disabled->SetPropertySelector("/disabled");
    mUI.focused->SetPropertySelector("/focused");
    mUI.moused->SetPropertySelector("/mouse-over");
    mUI.pressed->SetPropertySelector("/pressed");

    mUI.normal->SetStyle(&mStyle);
    mUI.disabled->SetStyle(&mStyle);
    mUI.focused->SetStyle(&mStyle);
    mUI.moused->SetStyle(&mStyle);
    mUI.pressed->SetStyle(&mStyle);
}

void DlgStyleEditor::OpenStyleFile(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open style file: '%1' (%2)", filename, file.error());
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to open style file.\n%1").arg(file.errorString()));
        msg.exec();
        return;
    }
    const auto& buff = file.readAll();
    if (buff.isEmpty())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Unable to load the style because no JSON content was found in style file."));
        msg.exec();
        ERROR("No style JSON content found in file: '%1'", filename);
        return;
    }

    // Warning about nlohmann::json
    // !! SEMANTICS CHANGE BETWEEN DEBUG AND RELEASE BUILD !!
    //
    // Trying to access an attribute using operator[] does not check
    // whether a given key exists. instead it uses a standard CRT assert
    // which then changes semantics depending whether NDEBUG is defined
    // or not.

    const auto* beg  = buff.data();
    const auto* end  = buff.data() + buff.size();
    auto [ok, json, error] = base::JsonParse(beg, end);
    if (!ok)
    {
        ERROR("JSON parse error: '%1' in file: '%2'", error, filename);
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Unable to load the style because JSON parse error occurred."));
        msg.exec();
        return;
    }

    game::UIStyle style;

    if (!style.LoadStyle(json))
    {
        WARN("Errors were found while parsing the style.");
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("Errors were found while parsing the style settings.\n"
                       "Styling might be incomplete or unusable.\n"
                       "Do you want to continue?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    mStyle = std::move(style);
    SetValue(mUI.fileName, filename);
    SetEnabled(mUI.cmbWidget, true);
    SetEnabled(mUI.tabStyles, true);
    SetEnabled(mUI.btnSave, true);
    SetEnabled(mUI.btnSaveAs, true);
    SetEnabled(mUI.btnOpen, true);
    SetEnabled(mUI.btnClose, true);

    on_cmbWidget_currentIndexChanged(0);
}

void DlgStyleEditor::on_cmbWidget_currentIndexChanged(int)
{
    const std::string& klass = GetItemId(mUI.cmbWidget);
    mUI.normal->SetWidgetClass(klass);
    mUI.disabled->SetWidgetClass(klass);
    mUI.focused->SetWidgetClass(klass);
    mUI.pressed->SetWidgetClass(klass);
    mUI.moused->SetWidgetClass(klass);
}

void DlgStyleEditor::on_btnOpen_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select JSON Style File"), "",
        tr("Style (*.json)"));
    if (file.isEmpty())
        return;
    OpenStyleFile(file);
}
void DlgStyleEditor::on_btnSave_clicked()
{

}
void DlgStyleEditor::on_btnSaveAs_clicked()
{

}
void DlgStyleEditor::on_btnClose_clicked()
{
    close();
}

} // namespace


