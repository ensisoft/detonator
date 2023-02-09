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
#  include <boost/circular_buffer.hpp>
#  include "ui_playwindow.h"
#include "warnpop.h"

#include <memory>
#include <variant>

#include "base/utility.h"
#include "engine/main/interface.h"
#include "editor/app/eventlog.h"
#include "editor/gui/dlgeventlog.h"
#include "wdk/events.h"

namespace app {
    class Workspace;
} // namespace

namespace engine {
    class Engine;
} //

namespace gui
{
    // PlayWindow is an OpenGL enabled window that will display
    // and host the game/app that is loaded from an app library.
    class PlayWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        PlayWindow(app::Workspace& workspace, bool is_separate_process = false);
       ~PlayWindow();

        // Returns true if the user has closed the window.
        bool IsClosed() const
        { return mClosed; }

        // Iterate the app once.
        void RunGameLoopOnce();

        // Do periodic low frequency tick.
        void NonGameTick();

        // Load the game library and launch the game from the beginning.
        // Returns true if successful or false if some problem happened.
        bool LoadGame();
        // Load the game library and begin a limited game launch in a
        // preview mode. In preview mode instead of running the main game
        // script, i.e. the actual game script we run another script, the
        // "preview script" instead.
        bool LoadPreview(const std::shared_ptr<const game::EntityClass>& entity);
        bool LoadPreview(const std::shared_ptr<const game::SceneClass>& scene);

        // Shut down the game and unload the library.
        void Shutdown();

        // Load the previous window state including window position, widget visibility
        // status bar and dock widget state from the current workspace when possible.
        // If no previous state is available under the given property keys (with the prefix)
        // it's assumed that this is the first launch and defaults invented. The optional
        // parent widget (his is a MainWindow so there's no Qt parent) is used to
        // center the play window around the parent when no previous state exists.
        void LoadState(const QString& key_prefix, const QWidget* parent = nullptr);

        // Save the current window state including window position, widget visibility,
        // status bar and dock widget state in the current workspce using the given
        // property key prefix for all the properties. The same values can then be
        // retrieved (and the state restored) by calling LoadState.
        void SaveState(const QString& key_prefix);

        void ShowWithWAR();
    public slots:
        void ActivateWindow();

    private slots:
        void InitGame();
        void InitPreview(const QString& script);
        void SelectResolution();
        void on_actionPause_toggled(bool val);
        void on_actionClose_triggered();
        void on_actionClearLog_triggered();
        void on_actionLogShowDebug_toggled(bool val);
        void on_actionLogShowInfo_toggled(bool val);
        void on_actionLogShowWarning_toggled(bool val);
        void on_actionLogShowError_toggled(bool val);
        void on_actionToggleDebugDraw_toggled();
        void on_actionToggleDebugLog_toggled();
        void on_actionToggleDebugMsg_toggled();
        void on_actionFullscreen_triggered();
        void on_actionScreenshot_triggered();
        void on_actionEventLog_triggered();
        void on_actionReloadShaders_triggered();
        void on_actionReloadTextures_triggered();
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
        void SetEngineConfig() const;
        void Barf(const std::string& msg);
        bool LoadLibrary();

    private:
        class WindowContext;
        class ResourceLoader;
        class SessionLogger;
        class ClassLibrary;
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
        Gamestudio_CreateEngineFunc mGameLibCreateEngine = nullptr;
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
        // False until DoInit has run.
        bool mInitDone = false;
        // Logger that we give to the application.
        // Note the thread safety because of for example audio threading.
        std::unique_ptr<SessionLogger> mLogger;
        // proxy model for filtering application event log
        app::EventLogProxy mAppEventLog;
        // The game engine loaded from the game library.
        std::unique_ptr<engine::Engine> mEngine;
        // rendering context implementation for the QWindow surface.
        std::unique_ptr<WindowContext> mWindowContext;
        // This resource loader implements the resolveURI to map
        // URIs to files based on the workspace configuration for 
        // the game playing (i.e. the working folder)
        std::unique_ptr<ResourceLoader> mResourceLoader;
        // Class library proxy for intercepting special resource names
        // when doing resource previews.
        std::unique_ptr<ClassLibrary> mClassLibrary;
        // timer to measure the passing of time.
        base::ElapsedTimer mTimer;
        // Current number of frames within the last second.
        unsigned mNumFrames = 0;
        // total number of frames rendered.
        unsigned mNumFramesTotal = 0;
        // Timer to keep track of rendering times.
        QElapsedTimer mFrameTimer;
        // Interface dialog for recording and playing back
        // window event captures i.e. mouse/keyboard inputs.
        std::unique_ptr<DlgEventLog> mWinEventLog;
        // event queue of wdk events that are waiting to be dispatched
        // to the game.
        using WindowEvent = std::variant<wdk::WindowEventResize,
                     wdk::WindowEventMouseRelease,
                     wdk::WindowEventMousePress,
                     wdk::WindowEventMouseMove,
                     wdk::WindowEventKeyUp,
                     wdk::WindowEventKeyDown>;
        boost::circular_buffer<WindowEvent> mEventQueue;
    };

} // namespace
