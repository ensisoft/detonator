// Copyright (C) 2020-2022 Sami Väisänen
// Copyright (C) 2020-2022 Ensisoft http://www.ensisoft.com
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
#  include <QMainWindow>
#  include <QWindow>
#  include <QElapsedTimer>
#  include <QTimer>
#  include "ui_viewwindow.h"
#include "warnpop.h"

#include <memory>

#include "editor/gui/appsettings.h"
#include "editor/app/ipc.h"

namespace app {
    class Workspace;
} // namespace
namespace gui {
    class MainWidget;
} // namespace
namespace engine {
    class Engine;
} //

namespace gui
{
    class ViewWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        ViewWindow(QApplication& app);
       ~ViewWindow();

        bool IsClosed() const
        { return mClosed; }

        void Connect(const QString& local_ipc_socket);

        unsigned GetFrameDelay() const
        { return 1; }

        bool TryVSync() const
        { return false; }

    private slots:
        void on_btnDemoBandit_clicked();
        void on_btnDemoBlast_clicked();
        void on_btnDemoBreak_clicked();
        void on_btnDemoParticles_clicked();
        void on_btnDemoPlayground_clicked();
        void on_btnDemoUI_clicked();
        void on_btnDemoDerp_clicked();
        void on_btnSelectFile_clicked();
        void on_btnClose_clicked();
        void on_btnExport_clicked();

        void SelectResource(const QItemSelection&, const QItemSelection&);
        void JsonMessageReceived(const QJsonObject& json);

    private:
        virtual bool event(QEvent* event)  override;
        virtual void closeEvent(QCloseEvent* event) override;

        void IterateGameLoop();
        void ShutdownWidget();
        void RefreshUI();
        void SendWindowState();
        void LoadDemoWorkspace(const QString& name);
        void LoadWorkspace(const QString& dir);
    private:
        Ui::ViewWindow mUI;
    private:
        QApplication& mApp;
        // the refresh timer to do low frequency UI updates.
        QTimer mRefreshTimer;
        std::unique_ptr<app::Workspace> mWorkspace;
        gui::MainWidget* mCurrentWidget = nullptr;
        // total time measured in update steps frequency.
        double mTimeTotal = 0.0;
        // The time accumulator for keeping track of partial updates.
        double mTimeAccum = 0.0;
        // flag to indicate whether the window should be closed
        bool mClosed = false;

        AppSettings mSettings;

        app::IPCClient mClientSocket;
    };
} // namespace