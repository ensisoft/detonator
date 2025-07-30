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
#  include <QCoreApplication>
#  include <QStringList>
#  include <QFileDialog>
#  include <QMessageBox>
#  include <QEventLoop>
#  include <QDir>
#  include <QRegularExpression>
#include "warnpop.h"

#include "base/platform.h"
#include "editor/gui/appsettings.h"
#include "editor/app/eventlog.h"
#include "editor/app/packing.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgpackage.h"
#include "editor/gui/dlgsettings.h"
#include "editor/gui/dlgcomplete.h"

//#include "git.h"
extern "C" const char* git_CommitSHA1();
extern "C" const char* git_Branch();
extern "C" bool git_AnyUncommittedChanges();

namespace {
    bool VerifyWasmBuildVersion(const QString& wasm_version_file, QString* sha)
    {
        const auto& wasm_version_data = app::ReadTextFile(wasm_version_file);
        if (wasm_version_data.isEmpty())
            return false;

        const auto& lines = wasm_version_data.split("\n", Qt::SkipEmptyParts);
        int index = 0;
        for (index=0; index<lines.size(); ++index)
        {
            if (lines[index].contains("git_CommitSHA1"))
                break;
        }
        if (++index >= lines.size())
            return false;

        const QRegularExpression regex(R"(return\s*\"([a-fA-F0-9]{40})\";)");
        QRegularExpressionMatch match = regex.match(lines[index]);

        if (match.hasMatch())
        {
            *sha = match.captured(1);  // Capture group 1 (SHA-1 hash)
            return true;
        }
        return false;
    }
} // namespace

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

    bool copy_native = false;
    bool copy_html5  = false;
    GetProperty(workspace, "packing_param_copy_native", &copy_native);
    GetProperty(workspace, "packing_param_copy_html5",  &copy_html5);

#if defined(WINDOWS_OS)
    mUI.btnNative->setIcon(QIcon(":/logo/windows.png"));
#elif defined(LINUX_OS)
    mUI.btnNative->setIcon(QIcon(":/logo/linux.png"));
#endif
    SetValue(mUI.btnNative, copy_native);
    SetValue(mUI.btnHtml5, copy_html5);
    SetVisible(mUI.warning, false);

    const auto& project_settings = mWorkspace.GetProjectSettings();
    const auto wasm_version_file = mWorkspace.MapFileToFilesystem(project_settings.GetWasmEngineVersionFile());

    QString warning;
    QString wasm_sha;
    if (!VerifyWasmBuildVersion(wasm_version_file, &wasm_sha))
    {
        warning += "Failed to verify HTML5/WASM build version. Rebuild with Emscripten.\n";
    }
    else if (wasm_sha != git_CommitSHA1())
    {
        mWasmBuildWarning = true;
        warning += "Your HTML5/WASM build is outdated. Rebuild with Emscripten.\n";
    }
    if (mWorkspace.GetProjectSettings().log_debug)
    {
        warning += "Debug logging is enabled. This can cause slow performance.\n";
    }

    if (!warning.isEmpty())
    {
        SetVisible(mUI.warning, true);
        SetValue(mUI.message, warning);
    }
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

    if (mUI.btnHtml5->isChecked() && mWasmBuildWarning)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setWindowTitle("HTML5 WARNING");
        msg.setText(tr("Your HTML5/WASM engine build seems to be out of sync.\n"
                       "This can cause unexpected behaviour and failures.\n"
                       "You should rebuild the engine with Emscripten.\n\n"
                       "Are you sure you want to continue?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    const QString& path = GetValue(mUI.editOutDir);
    QDir dir(path);
    if (dir.exists() && !dir.isEmpty() && GetValue(mUI.chkDelete))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setWindowTitle("Delete Output Folder+");
        msg.setText(tr("You've chosen to delete the previous contents of\n%1.\n\n"
                       "Are you sure you want to proceed?").arg(path));
        if (msg.exec() == QMessageBox::No)
            return;
    }
    else if (dir.exists() && !dir.isEmpty())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("The directory\n%1\ncontains files that will get overwritten.\n\n"
            "Are you sure you want to proceed?").arg(path));
        if (msg.exec() == QMessageBox::No)
            return;
    }
    if (GetValue(mUI.chkDelete))
    {
        dir.removeRecursively();
    }
    if (GetValue(mUI.chkGenerateHtml5FS) && mUI.btnHtml5->isChecked())
    {
        if (mSettings.emsdk.isEmpty())
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setText(tr("You haven't given any Emscripten SDK path.\n"
                           "Emscripten SDK is needed in order to package the game content for the web.\n\n"
                           "You need to configure the Emscripten SDK in the settings."));
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
    SetProperty(mWorkspace, "packing_param_copy_native", mUI.btnNative->isChecked());
    SetProperty(mWorkspace, "packing_param_copy_html5", mUI.btnHtml5->isChecked());
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
    options.copy_native_files             = mUI.btnNative->isChecked();
    options.copy_html5_files              = mUI.btnHtml5->isChecked();
    options.write_html5_game_file         = GetValue(mUI.chkGenerateHtml5);
    options.write_html5_content_fs_image  = GetValue(mUI.chkGenerateHtml5FS);
    options.python_executable             = mSettings.python_executable;
    options.emsdk_path                    = mSettings.emsdk;
    const auto success = mWorkspace.BuildReleasePackage(resources, options, this);

    mUI.btnStart->setEnabled(true);
    mUI.btnClose->setEnabled(true);
    mUI.progressBar->setVisible(false);
    mPackageInProgress = false;

    if (success)
    {
        DlgComplete dlg(this, mWorkspace, options);
        dlg.exec();
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

void DlgPackage::ApplyPendingUpdates()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (const auto& update: mUpdateQueue)
        {
            auto step = update.current_step;
            auto max = update.step_count;
            if (step > max)
                step = max;

            mUI.progressBar->setMaximum(max);
            mUI.progressBar->setValue(step);
            mUI.progressBar->setFormat(update.msg + " %p%");
        }
        mUpdateQueue.clear();
    }

    qApp->processEvents();
}

} // namespace

