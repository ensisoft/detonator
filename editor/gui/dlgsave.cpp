// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

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
