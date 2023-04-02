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

#include "warnpush.h"
#  include <QAbstractTableModel>
#include "warnpop.h"

#include <vector>

#include "base/utility.h"
#include "editor/app/process.h"
#include "editor/app/workspace.h"
#include "editor/app/format.h"
#include "editor/gui/dlgvcs.h"
#include "editor/gui/appsettings.h"
#include "editor/gui/utility.h"

namespace gui {
class DlgVCS::TableModel : public QAbstractTableModel
{
public:
    enum class SyncAction {
        None, Delete, Add, Commit
    };
    enum class FileStatus {
        None, Found, Failed, Success
    };

    struct FileResource {
        FileStatus status = FileStatus::None;
        SyncAction sync   = SyncAction::None;
        QString resource;
        QString file;
    };

    QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& file = mFiles[index.row()];
        if (role == Qt::DisplayRole)
        {
            if (index.column() == 0)  return app::toString(file.sync);
            else if (index.column() == 1) return app::toString(file.status);
            else if (index.column() == 2) return file.resource;
            else if (index.column() == 3) return file.file;
        }
        else if (role == Qt::DecorationRole && index.column() == 0)
        {
            if (file.sync == SyncAction::None)
                return QIcon("icons:transmit_blue.png");
            else if(file.sync == SyncAction::Commit)
                return QIcon("icons:transmit_edit.png");
            else if (file.sync == SyncAction::Add)
                return QIcon("icons:transmit_add.png");
            else if (file.sync == SyncAction::Delete)
                return QIcon("icons:transmit_delete.png");
        } else if (role == Qt::DecorationRole && index.column() == 1)
        {
            if (file.status == FileStatus::Success)
                return QIcon("icons:tick_ok.png");
            else if (file.status == FileStatus::Failed)
                return QIcon("icons:exclamation.png");
        }
        return {};
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0) return "Action";
            else if (section == 1) return "Status";
            else if (section == 2) return "Resource";
            else if (section == 3) return "File";
        }
        return {};
    }

    int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mFiles.size());
    }

    int columnCount(const QModelIndex&) const override
    {
        return 4;
    }

    void AddItem(FileResource&& res)
    {
        const auto row = static_cast<int>(mFiles.size());
        QAbstractTableModel::beginInsertRows(QModelIndex(),row, row);
        mFiles.push_back(std::move(res));
        QAbstractTableModel::endInsertRows();
    }
    void Update(const std::unordered_map<QString, SyncAction>& action_set)
    {
        for (auto& file : mFiles) {
            if (const auto* action = base::SafeFind(action_set, file.file))
                file.sync = *action;
        }
        emit dataChanged(index(0, 0), index(mFiles.size(), 4));
    }
    void Update(const std::unordered_map<QString, FileStatus>& status_set)
    {
        for (auto& file : mFiles) {
            if (const auto* status = base::SafeFind(status_set, file.file))
                file.status = *status;
        }
        emit dataChanged(index(0, 0), index(mFiles.size(), 4));
    }
    void Refresh()
    {
        emit dataChanged(index(0, 0), index(mFiles.size(), 4));
    }
    FileResource& GetItem(size_t index)
    { return mFiles[index]; }
    std::vector<FileResource>& GetItems()
    { return mFiles; }

private:
    std::vector<FileResource> mFiles;
};


DlgVCS::DlgVCS(QWidget* parent,
               const app::Workspace* workspace,
               const gui::AppSettings& settings)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mSettings(settings)
{
    mModel.reset(new TableModel);

    mUI.setupUi(this);
    mUI.tableView->setModel(mModel.get());

    BeginScan();
}

DlgVCS::~DlgVCS() = default;

void DlgVCS::on_btnClose_clicked()
{
    close();
}
void DlgVCS::on_btnSync_clicked()
{
    SetEnabled(mUI.btnSync, false);

    for (auto& file : mModel->GetItems())
    {
        QString cmd;
        if (file.sync == TableModel::SyncAction::Commit)
            cmd = mSettings.vcs_cmd_commit_file;
        else if (file.sync == TableModel::SyncAction::Delete)
            cmd = mSettings.vcs_cmd_del_file;
        else if (file.sync == TableModel::SyncAction::Add)
            cmd = mSettings.vcs_cmd_add_file;
        else if (file.sync == TableModel::SyncAction::None)
            continue;

        QStringList arg_list;
        QStringList tok_list = cmd.split(" ", Qt::SkipEmptyParts);
        for (const auto& tok : tok_list) {
            if (tok == "${workspace}")
                arg_list << mWorkspace->GetDir();
            else if (tok == "${file}")
                arg_list << file.file;
            else arg_list << tok;
        }
        QStringList stdout_buffer;
        QStringList stderr_buffer;
        if (!RunCommand(arg_list, &stdout_buffer, &stderr_buffer))
        {
            file.status = TableModel::FileStatus::Failed;
            continue;
        }
        file.status = TableModel::FileStatus::Success;
        file.sync   = TableModel::SyncAction::None;
    }
    mModel->Refresh();
    SetEnabled(mUI.btnSync, true);
}

void DlgVCS::AppendLog(const QString& str)
{
    mUI.plainTextEdit->appendPlainText(str);
}

void DlgVCS::BeginScan()
{
    SetEnabled(mUI.btnSync, false);
    AppendLog("Begin resource scan ...");

    // add these special files.
    {
        TableModel::FileResource file;
        file.status = TableModel::FileStatus::Found;
        file.sync   = TableModel::SyncAction::Commit;
        file.file   = "content.json";
        mModel->AddItem(std::move(file));
    }
    // add these special files.
    {
        TableModel::FileResource file;
        file.status = TableModel::FileStatus::Found;
        file.sync   = TableModel::SyncAction::Commit;
        file.file   = "workspace.json";
        mModel->AddItem(std::move(file));
    }

    std::unordered_set<QString> uri_set;
    std::unordered_set<QString> vcs_set;

    // find our set of files that are used by our user defined resources.

    for (size_t i=0; i<mWorkspace->GetNumUserDefinedResources(); ++i)
    {
        const auto& res = mWorkspace->GetUserDefinedResource(i);
        AppendLog(app::toString("Scanning '%1'", res.GetName()));

        const QStringList& uris = mWorkspace->ListFileResources(i);
        for (QString uri : uris)
        {
            if (uri_set.find(uri) != uri_set.end())
                continue;

            AppendLog(app::toString("Found resource file '%1'", uri));

            if (uri.startsWith("ws://"))
            {
                uri = uri.mid(5);
                uri_set.insert(uri);
                TableModel::FileResource file;
                file.status = TableModel::FileStatus::Found;
                file.sync = TableModel::SyncAction::None;
                file.file = uri;
                file.resource = res.GetName();
                mModel->AddItem(std::move(file));
            }
        }
    }

    // list files currently in the version control
    QStringList arg_list;
    QStringList tok_list = mSettings.vcs_cmd_list_files.split(" ", Qt::SkipEmptyParts);
    for (const auto& tok : tok_list) {
        if (tok == "${workspace}")
            arg_list << mWorkspace->GetDir();
        else arg_list << tok;
    }
    QStringList stdout_buffer;
    QStringList stderr_buffer;
    if (!RunCommand(arg_list, &stdout_buffer, &stderr_buffer))
        return;

    // 1. any file that is in the uri_set but not in the vcs_set needs to be added.
    // 2. any file that is in the vcs_set but not in the uri_set needs to be removed.
    // 3. any file that is changed needs to be committed

    const QStringList& ignore_list = mSettings.vcs_ignore_list.split(",", Qt::SkipEmptyParts);

    // okay.. stderr, weird?
    for (auto vcs_file : stderr_buffer)
    {
        AppendLog(app::toString("Found VCS file: '%1'", vcs_file));
        vcs_file.replace("\\", "//"); // Windows-ism?

        // should we ignore this file?
        bool ignore_this_file = false;
        for (const auto& ignore : ignore_list) {
            if (vcs_file.contains(ignore, Qt::CaseInsensitive))
            {
                ignore_this_file = true;
                break;
            }
        }
        if (ignore_this_file)
        {
            AppendLog(app::toString("File is ignored. '%1'", vcs_file));
            continue;
        }

        vcs_set.insert(vcs_file);

        // if the file is no longer in the set of resource files we're
        // adding an action for deleting it.
        if (!base::Contains(uri_set, vcs_file))
        {
            TableModel::FileResource file;
            file.sync = TableModel::SyncAction::Delete;
            file.status = TableModel::FileStatus::Found;
            file.file = vcs_file;
            mModel->AddItem(std::move(file));
        }
    }

    std::unordered_map<QString, TableModel::SyncAction> actions;

    for (const auto& uri_file : uri_set)
    {
        if (base::Contains(vcs_set, uri_file))
        {
            // could check here if the there's an actual change.
            actions[uri_file] = TableModel::SyncAction::Commit;
        }
        else
        {
            actions[uri_file] = TableModel::SyncAction::Add;
        }
    }
    mModel->Update(actions);


    SetEnabled(mUI.btnSync, true);
}

bool DlgVCS::RunCommand(const QStringList& arg_list,
                        QStringList* stdout_buffer,
                        QStringList* stderr_buffer)
{
    AppendLog(app::toString("Running command '%1 %2'",  mSettings.vcs_executable, arg_list.join(' ')));

    app::Process::Error error;
    int exit_code = 0;
    const bool silent_mode = true;
    if (!app::Process::RunAndCapture(mSettings.vcs_executable, mWorkspace->GetDir(), arg_list, stdout_buffer, stderr_buffer, &error, &exit_code, silent_mode))
    {
        AppendLog(app::toString("Failed to run command. error = %1", error));
        AppendLog(app::toString("  executable  = %1", mSettings.vcs_executable));
        AppendLog(app::toString("  working dir = %1", mWorkspace->GetDir()));
        AppendLog(app::toString("  arguments   = %1", arg_list.join(' ')));
        return false;
    }
    if (exit_code)
    {
        AppendLog(app::toString("Command failed with exit_code = %1", exit_code));
        AppendLog(stdout_buffer->join("\n"));
        AppendLog(stderr_buffer->join("\n"));
        return false;
    }
    return true;
}

} // namespace