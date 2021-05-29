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
#  include "ui_dlgpackage.h"
#  include <QDialog>
#include "warnpop.h"

#include "editor/app/workspace.h"

namespace gui
{
    class DlgPackage :  public QDialog
    {
        Q_OBJECT
    public:
        DlgPackage(QWidget* parent, app::Workspace& workspace);

    private slots:
        void on_btnSelectAll_clicked();
        void on_btnSelectNone_clicked();
        void on_btnBrowse_clicked();
        void on_btnStart_clicked();
        void on_btnClose_clicked();
        void ResourcePackingUpdate(const QString& action, int step, int max);
    private:
        virtual void closeEvent(QCloseEvent* event) override;
    private:
        Ui::DlgPackage mUI;
    private:
        app::Workspace& mWorkspace;
        bool mPackageInProgress = false;
    };
} // namespace


