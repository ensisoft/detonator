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
#include <stack>

#include "base/threadpool.h"
#include "editor/app/workspace.h"
#include "editor/app/process.h"
#include "editor/app/eventlog.h"
#include "editor/app/ipc.h"
#include "editor/gui/appsettings.h"
#include "editor/gui/clipboard.h"
#include "editor/gui/mainwidget.h"

class FramelessWindow;

namespace app {
    class ResourceCache;
} // app

namespace gui
{
    class MainWidget;
    class Settings;
    class ChildWindow;
    class PlayWindow;
    class DlgImgPack;
    class DlgImgView;
    class DlgFontMap;
    class DlgSvgView;
    class DlgTilemap;

    // Main application window. Composes several MainWidgets
    // into a single cohesive window object that the user can
    // interact with.
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        MainWindow(QApplication& app, base::ThreadPool* threadpool);
       ~MainWindow();

        // Load the main editor settings.
        void LoadSettings();

        // Load previous application state.
        void LoadLastState(FramelessWindow* window);

        void LoadLastWorkspace();

        // Prepare (enumerate) the currently open MainWidgets in the window menu
        // and prepare window swapping shortcuts.
        void UpdateWindowMenu();

        // Show the application window.
        void showWindow();

        // Perform one iteration of the "game" loop, update and render
        // all currently open widgets.
        void RunGameLoopOnce();

        // Returns true when the window has been closed.
        bool IsClosed() const
        { return mIsClosed; }

        bool TryVSync() const
        { return mSettings.vsync; }

        unsigned GetFrameDelay() const
        { return mSettings.frame_delay; }

    private slots:
        void on_menuEdit_aboutToShow();
        void on_mainTab_currentChanged(int index);
        void on_mainTab_tabCloseRequested(int index);
        void on_actionClearGraphicsCache_triggered();
        void on_actionExit_triggered();
        void on_actionHelp_triggered();
        void on_actionAbout_triggered();
        void on_actionWindowClose_triggered();
        void on_actionWindowNext_triggered();
        void on_actionWindowPrev_triggered();
        void on_actionWindowPopOut_triggered();
        void on_actionTabClose_triggered();
        void on_actionTabPopOut_triggered();
        void on_actionCut_triggered();
        void on_actionCopy_triggered();
        void on_actionPaste_triggered();
        void on_actionUndo_triggered();
        void on_actionZoomIn_triggered();
        void on_actionZoomOut_triggered();
        void on_actionReloadShaders_triggered();
        void on_actionReloadTextures_triggered();
        void on_actionTakeScreenshot_triggered();
        void on_actionNewMaterial_triggered();
        void on_actionNewParticleSystem_triggered();
        void on_actionNewCustomShape_triggered();
        void on_actionNewEntity_triggered();
        void on_actionNewScene_triggered();
        void on_actionNewScript_triggered();
        void on_actionNewBlankScript_triggered();
        void on_actionNewEntityScript_triggered();
        void on_actionNewSceneScript_triggered();
        void on_actionNewUIScript_triggered();
        void on_actionNewAnimatorScript_triggered();
        void on_actionNewUI_triggered();
        void on_actionNewTilemap_triggered();
        void on_actionNewAudioGraph_triggered();
        void on_actionImportModel_triggered();
        void on_actionImportAudioFile_triggered();
        void on_actionImportImageFile_triggered();
        void on_actionImportTiles_triggered();
        void on_actionExportJSON_triggered();
        void on_actionImportJSON_triggered();
        void on_actionImportZIP_triggered();
        void on_actionExportZIP_triggered();
        void on_actionEditTags_triggered();
        void on_actionEditResource_triggered();
        void on_actionEditResourceNewWindow_triggered();
        void on_actionEditResourceNewTab_triggered();
        void on_actionDeleteResource_triggered();
        void on_actionRenameResource_triggered();
        void on_actionDuplicateResource_triggered();
        void on_actionDependencies_triggered();
        void on_actionSaveWorkspace_triggered();
        void on_actionLoadWorkspace_triggered();
        void on_actionNewWorkspace_triggered();
        void on_actionCloseWorkspace_triggered();
        void on_actionSettings_triggered();
        void on_actionImagePacker_triggered();
        void on_actionImageViewer_triggered();
        void on_actionSvgViewer_triggered();
        void on_actionFontMap_triggered();
        void on_actionTilemap_triggered();
        void on_actionImportProjectResource_triggered();
        void on_actionClearLog_triggered();
        void on_actionLogShowInfo_toggled(bool val);
        void on_actionLogShowWarning_toggled(bool val);
        void on_actionLogShowError_toggled(bool val);
        void on_eventlist_customContextMenuRequested(QPoint point);
        void on_workspace_customContextMenuRequested(QPoint);
        void on_workspace_doubleClicked();
        void on_workspaceFilter_textChanged(const QString&);
        void on_actionPackageResources_triggered();
        void on_actionSelectResourceForEditing_triggered();
        void on_actionCreateResource_triggered();
        void on_actionProjectSettings_triggered();
        void on_actionProjectPlay_triggered();
        void on_actionProjectPlayClean_triggered();
        void on_actionProjectSync_triggered();
        void on_btnDemoBandit_clicked();
        void on_btnDemoBlast_clicked();
        void on_btnDemoBreak_clicked();
        void on_btnDemoParticles_clicked();
        void on_btnDemoPlayground_clicked();
        void on_btnDemoUI_clicked();
        void on_btnDemoDerp_clicked();
        void on_btnDemoCharacter_clicked();
        void on_btnMaterial_clicked();
        void on_btnParticle_clicked();
        void on_btnShape_clicked();
        void on_btnEntity_clicked();
        void on_btnScene_clicked();
        void on_btnScript_clicked();
        void on_btnUI_clicked();
        void on_btnAudio_clicked();
        void on_btnTilemap_clicked();
        void actionWindowFocus_triggered();
        void RefreshUI();
        void ShowNote(const app::Event& event);
        void OpenExternalImage(const QString& file);
        void OpenExternalShader(const QString& file);
        void OpenExternalScript(const QString& file);
        void OpenExternalAudio(const QString& file);
        void OpenNewWidget(gui::MainWidget* widget);
        bool FocusWidget(const QString& id);
        void ActOnWidget(const QString& action);
        void RefreshWidget();
        void RefreshWidgetActions();
        void LaunchScript(const QString& id);
        void OpenResource(const QString& id);
        void OpenRecentWorkspace();
        void ToggleShowResource();
        void CleanGarbage();
        void ResourceLoaded(const app::Resource* resource);
        void ResourceUpdated(const app::Resource* resource);
        void ResourceAdded(const app::Resource* resource);
        void ResourceRemoved(const app::Resource* resource);
        void ViewerJsonMessageReceived(const QJsonObject& json);

    private:
        void CloseTab(int index);
        void FloatTab(int index);
        void LaunchGame(bool clean);
        void BuildRecentWorkspacesMenu();
        void SaveSettings();
        bool SaveState();
        void LoadDemoWorkspace(const QString& which);
        bool LoadWorkspace(const QString& dir);
        bool SaveWorkspace();
        void CloseWorkspace();
        void EditResources(bool open_new_window);
        void ShowHelpWidget();
        void ImportFiles(const QStringList& files);
        void UpdateStats();
        void FocusPreviousTab();
        void UpdateActions(MainWidget* widget);
        void UpdateMainToolbar();
        ChildWindow* ShowWidget(MainWidget* widget, bool new_window);
        MainWidget* MakeWidget(app::Resource::Type type, const app::Resource* resource = nullptr);

        using ScriptGen = QString(*)(QString);
        void GenerateNewScript(const QString& script_name, const QString& arg_name, ScriptGen gen);
        void DrawResourcePreview(gfx::Painter& painter, double secs);

    private:
        virtual bool event(QEvent* event)  override;
        virtual void closeEvent(QCloseEvent* event) override;
        virtual void dragEnterEvent(QDragEnterEvent* drag) override;
        virtual void dropEvent(QDropEvent* event) override;
        virtual bool eventFilter(QObject* destination, QEvent* event) override;

    private:
        Ui::MainWindow mUI;
        QMenu* mImportMenu = nullptr;
        QMenu* mCreateMenu = nullptr;
        QMenu* mTabMenu = nullptr;
    private:
        class GfxResourceLoader;

        QApplication& mApplication;
        // the refresh timer to do low frequency UI updates.
        QTimer mRefreshTimer;
        // current application settings that are not part of any
        // other state. Loaded on startup and saved on exit.
        AppSettings mSettings;
        // Current focused widget in the main tab.
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
        // The time accumulator for keeping track of partial updates.
        double mTimeAccum = 0.0;
        // List of recently opened workspaces.
        QStringList mRecentWorkspaces;
        // the child process for running the game
        // in a separate process (GameHost).
        app::Process mGameProcess;
        app::Process mViewerProcess;
        // Local host socket for communicating workspace
        // events to the child (client) game process over
        // ICP messaging.
        std::unique_ptr<app::IPCHost> mIPCHost;
        std::unique_ptr<app::IPCHost> mIPCHost2;
        // proxy model for filtering application event log
        app::EventLogProxy mEventLog;
        // the application's main clipboard.
        Clipboard mClipboard;
        // Default settings for the main widgets for visualization.
        MainWidget::UISettings mUISettings;
        // Non modal tool dialogs.
        std::unique_ptr<DlgImgPack> mDlgImgPack;
        std::unique_ptr<DlgImgView> mDlgImgView;
        std::unique_ptr<DlgFontMap> mDlgFontMap;
        std::unique_ptr<DlgSvgView> mDlgSvgView;
        std::unique_ptr<DlgTilemap> mDlgTilemap;
        using FocusStack = std::stack<QString>;
        // Tab focus stack for moving to the previous widget
        // when a widget is closed or popped off of the main panel
        FocusStack mFocusStack;
        // GFX Resource loader that is used when there's no workspace.
        std::unique_ptr<GfxResourceLoader> mLoader;

        std::unique_ptr<app::ResourceCache> mResourceCache;

        base::ThreadPool* mThreadPool = nullptr;

        struct Preview {
            std::string textureId;
            std::string resourceId;
            std::unique_ptr<gfx::Drawable> drawable;
            std::unique_ptr<gfx::Material> material;
            app::Resource::Type type;
        };
        Preview mPreview;

        FramelessWindow* mFramelessWindow = nullptr;
    };

} // namespace

