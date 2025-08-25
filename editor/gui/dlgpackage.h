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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_dlgpackage.h"
#  include <QDialog>
#include "warnpop.h"

#include <mutex>
#include <vector>
#include <variant>

#include "editor/app/workspace.h"
#include "editor/app/workspace_observer.h"

namespace gui
{
    struct AppSettings;

    class DlgPackage :  public QDialog,
                        public app::WorkspaceAsyncWorkObserver
    {
        Q_OBJECT
    public:
        DlgPackage(QWidget* parent, gui::AppSettings& settings, app::Workspace& workspace);

        void EnqueueUpdate(const app::AnyString& message, unsigned step_count, unsigned current_step) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            UpdateMessage msg;
            msg.msg = message;
            msg.step_count = step_count;
            msg.current_step = current_step;
            mUpdateQueue.push_back(msg);
        }

        void EnqueueUpdateMessage(const app::AnyString& msg) override {}
        void EnqueueStepReset(unsigned count) override {}
        void EnqueueStepIncrement() override {}
        void ApplyPendingUpdates() override;

    private slots:
        void on_btnSelectAll_clicked();
        void on_btnSelectNone_clicked();
        void on_btnBrowse_clicked();
        void on_btnStart_clicked();
        void on_btnClose_clicked();

    private:
        void closeEvent(QCloseEvent* event) override;
    private:
        Ui::DlgPackage mUI;
    private:
        struct UpdateMessage {
            QString msg;
            unsigned step_count;
            unsigned current_step;
        };

        gui::AppSettings& mSettings;
        app::Workspace& mWorkspace;
        std::mutex mMutex;
        std::vector<UpdateMessage> mUpdateQueue;
        bool mPackageInProgress = false;
        bool mWasmBuildWarning = false;
    };
} // namespace


