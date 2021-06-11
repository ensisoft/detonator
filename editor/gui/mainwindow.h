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
#  include <QtWidgets>
#  include <QTimer>
#  include <QElapsedTimer>
#  include <QStringList>
#  include <QProcess>
#  include "ui_mainwindow.h"
#include "warnpop.h"

#include <vector>
#include <memory>

#include "editor/app/workspace.h"
#include "editor/app/process.h"
#include "editor/app/eventlog.h"
#include "editor/app/ipc.h"
#include "editor/gui/appsettings.h"
#include "editor/gui/clipboard.h"

namespace gui
{
    class MainWidget;
    class Settings;
    class ChildWindow;
    class PlayWindow;

    // Main application window. Composes several MainWidgets
    // into a single cohesive window object that the user can
    // interact with.
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        MainWindow(QApplication& app);
       ~MainWindow();

        // Close the widget object and delete it.
        void closeWidget(MainWidget* widget);

        // Move focus of the application to this widget if the widget
        // is currently being shown. If the widget is not being shown
        // then does nothing.
        void focusWidget(const MainWidget* widget);

        // Load the MainWindow and all MainWidget states.
        void loadState();

        // Prepare (enumerate) the currently open MainWidgets in the window menu
        // and prepare window swapping shortcuts.
        void UpdateWindowMenu();

        // Show the application window.
        void showWindow();

        // Perform one iteration of the "game" loop, update and render
        // all currently open widgets.
        void iterateGameLoop();

        bool haveAcceleratedWindows() const;

        // Returns true when the window has been closed.
        bool isClosed() const
        { return mIsClosed; }

    signals:
        void newAcceleratedWindowOpen();
        void aboutToClose();

    private slots:
        void on_menuEdit_aboutToShow();
        void on_mainTab_currentChanged(int index);
        void on_mainTab_tabCloseRequested(int index);
        void on_actionExit_triggered();
        void on_actionAbout_triggered();
        void on_actionWindowClose_triggered();
        void on_actionWindowNext_triggered();
        void on_actionWindowPrev_triggered();
        void on_actionWindowPopOut_triggered();
        void on_actionCut_triggered();
        void on_actionCopy_triggered();
        void on_actionPaste_triggered();
        void on_actionUndo_triggered();
        void on_actionZoomIn_triggered();
        void on_actionZoomOut_triggered();
        void on_actionReloadShaders_triggered();
        void on_actionReloadTextures_triggered();
        void on_actionNewMaterial_triggered();
        void on_actionNewParticleSystem_triggered();
        void on_actionNewCustomShape_triggered();
        void on_actionNewEntity_triggered();
        void on_actionNewScene_triggered();
        void on_actionNewScript_triggered();
        void on_actionNewUI_triggered();
        void on_actionImportFiles_triggered();
        void on_actionExportJSON_triggered();
        void on_actionEditResource_triggered();
        void on_actionEditResourceNewWindow_triggered();
        void on_actionEditResourceNewTab_triggered();
        void on_actionDeleteResource_triggered();
        void on_actionDuplicateResource_triggered();
        void on_actionSaveWorkspace_triggered();
        void on_actionLoadWorkspace_triggered();
        void on_actionNewWorkspace_triggered();
        void on_actionCloseWorkspace_triggered();
        void on_actionSettings_triggered();
        void on_actionImagePacker_triggered();
        void on_actionClearLog_triggered();
        void on_actionLogShowInfo_toggled(bool val);
        void on_actionLogShowWarning_toggled(bool val);
        void on_actionLogShowError_toggled(bool val);
        void on_eventlist_customContextMenuRequested(QPoint point);
        void on_workspace_customContextMenuRequested(QPoint);
        void on_workspace_doubleClicked();
        void on_actionPackageResources_triggered();
        void on_actionSelectResourceForEditing_triggered();
        void on_actionNewResource_triggered();
        void on_actionProjectSettings_triggered();
        void on_actionProjectPlay_triggered();
        void on_btnBandit_clicked();
        void on_btnBlast_clicked();
        void on_btnMaterial_clicked();
        void on_btnParticle_clicked();
        void on_btnShape_clicked();
        void on_btnEntity_clicked();
        void on_btnScene_clicked();
        void on_btnScript_clicked();
        void on_btnUI_clicked();
        void actionWindowFocus_triggered();
        void RefreshUI();
        void ShowNote(const app::Event& event);
        void OpenExternalImage(const QString& file);
        void OpenExternalShader(const QString& file);
        void OpenExternalScript(const QString& file);
        void OpenNewWidget(MainWidget* widget);
        void OpenRecentWorkspace();
        void ToggleShowResource();

    private:
        void BuildRecentWorkspacesMenu();
        bool SaveState();
        void LoadDemoWorkspace(const QString& which);
        bool LoadWorkspace(const QString& dir);
        bool SaveWorkspace();
        void CloseWorkspace();
        void EditResources(bool open_new_window);
        bool FocusWidget(const QString& id);
        ChildWindow* ShowWidget(MainWidget* widget, bool new_window);
        void ShowHelpWidget();
        void ImportFiles(const QStringList& files);

    private:
        bool event(QEvent* event)  override;
        void closeEvent(QCloseEvent* event) override;
        void dragEnterEvent(QDragEnterEvent* drag) override;
        void dropEvent(QDropEvent* event) override;

    private:
        Ui::MainWindow mUI;

    private:
        QApplication& mApplication;
        // the refresh timer to do low frequency UI updates.
        QTimer mRefreshTimer;
        // current application settings that are not part of any
        // other state. Loaded on startup and saved on exit.
        AppSettings mSettings;
        // currently focused (current) widget in the main tab.
        MainWidget* mCurrentWidget = nullptr;
        // Filtering proxy for workspace.
        app::WorkspaceProxy mWorkspaceProxy;
        // workspace object.
        std::unique_ptr<app::Workspace> mWorkspace;
        // the list of child windows that are opened to show mainwidgets.
        // it's possible to open some resource in a separate window
        // in which case a new ChildWindow is opened to display/contain the MainWidget.
        // instead of the tab in this window.
        std::vector<ChildWindow*> mChildWindows;
        // The play window if any currently.
        std::unique_ptr<PlayWindow> mPlayWindow;
        // flag to indicate whether window has been closed or not
        bool mIsClosed = false;
        // total time measured in update steps frequency.
        double mTimeTotal = 0.0;
        // The time accumulator for keeping track of partial
        // updates.
        double mTimeAccum = 0.0;
        // List of recently opened workspaces.
        QStringList mRecentWorkspaces;
        // the child process for running the game
        // in a separate process (GameHost).
        app::Process mGameProcess;
        // Local host socket for communicating workspace
        // events to the child (client) game process over
        // ICP messaging.
        std::unique_ptr<app::IPCHost> mIPCHost;
        // proxy model for filtering application event log
        app::EventLogProxy mEventLog;
        // the application's main clipboard.
        Clipboard mClipboard;
    };

} // namespace

