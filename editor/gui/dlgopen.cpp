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
#  include <QFileDialog>
#  include <QMessageBox>
#  include <QDir>
#include "warnpop.h"

#include "base/math.h"
#include "editor/app/eventlog.h"
#include "editor/app/packing.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgopen.h"

namespace gui
{

DlgOpen::DlgOpen(QWidget* parent, app::Workspace& workspace)
    : QDialog(parent)
    , mWorkspace(workspace)
{
    mUI.setupUi(this);

    const auto num_resources = mWorkspace.GetNumResources();
    for (size_t i=0; i<num_resources; ++i)
    {
        const auto& resource = mWorkspace.GetResource(i);
        if (resource.IsPrimitive())
            continue;
        QListWidgetItem* item = new QListWidgetItem();
        item->setIcon(resource.GetIcon());
        item->setText(resource.GetName());
        item->setData(Qt::UserRole, (unsigned)i);
        mUI.listWidget->addItem(item);
    }

    if (num_resources)
    {
        mUI.listWidget->setCurrentItem(mUI.listWidget->item(0));
    }
    // capture some special key presses in order to move the
    // selection (current item) in the list widget more conveniently.
    mUI.filter->installEventFilter(this);
}

app::Resource* DlgOpen::GetSelected()
{
    QList<QListWidgetItem*> items = mUI.listWidget->selectedItems();
    if (items.isEmpty())
        return nullptr;

    const auto index = items[0]->data(Qt::UserRole).toUInt();
    return &mWorkspace.GetResource(index);
}

void DlgOpen::on_btnAccept_clicked()
{
    accept();
}

void DlgOpen::on_btnCancel_clicked()
{
    reject();
}

void DlgOpen::on_filter_textChanged(const QString& text)
{
    // shitty but ok, just rebuild the tree.
    mUI.listWidget->clear();

    unsigned matches = 0;

    const auto num_resources = mWorkspace.GetNumResources();
    for (size_t i=0; i<num_resources; ++i)
    {
        const auto& resource = mWorkspace.GetResource(i);
        if (resource.IsPrimitive())
            continue;
        const auto& name = resource.GetName().toLower();
        if (name.contains(text))
        {
            QListWidgetItem* item = new QListWidgetItem();
            item->setIcon(resource.GetIcon());
            item->setText(resource.GetName());
            item->setData(Qt::UserRole, (unsigned)i);
            mUI.listWidget->addItem(item);
            matches++;
        }
    }
    if (matches)
    {
        mUI.listWidget->setCurrentItem(mUI.listWidget->item(0));
    }
}

void DlgOpen::on_listWidget_itemDoubleClicked(QListWidgetItem*)
{
    accept();
}

bool DlgOpen::eventFilter(QObject* destination, QEvent* event)
{
    // returning true will eat the event and stop from
    // other event handlers ever seeing the event.
    if (destination != mUI.filter)
        return false;
    else if (event->type() != QEvent::KeyPress)
        return false;
    else if (mUI.listWidget->count() == 0)
        return false;

    const auto* key = static_cast<QKeyEvent*>(event);
    const bool ctrl = key->modifiers() & Qt::ControlModifier;

    int current = mUI.listWidget->currentIndex().row();
    if (current == -1)
        current = 0;

    if (ctrl && key->key() == Qt::Key_N)
        current = math::wrap(0, mUI.listWidget->count()-1, current+1);
    else if (ctrl && key->key() == Qt::Key_P)
        current = math::wrap(0, mUI.listWidget->count()-1, current-1);
    else if (key->key() == Qt::Key_Up)
        current = math::wrap(0, mUI.listWidget->count()-1, current-1);
    else if (key->key() == Qt::Key_Down)
        current = math::wrap(0, mUI.listWidget->count()-1, current+1);
    else return false;

    mUI.listWidget->setCurrentItem(mUI.listWidget->item(current));
    return true;
}

} // namespace

