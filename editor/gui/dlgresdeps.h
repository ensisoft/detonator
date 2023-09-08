// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgresdeps.h"
#  include <QDialog>
#include "warnpop.h"

#include "editor/app/workspace.h"

namespace gui
{
    class DlgResourceDeps :  public QDialog
    {
        Q_OBJECT

    public:
        DlgResourceDeps(QWidget* parent, app::Workspace& workspace);

        void SelectItem(const app::Resource& item);
        void SelectItem(const app::AnyString& id);

    private slots:
        void on_btnClose_clicked();

        void on_tableUsers_doubleClicked();
        void on_tableDependents_doubleClicked();
        void SelectedResourceChanged(const QItemSelection&, const QItemSelection&);

    private:
        void Update();

    private:
        Ui::DlgResourceDeps mUI;
    private:
        app::Workspace& mWorkspace;
        app::ResourceListModel mUsers;
        app::ResourceListModel mResources;
        app::ResourceListModel mDependencies;

    };
} // namespace


