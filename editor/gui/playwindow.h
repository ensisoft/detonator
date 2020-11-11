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

#include "base/logging.h"
#include "gamelib/main/interface.h"
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

        // Update the application.
        void Update(double dt);

        // Render the application.
        void Render();

        // Do periodic low frequency tick.
        void Tick();

        // Load the gamelibrary and launch the game.
        // Returns true if successful or false if some problem happened.
        bool LoadGame();

        // Shut down the game and unload the library.
        void Shutdown();

    private slots:
        void DoInit();
        void on_actionPause_triggered();
        void on_actionClose_triggered();
        void on_tabWidget_currentChanged(int index);
        void ResourceUpdated(const app::Resource* resource);
        void NewLogEvent(const app::Event& event);

    private:
        virtual void closeEvent(QCloseEvent* event) override;
        virtual bool eventFilter(QObject* destination, QEvent* event) override;
        void ResizeSurface(unsigned width, unsigned height);
        void SetFullscreen(bool fullscreen);
        void ToggleFullscreen();

    private:
        class WindowContext;
        class SessionAssets;
    private:
        Ui::PlayWindow mUI;
    private:
        // our current workspace.
        app::Workspace& mWorkspace;
        // Previous working directory.
        QString mPreviousWorkingDir;
        // Current working directory that we have to change
        // to before calling into app in case it refers to
        // data/resources relative to the working directory.
        QString mCurrentWorkingDir;
        // loader for the game/app library.
        QLibrary mLibrary;
        // entry point functions into the game/app library.
        MakeAppFunc                   mGameLibMakeApp = nullptr;
        SetResourceLoaderFunc         mGameLibSetResourceLoader = nullptr;
        SetGlobalLoggerFunc           mGameLibSetGlobalLogger = nullptr;
        // Qt's Open GL context for the QWindow.
        QOpenGLContext mContext;
        // This is the actual OpenGL renderable surface.
        // the QWindow is then put inside a QWidget so that
        // wen can actually display it inside this MainWindow.
        QWindow* mSurface = nullptr;
        // The widget container for the QWindow (mSurface)
        QWidget* mContainer = nullptr;
        // Logger that we give to the application.
        // Note the thread safety because of for example audio threading.
        base::LockedLogger<base::BufferLogger<app::EventLog>> mLogger;
        // Flag to indicate when the window has been closed or not.
        bool mClosed = false;
        // Flag to indicate if the app is currently "paused".
        // When pause is true no ticks or updates are called.
        bool mPaused = false;
        // False until DoInit has run.
        bool mInitDone = false;
        // The game/app object we've created and loaded from the library.
        std::unique_ptr<game::App> mApp;
        // rendering context implementation for the QWindow surface.
        std::unique_ptr<WindowContext> mWindowContext;
        // The current collection of assets for this play session.
        // The objects are derieved from the Workspacse instead of
        // loading from file. We then give this asset table object
        // to the game's app instance for getting data.
        std::unique_ptr<SessionAssets> mAssets;
        // Current game/app time. Updated in time steps whenever
        // the update timer runs.
        double mTimeTotal = 0.0;
        // time accumulator for keeping track of partial upates
        double mTimeAccum = 0.0;
        // time accumulator for doing app ticks.
        double mTickAccum = 0.0;
        // Current number of frames within the last second.
        unsigned mNumFrames = 0;
        // total number of frames rendered.
        unsigned mNumFramesTotal = 0;
        // Timer to keep track of rendering times.
        QElapsedTimer mFrameTimer;
    };

} // namespace
