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
#  include <QMainWindow>
#  include <QWindow>
#  include <QOpenGLContext>
#  include <QLibrary>
#  include <QElapsedTimer>
#  include <QTimer>
#  include "ui_playwindow.h"
#include "warnpop.h"

#include <memory>

#include "engine/main/interface.h"
#include "editor/app/eventlog.h"

namespace app {
    class Workspace;
} // namespace

namespace game {
    class App;
} //

namespace gui
{
    // PlayWindow is an OpenGL enabled window that will display
    // and host the game/app that is loaded from an app library.
    class PlayWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        PlayWindow(app::Workspace& workspace);
       ~PlayWindow();

        // Returns true if the user has closed the window.
        bool IsClosed() const
        { return mClosed; }

        // Begin an iteration of the mainloop.
        void BeginMainLoop();

        // Iterate the app once.
        void RunOnce();

        // Do periodic low frequency tick.
        void NonGameTick();

        // End an iteration of the mainloop.
        void EndMainLoop();

        // Load the game library and launch the game.
        // Returns true if successful or false if some problem happened.
        bool LoadGame();

        // Shut down the game and unload the library.
        void Shutdown();

        // Load the window state (window size/position
        // and visibility of status bar and dock widget) from the
        // current Workspace.
        void LoadState();

        // Save the current window state (window size/position
        // and visibility of status bar and dock widget) in the
        // current Workspace.
        void SaveState();
    private slots:
        void DoAppInit();
        void ActivateWindow();
        void on_actionPause_triggered();
        void on_actionClose_triggered();
        void on_actionClearLog_triggered();
        void on_actionLogShowDebug_toggled(bool val);
        void on_actionLogShowInfo_toggled(bool val);
        void on_actionLogShowWarning_toggled(bool val);
        void on_actionLogShowError_toggled(bool val);
        void on_actionToggleDebugDraw_toggled();
        void on_actionToggleDebugLog_toggled();
        void on_actionFullscreen_triggered();
        void on_actionScreenshot_triggered();
        void on_btnApplyFilter_clicked();
        void on_log_customContextMenuRequested(QPoint point);

    private:
        virtual void closeEvent(QCloseEvent* event) override;
        virtual bool eventFilter(QObject* destination, QEvent* event) override;
        void ResizeSurface(unsigned width, unsigned height);
        void AskSetFullScreen(bool fullscreen);
        void AskToggleFullScreen();
        bool InFullScreen() const;
        void SetFullScreen(bool fullscreen);
        void SetDebugOptions() const;
        void Barf(const std::string& msg);

    private:
        class WindowContext;
        class ResourceLoader;
        class SessionLogger;
    private:
        Ui::PlayWindow mUI;
    private:
        // our current workspace.
        app::Workspace& mWorkspace;
        // This process's working directory.
        QString mHostWorkingDir;
        // the working directory that we have to change
        // to before calling into app in case it refers to
        // data/resources relative to the working directory.
        QString mGameWorkingDir;
        // loader for the game/app library.
        QLibrary mLibrary;
        // entry point functions into the game/app library.
        Gamestudio_CreateAppFunc       mGameLibCreateApp = nullptr;
        Gamestudio_SetGlobalLoggerFunc mGameLibSetGlobalLogger = nullptr;
        // Qt's Open GL context for the QWindow.
        QOpenGLContext mContext;
        // This is the actual OpenGL renderable surface.
        // the QWindow is then put inside a QWidget so that
        // wen can actually display it inside this MainWindow.
        QWindow* mSurface = nullptr;
        // The widget container for the QWindow (mSurface)
        QWidget* mContainer = nullptr;
        // A crappy flag for trying to distinguish between full screen
        // and windowed mode.
        bool mFullScreen = false;
        // Flag to indicate when the window has been closed or not.
        bool mClosed = false;
        // Flag to indicate if the app is currently "paused".
        // When pause is true no ticks or updates are called.
        bool mPaused = false;
        // False until DoInit has run.
        bool mInitDone = false;
        // Logger that we give to the application.
        // Note the thread safety because of for example audio threading.
        std::unique_ptr<SessionLogger> mLogger;
        // proxy model for filtering application event log
        app::EventLogProxy mEventLog;
        // The game/app object we've created and loaded from the library.
        std::unique_ptr<game::App> mApp;
        // rendering context implementation for the QWindow surface.
        std::unique_ptr<WindowContext> mWindowContext;
        // This resource loader implements the resolveURI to map
        // URIs to files based on the workspace configuration for 
        // the game playing (i.e. the working folder)
        std::unique_ptr<ResourceLoader> mResourceLoader;
        // Current game/app time. Updated in time steps whenever
        // the update timer runs.
        double mTimeTotal = 0.0;
        // Current number of frames within the last second.
        unsigned mNumFrames = 0;
        // total number of frames rendered.
        unsigned mNumFramesTotal = 0;
        // Timer to keep track of rendering times.
        QElapsedTimer mFrameTimer;
    };

} // namespace
