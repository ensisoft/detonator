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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include "editor/gui/dlgresdeps.h"
#include "editor/gui/utility.h"

namespace gui
{
DlgResourceDeps::DlgResourceDeps(QWidget* parent, app::Workspace& workspace)
  : QDialog(parent)
  , mWorkspace(workspace)
{
    mUI.setupUi(this);
    mUI.tableResources->setModel(&mResources);
    mUI.tableUsers->setModel(&mUsers);
    mUI.tableDependents->setModel(&mDependencies);

    mResources.SetList(mWorkspace.ListUserDefinedResources());

    Connect(mUI.tableResources, this, &DlgResourceDeps::SelectedResourceChanged);
}

void DlgResourceDeps::SelectItem(const app::Resource& item)
{
    const auto& resources = mResources.GetList();
    for (size_t i=0; i<resources.size(); ++i)
    {
        if (item.GetId() == resources[i].id)
        {
            SelectRow(mUI.tableResources, i);
            Update();
            break;
        }
    }
}

void DlgResourceDeps::SelectItem(const app::AnyString& id)
{
    const auto& resources = mResources.GetList();
    for (size_t i=0; i<resources.size(); ++i)
    {
        if (id == resources[i].id)
        {
            SelectRow(mUI.tableResources, i);
            Update();
            break;
        }
    }
}

void DlgResourceDeps::on_btnClose_clicked()
{
    close();
}

void DlgResourceDeps::SelectedResourceChanged(const QItemSelection&, const QItemSelection&)
{
    Update();
}

void DlgResourceDeps::on_tableUsers_doubleClicked()
{
    const auto row = GetSelectedRow(mUI.tableUsers);
    if (row == -1)
        return;

    const auto& users = mUsers.GetList();
    SelectItem(users[row].id);

}

void DlgResourceDeps::on_tableDependents_doubleClicked()
{
    const auto row = GetSelectedRow(mUI.tableDependents);
    if (row == -1)
        return;

    const auto& deps = mDependencies.GetList();
    SelectItem(deps[row].id);
}

void DlgResourceDeps::Update()
{
    const auto row = GetSelectedRow(mUI.tableResources);
    if (row == -1)
    {
        mUsers.Clear();
        mDependencies.Clear();
        return;
    }
    mUsers.SetList(mWorkspace.ListResourceUsers(row));
    mDependencies.SetList(mWorkspace.ListDependencies(row));
}

} // namespace