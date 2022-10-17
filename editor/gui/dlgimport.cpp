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
#  include <QMessageBox>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QString>
#include "warnpop.h"

#include <set>

#include "editor/gui/dlgimport.h"
#include "editor/gui/utility.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/eventlog.h"

namespace gui
{

DlgImport::DlgImport(QWidget* parent, app::Workspace* workspace)
    : QDialog(parent)
    , mWorkspace(workspace)
{
    mUI.setupUi(this);
    SetEnabled(mUI.btnImport, false);
}

DlgImport::~DlgImport()
{

}

void DlgImport::on_btnSelectFile_clicked()
{
    const auto& filename = QFileDialog::getOpenFileName(this,
        tr("Import resource(s) from Zip"),
        QString(""),
        tr("ZIP (*.zip)"));
    if (filename.isEmpty())
        return;

    auto zip = std::make_unique<app::ResourceArchive>();
    if (!zip->Open(filename))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to import resource(s) from the zip file.\n"
                       "Please see the application log for more details."));
        msg.exec();
        return;
    }

    mUI.list->clear();
    for (size_t i=0; i<zip->GetNumResources(); ++i)
    {
        const auto& resource = zip->GetResource(i);
        QListWidgetItem* item = new QListWidgetItem();
        item->setIcon(resource.GetIcon());
        item->setText(resource.GetName());
        mUI.list->addItem(item);
    }
    mZip = std::move(zip);
    SetValue(mUI.file, filename);
    SetEnabled(mUI.btnImport, true);
}

void DlgImport::on_btnImport_clicked()
{
    if (!mZip)
        return;

    AutoEnabler import_btn(mUI.btnImport);

    size_t import_count = 0;

    for (size_t i=0;  i<mZip->GetNumResources(); ++i)
    {
        const auto& resource = mZip->GetResource(i);
        if (const auto* previous = mWorkspace->FindResourceById(resource.GetId()))
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Question);
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            msg.setText(tr("A resource with this ID (%1, '%2') already exists in the workspace.\n"
                           "Do you want to overwrite it?").arg(previous->GetId()).arg(previous->GetName()));
            const auto ret = msg.exec();
            if (ret == QMessageBox::Cancel)
                return;
            else if (ret == QMessageBox::No)
                mZip->IgnoreResource(i);
            else import_count++;
        }
    }
    if (!mWorkspace->ImportResourceArchive(*mZip))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to import resources from zip.\n"
                       "Please see the application log for details."));
        msg.exec();
        return;
    }
    NOTE("Imported %1 resource(s) into workspace.", import_count);
    INFO("Imported %1 resource(s) into workspace.", import_count);
    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Information);
    msg.setStandardButtons(QMessageBox::Ok);
    msg.setText(tr("Imported %1 resources into workspace.").arg(import_count));
    msg.exec();
}

void DlgImport::on_btnClose_clicked()
{
    close();
}

} // namespace
