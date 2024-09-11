// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgscriptvarname.h"
#  include <QDialog>
#  include <QString>
#include "warnpop.h"

#include <algorithm>

#include "editor/app/utility.h"
#include "editor/gui/utility.h"

namespace gui
{
    class DlgScriptVarName : public QDialog
    {
        Q_OBJECT

    public:
        explicit DlgScriptVarName(QWidget* parent, QString str, QString backup)
          : QDialog(parent)
          , mBackup(backup)
        {
            mUI.setupUi(this);
            SetValue(mUI.name, str);
            mUI.name->setCursorPosition(str.size());
            UpdateExample();
        }
        QString GetName() const
        {
            return GetValue(mUI.name);
        }

    private:
        void UpdateExample();

    private slots:
        void on_btnAccept_clicked()
        {
            accept();
        }
        void on_btnCancel_clicked()
        {
            reject();
        }
        void on_name_textEdited(const QString& text);

    private:
        Ui::DlgScriptVarName mUI;
        QString mBackup;
    };
} // namespace