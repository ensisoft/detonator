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
#  include <QStringList>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgsave.h"
#include "editor/gui/mainwidget.h"

namespace gui
{

DlgSave::DlgSave(QWidget* parent, std::vector<MainWidget*>& widgets)
  : QDialog(parent)
  , mWidgets(widgets)
{
    mUI.setupUi(this);
    for (size_t i=0; i<mWidgets.size(); ++i)
    {
        auto* widget = mWidgets[i];
        QListWidgetItem* item = new QListWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setText(widget->windowTitle());
        item->setIcon(widget->windowIcon());
        item->setData(Qt::UserRole, (quint64)i);
        mUI.listWidget->addItem(item);
    }
}
bool DlgSave::SaveAutomatically() const
{
    return GetValue(mUI.chkSaveAutomatically);
}
void DlgSave::on_btnSelectAll_clicked()
{
    for (int i=0; i<GetCount(mUI.listWidget); ++i)
    {
        QListWidgetItem* item = mUI.listWidget->item(i);
        item->setCheckState(Qt::Checked);
    }
}
void DlgSave::on_btnSelectNone_clicked()
{
    for (int i=0; i<GetCount(mUI.listWidget); ++i)
    {
        QListWidgetItem* item = mUI.listWidget->item(i);
        item->setCheckState(Qt::Unchecked);
    }
}
void DlgSave::on_btnAccept_clicked()
{
    for (int i=0; i<GetCount(mUI.listWidget); ++i)
    {
        QListWidgetItem* item = mUI.listWidget->item(i);
        if (item->checkState() == Qt::Unchecked)
            continue;
        mWidgets[i]->Save();
    }
    accept();
}
void DlgSave::on_btnCancel_clicked()
{
    reject();
}

} // namespace
