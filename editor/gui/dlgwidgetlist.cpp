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

#include "config.h"

#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgwidgetlist.h"
#include "uikit/widget.h"

namespace gui
{
DlgWidgetList::DlgWidgetList(QWidget* parent, std::vector<uik::Widget*>& list)
  : QDialog(parent)
  , mList(list)
{
    mUI.setupUi(this);

    for (unsigned i=0; i<mList.size(); ++i)
    {
        const auto* widget = mList[i];
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(app::toString("%1. %2", i, widget->GetName()));
        item->setData(Qt::UserRole, i);
        mUI.listWidget->addItem(item);
    }
}

void DlgWidgetList::on_btnAccept_clicked()
{
    // the user can re-arrange the widgets though the ListWidget
    // by dragging and dropping the items in place.
    // create the new order of the widgets based on the items
    // in the list widget.
    std::vector<uik::Widget*> widgets;

    for (unsigned i=0; i<GetCount(mUI.listWidget); ++i)
    {
        const QListWidgetItem* item = mUI.listWidget->item(i);
        const unsigned index = item->data(Qt::UserRole).toUInt();
        uik::Widget* widget = mList[index];
        widgets.push_back(widget);
    }
    mList = std::move(widgets);

    accept();
}
void DlgWidgetList::on_btnCancel_clicked()
{
    reject();
}

} // namespace


