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
    mProxy.setSourceModel(&mWorkspace);
    mProxy.SetModel(&mWorkspace);
    mUI.setupUi(this);
    mUI.tableView->setModel(&mProxy);
    SelectRow(mUI.tableView, 0);

    // capture some special key presses in order to move the
    // selection (current item) in the list widget more conveniently.
    mUI.filter->installEventFilter(this);
}

app::Resource* DlgOpen::GetSelected()
{
    const auto& index = GetSelectedIndex(mUI.tableView);
    if (!index.isValid())
        return nullptr;
    return &mWorkspace.GetResource(index.row());
}

void DlgOpen::SetOpenMode(const QString& mode)
{
    SetValue(mUI.cmbOpenMode, mode);
}

QString DlgOpen::GetOpenMode() const
{
    return GetValue(mUI.cmbOpenMode);
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
    mProxy.SetFilterString(text);
    mProxy.invalidate();
    SelectRow(mUI.tableView, 0);
}

void DlgOpen::on_tableView_doubleClicked(const QModelIndex&)
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
    else if (mWorkspace.GetNumUserDefinedResources() == 0)
        return false;

    const auto* key = static_cast<QKeyEvent*>(event);
    const bool ctrl = key->modifiers() & Qt::ControlModifier;
    const bool shift = key->modifiers() & Qt::ShiftModifier;

    if (shift)
    {
        auto index = mUI.cmbOpenMode->currentIndex();
        index = math::wrap(0, 1, index + 1);
        mUI.cmbOpenMode->setCurrentIndex(index);
        return true;
    }

    int current = GetSelectedRow(mUI.tableView);

    const auto max = GetCount(mUI.tableView);

    if (ctrl && key->key() == Qt::Key_N)
        current = math::wrap(0, max-1, current+1);
    else if (ctrl && key->key() == Qt::Key_P)
        current = math::wrap(0, max-1, current-1);
    else if (key->key() == Qt::Key_Up)
        current = math::wrap(0, max-1, current-1);
    else if (key->key() == Qt::Key_Down)
        current = math::wrap(0, max-1, current+1);
    else return false;

    SelectRow(mUI.tableView, current);
    return true;
}

} // namespace

