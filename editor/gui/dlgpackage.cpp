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
#  include <QEventLoop>
#  include <QDir>
#include "warnpop.h"

#include "editor/gui/appsettings.h"
#include "editor/app/eventlog.h"
#include "editor/app/packing.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgpackage.h"
#include "editor/gui/dlgsettings.h"

namespace gui
{

DlgPackage::DlgPackage(QWidget* parent, gui::AppSettings& settings, app::Workspace& workspace)
    : QDialog(parent)
    , mSettings(settings)
    , mWorkspace(workspace)
{
    mUI.setupUi(this);

    const auto num_resources = mWorkspace.GetNumResources();
    for (size_t i=0; i<num_resources; ++i)
    {
        const auto& resource = mWorkspace.GetResource(i);
        if (resource.IsPrimitive())
            continue;
        const bool check = resource.GetProperty("checked_for_packing", true);
        QListWidgetItem* item = new QListWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(check ? Qt::Checked : Qt::Unchecked);
        item->setIcon(resource.GetIcon());
        item->setText(resource.GetName());
        item->setData(Qt::UserRole, quint64(i));
        mUI.listWidget->addItem(item);
    }
    QString path;
    GetProperty(workspace, "packing_param_pack_height", mUI.cmbPackHeight);
    GetProperty(workspace, "packing_param_pack_width", mUI.cmbPackWidth);
    GetProperty(workspace, "packing_param_max_tex_height", mUI.cmbMaxTexHeight);
    GetProperty(workspace, "packing_param_max_tex_width", mUI.cmbMaxTexWidth);
    GetProperty(workspace, "packing_param_tex_padding", mUI.spinTexPadding);
    GetProperty(workspace, "packing_param_combine_textures", mUI.chkCombineTextures);
    GetProperty(workspace, "packing_param_resize_large_textures", mUI.chkResizeTextures);
    GetProperty(workspace, "packing_param_delete_prev", mUI.chkDelete);
    GetProperty(workspace, "packing_param_write_config", mUI.chkWriteConfig);
    GetProperty(workspace, "packing_param_copy_native", mUI.chkCopyNative);
    GetProperty(workspace, "packing_param_copy_html5", mUI.chkCopyHtml5);
    GetProperty(workspace, "packing_param_generate_html5", mUI.chkGenerateHtml5);
    GetProperty(workspace, "packing_param_generate_html5_filesys", mUI.chkGenerateHtml5FS);
    GetProperty(workspace, "packing_param_output_dir", &path);
    if (path.isEmpty()) {
        path = app::JoinPath(workspace.GetDir(), "dist");
    } else {
        path = workspace.MapFileToFilesystem(path);
    }

    SetValue(mUI.editOutDir, path);
    mUI.progressBar->setVisible(false);

    connect(&mWorkspace, &app::Workspace::ResourcePackingUpdate,
            this, &DlgPackage::ResourcePackingUpdate);
}

void DlgPackage::on_btnSelectAll_clicked()
{
    for (int i=0; i<GetCount(mUI.listWidget); ++i)
    {
        QListWidgetItem* item = mUI.listWidget->item(i);
        item->setCheckState(Qt::Checked);
    }
}

void DlgPackage::on_btnSelectNone_clicked()
{
    for (int i=0; i<GetCount(mUI.listWidget); ++i)
    {
        QListWidgetItem* item = mUI.listWidget->item(i);
        item->setCheckState(Qt::Unchecked);
    }
}

void DlgPackage::on_btnBrowse_clicked()
{
    const auto& dir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory"), mWorkspace.GetDir());
    if (dir.isEmpty())
        return;

    mUI.editOutDir->setText(dir);
}
void DlgPackage::on_btnStart_clicked()
{
    if (!MustHaveInput(mUI.editOutDir))
        return;
    else if (!MustHaveNumber(mUI.cmbMaxTexHeight))
        return;
    else if (!MustHaveNumber(mUI.cmbMaxTexWidth))
        return;

    const QString& path = GetValue(mUI.editOutDir);
    QDir dir(path);
    if (dir.exists() && !dir.isEmpty() && GetValue(mUI.chkDelete))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("You've chosen to delete the previous contents of\n%1.\n"
                       "Are you sure you want to proceed?").arg(path));
        if (msg.exec() == QMessageBox::No)
            return;
    }
    else if (dir.exists() && !dir.isEmpty())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("The directory\n%1\ncontains files that might get overwritten.\n"
            "Are you sure you want to proceed?").arg(path));
        if (msg.exec() == QMessageBox::No)
            return;
    }
    if (GetValue(mUI.chkDelete))
    {
        dir.removeRecursively();
    }
    if (GetValue(mUI.chkGenerateHtml5FS))
    {
        if (mSettings.emsdk.isEmpty())
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Question);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setText(tr("You haven't given any Emscripten SDK path.\n"
                           "Emscripten SDK is needed in order to package the game content for the web."));
            //todo: open the settings maybe
            msg.exec();
            return;
        }
    }

    mUI.btnStart->setEnabled(false);
    mUI.btnClose->setEnabled(false);
    mUI.progressBar->setVisible(true);
    mPackageInProgress = true;

    std::vector<const app::Resource*> resources;

    for (size_t i=0; i<GetCount(mUI.listWidget); ++i)
    {
        const QListWidgetItem* item = mUI.listWidget->item(i);
        const auto index   = item->data(Qt::UserRole).toULongLong();
        const auto checked = item->checkState() == Qt::Checked;
        auto& resource = mWorkspace.GetResource(index);
        resource.SetProperty("checked_for_packing", checked);
        if (checked)
        {
            resources.push_back(&resource);
        }
    }
    // remember the settings.
    SetProperty(mWorkspace, "packing_param_pack_height", mUI.cmbPackHeight);
    SetProperty(mWorkspace, "packing_param_pack_width", mUI.cmbPackWidth);
    SetProperty(mWorkspace, "packing_param_max_tex_height", mUI.cmbMaxTexHeight);
    SetProperty(mWorkspace, "packing_param_max_tex_width", mUI.cmbMaxTexWidth);
    SetProperty(mWorkspace, "packing_param_tex_padding", mUI.spinTexPadding);
    SetProperty(mWorkspace, "packing_param_combine_textures", mUI.chkCombineTextures);
    SetProperty(mWorkspace, "packing_param_resize_large_textures", mUI.chkResizeTextures);
    SetProperty(mWorkspace, "packing_param_delete_prev", mUI.chkDelete);
    SetProperty(mWorkspace, "packing_param_write_config", mUI.chkWriteConfig);
    SetProperty(mWorkspace, "packing_param_copy_native", mUI.chkCopyNative);
    SetProperty(mWorkspace, "packing_param_copy_html5", mUI.chkCopyHtml5);
    SetProperty(mWorkspace, "packing_param_generate_html5", mUI.chkGenerateHtml5);
    SetProperty(mWorkspace, "packing_param_generate_html5_filesys", mUI.chkGenerateHtml5FS);
    SetProperty(mWorkspace, "packing_param_output_dir", mWorkspace.MapFileToWorkspace(path));

    app::Workspace::ContentPackingOptions options;
    options.directory                     = path;
    options.package_name                  = "pack0";
    options.combine_textures              = GetValue(mUI.chkCombineTextures);
    options.resize_textures               = GetValue(mUI.chkResizeTextures);
    options.texture_pack_height           = GetValue(mUI.cmbPackHeight);
    options.texture_pack_width            = GetValue(mUI.cmbPackWidth);
    options.max_texture_width             = GetValue(mUI.cmbMaxTexWidth);
    options.max_texture_height            = GetValue(mUI.cmbMaxTexHeight);
    options.write_config_file             = GetValue(mUI.chkWriteConfig);
    options.texture_padding               = GetValue(mUI.spinTexPadding);
    options.write_content_file            = true; // seems pointless to not write this ever
    options.copy_native_files             = GetValue(mUI.chkCopyNative);
    options.copy_html5_files              = GetValue(mUI.chkCopyHtml5);
    options.write_html5_game_file         = GetValue(mUI.chkGenerateHtml5);
    options.write_html5_content_fs_image  = GetValue(mUI.chkGenerateHtml5FS);
    options.python_executable             = mSettings.python_executable;
    options.emsdk_path                    = mSettings.emsdk;
    const auto success = mWorkspace.PackContent(resources, options);

    mUI.btnStart->setEnabled(true);
    mUI.btnClose->setEnabled(true);
    mUI.progressBar->setVisible(false);
    mPackageInProgress = false;

    if (success)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Information);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Success!\nHave a good day. :)"));
        msg.exec();
    }
    else
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Content packing completed with errors/warnings.\n"
                       "Please see the log for details."));
        msg.exec();
    }
}

void DlgPackage::on_btnClose_clicked()
{
    close();
}

void DlgPackage::closeEvent(QCloseEvent* event)
{
    if (mPackageInProgress)
    {
        event->ignore();
        return;
    }
    event->accept();
}

void DlgPackage::ResourcePackingUpdate(const QString& action, int step, int max)
{
    if (step > max)
        step = max;
    mUI.progressBar->setValue(step);
    mUI.progressBar->setMaximum(max);
    mUI.progressBar->setFormat(QString("%1 %p%").arg(action));

    QEventLoop footgun;
    footgun.processEvents();
}

} // namespace

