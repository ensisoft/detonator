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

#include "editor/gui/dlgimport.h"
#include "editor/gui/utility.h"
#include "editor/app/format.h"

namespace gui
{

DlgImport::DlgImport(QWidget* parent, app::Workspace& workspace,
                     std::vector<std::unique_ptr<app::Resource>>& resources)
    : QDialog(parent)
    , mWorkspace(workspace)
    , mResources(resources)
{
    mUI.setupUi(this);
    mUI.tableWidget->setRowCount(resources.size());
    mUI.tableWidget->setColumnCount(2);

    for (size_t i=0; i<mResources.size(); ++i)
    {
        const auto& res = mResources[i];
        QTableWidgetItem* item = new QTableWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setIcon(res->GetIcon());
        item->setText(app::toString(res->GetType()));
        mUI.tableWidget->setItem(i, 0, item);
        item = new QTableWidgetItem();
        item->setText(res->GetName());
        mUI.tableWidget->setItem(i, 1, item);
    }

    QStringList header;
    header << "Type";
    header << "Name";
    mUI.tableWidget->setHorizontalHeaderLabels(header);
}

bool DlgImport::DontAskAgain() const
{
    return GetValue(mUI.checkBox);
}

void DlgImport::on_btnSelectAll_clicked()
{
    for (size_t i=0; i<GetCount(mUI.tableWidget); ++i)
    {
        auto* item = mUI.tableWidget->item(i, 0);
        item->setCheckState(Qt::Checked);
    }
}
void DlgImport::on_btnSelectNone_clicked()
{
    for (size_t i=0; i<GetCount(mUI.tableWidget); ++i)
    {
        auto* item = mUI.tableWidget->item(i, 0);
        item->setCheckState(Qt::Unchecked);
    }
}
void DlgImport::on_btnImport_clicked()
{
    for (size_t i=0; i<GetCount(mUI.tableWidget); ++i)
    {
        auto* item = mUI.tableWidget->item(i, 0);
        if (item->checkState() == Qt::Checked)
            mWorkspace.SaveResource(std::move(mResources[i]));
    }
    accept();
}
void DlgImport::on_btnCancel_clicked()
{
    reject();
}

} // namespace
