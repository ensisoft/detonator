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

#define LOGTAG "playwindow"

#include "config.h"

#include "warnpush.h"
#  include <QCoreApplication>
#  include <QTimer>
#include "warnpop.h"

#include <map>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <gamelib/main/interface.h>

#include "base/assert.h"
#include "base/logging.h"
#include "base/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/gui/playwindow.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "gamelib/main/interface.h"
#include "gamelib/animation.h"
#include "wdk/system.h"

namespace {

// returns number of seconds elapsed since the last call
// of this function.
double ElapsedSeconds()
{
    using clock = std::chrono::steady_clock;
    static auto start = clock::now();
    auto now  = clock::now();
    auto gone = now - start;
    start = now;
    return std::chrono::duration_cast<std::chrono::microseconds>(gone).count() /
        (1000.0 * 1000.0);
}

// returns number of seconds since the application started
// running.
double CurrentRuntime()
{
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    const auto now  = clock::now();
    const auto gone = now - start;
    return std::chrono::duration_cast<std::chrono::microseconds>(gone).count() /
        (1000.0 * 1000.0);
}

class TemporaryCurrentDirChange
{
public:
    TemporaryCurrentDirChange(const QString&  current, const QString& prev)
        : mCurrent(QDir::cleanPath(current))
        , mPrevious(QDir::cleanPath(prev))
    {
        if (mCurrent == mPrevious)
            return;
        QDir::setCurrent(current);
    }
   ~TemporaryCurrentDirChange()
    {
        if (mCurrent == mPrevious)
            return;
        QDir::setCurrent(mPrevious);
    }
    TemporaryCurrentDirChange(const TemporaryCurrentDirChange&) = delete;
    TemporaryCurrentDirChange(TemporaryCurrentDirChange&&) = delete;
    TemporaryCurrentDirChange& operator=(const TemporaryCurrentDirChange&) = delete;
    TemporaryCurrentDirChange& operator=(TemporaryCurrentDirChange&&) = delete;
private:
    const QString mCurrent;
    const QString mPrevious;
};

wdk::MouseButton MapMouseButton(Qt::MouseButton button)
{
    if (button == Qt::MouseButton::NoButton)
        return wdk::MouseButton::None;
    else if (button == Qt::MouseButton::LeftButton)
        return wdk::MouseButton::Left;
    else if (button == Qt::MouseButton::RightButton)
        return wdk::MouseButton::Right;
    else if (button == Qt::MouseButton::MiddleButton)
        return wdk::MouseButton::Wheel;
    else if (button == Qt::MouseButton::BackButton)
        return wdk::MouseButton::Thumb1;
    WARN("Unmapped mouse button '%1'", button);
    return wdk::MouseButton::None;
}

wdk::bitflag<wdk::Keymod> MapKeyModifiers(int mods)
{
    wdk::bitflag<wdk::Keymod> modifiers;
    if (mods & Qt::ShiftModifier)
        modifiers |= wdk::Keymod::Shift;
    if (mods & Qt::ControlModifier)
        modifiers |= wdk::Keymod::Control;
    if (mods & Qt::AltModifier)
        modifiers |= wdk::Keymod::Alt;
    return modifiers;
}

    // table mapping Qt key identifiers to WDK key identifiers.
    // Qt doesn't provide a way to separate in virtual keys between
    // between Left and Right Control or Left and Right shift key.
    // We're mapping these to the *left* key for now.
wdk::Keysym MapVirtualKey(int from_qt)
{
    static std::map<int, wdk::Keysym> KeyMap = {
        {Qt::Key_Backspace, wdk::Keysym::Backspace},
        {Qt::Key_Tab,       wdk::Keysym::Tab},
        {Qt::Key_Return,    wdk::Keysym::Enter},
        {Qt::Key_Space,     wdk::Keysym::Space},
        {Qt::Key_0,         wdk::Keysym::Key0},
        {Qt::Key_1,         wdk::Keysym::Key1},
        {Qt::Key_2,         wdk::Keysym::Key2},
        {Qt::Key_3,         wdk::Keysym::Key3},
        {Qt::Key_4,         wdk::Keysym::Key4},
        {Qt::Key_5,         wdk::Keysym::Key5},
        {Qt::Key_6,         wdk::Keysym::Key6},
        {Qt::Key_7,         wdk::Keysym::Key7},
        {Qt::Key_8,         wdk::Keysym::Key8},
        {Qt::Key_9,         wdk::Keysym::Key9},
        {Qt::Key_A,         wdk::Keysym::KeyA},
        {Qt::Key_B,         wdk::Keysym::KeyB},
        {Qt::Key_C,         wdk::Keysym::KeyC},
        {Qt::Key_D,         wdk::Keysym::KeyD},
        {Qt::Key_E,         wdk::Keysym::KeyE},
        {Qt::Key_F,         wdk::Keysym::KeyF},
        {Qt::Key_G,         wdk::Keysym::KeyG},
        {Qt::Key_H,         wdk::Keysym::KeyH},
        {Qt::Key_I,         wdk::Keysym::KeyI},
        {Qt::Key_J,         wdk::Keysym::KeyJ},
        {Qt::Key_K,         wdk::Keysym::KeyK},
        {Qt::Key_L,         wdk::Keysym::KeyL},
        {Qt::Key_M,         wdk::Keysym::KeyM},
        {Qt::Key_N,         wdk::Keysym::KeyN},
        {Qt::Key_O,         wdk::Keysym::KeyO},
        {Qt::Key_P,         wdk::Keysym::KeyP},
        {Qt::Key_Q,         wdk::Keysym::KeyQ},
        {Qt::Key_R,         wdk::Keysym::KeyR},
        {Qt::Key_S,         wdk::Keysym::KeyS},
        {Qt::Key_T,         wdk::Keysym::KeyT},
        {Qt::Key_U,         wdk::Keysym::KeyU},
        {Qt::Key_V,         wdk::Keysym::KeyV},
        {Qt::Key_W,         wdk::Keysym::KeyW},
        {Qt::Key_X,         wdk::Keysym::KeyX},
        {Qt::Key_Y,         wdk::Keysym::KeyY},
        {Qt::Key_Z,         wdk::Keysym::KeyZ},
        {Qt::Key_F1,        wdk::Keysym::F1},
        {Qt::Key_F2,        wdk::Keysym::F2},
        {Qt::Key_F3,        wdk::Keysym::F3},
        {Qt::Key_F4,        wdk::Keysym::F4},
        {Qt::Key_F5,        wdk::Keysym::F5},
        {Qt::Key_F6,        wdk::Keysym::F6},
        {Qt::Key_F7,        wdk::Keysym::F7},
        {Qt::Key_F8,        wdk::Keysym::F8},
        {Qt::Key_F9,        wdk::Keysym::F9},
        {Qt::Key_F10,       wdk::Keysym::F10},
        {Qt::Key_F11,       wdk::Keysym::F11},
        {Qt::Key_F12,       wdk::Keysym::F12},
        {Qt::Key_Control,   wdk::Keysym::ControlL},
        {Qt::Key_Alt,       wdk::Keysym::AltL},
        {Qt::Key_Shift,     wdk::Keysym::ShiftL},
        {Qt::Key_CapsLock,  wdk::Keysym::CapsLock},
        {Qt::Key_Insert,    wdk::Keysym::Insert},
        {Qt::Key_Delete,    wdk::Keysym::Del},
        {Qt::Key_Home,      wdk::Keysym::Home},
        {Qt::Key_End,       wdk::Keysym::End},
        {Qt::Key_PageUp,    wdk::Keysym::PageUp},
        {Qt::Key_PageDown,  wdk::Keysym::PageDown},
        {Qt::Key_Left,      wdk::Keysym::ArrowLeft},
        {Qt::Key_Up,        wdk::Keysym::ArrowUp},
        {Qt::Key_Down,      wdk::Keysym::ArrowDown},
        {Qt::Key_Right,     wdk::Keysym::ArrowRight},
        {Qt::Key_Escape,    wdk::Keysym::Escape}
    };
    auto it = KeyMap.find(from_qt);
    if (it == std::end(KeyMap))
        return wdk::Keysym::None;
    return it->second;
}

} // namespace

namespace gui
{

class PlayWindow::WindowContext : public gfx::Device::Context
{
public:
    WindowContext(QOpenGLContext* context, QWindow* surface)
        : mContext(context)
        , mSurface(surface)
    {}
    virtual void Display() override
    {
        mContext->swapBuffers(mSurface);
    }
    virtual void MakeCurrent() override
    {
        mContext->makeCurrent(mSurface);
    }
    virtual void* Resolve(const char* name) override
    {
        return (void*)mContext->getProcAddress(name);
    }
private:
    QOpenGLContext* mContext = nullptr;
    QWindow* mSurface = nullptr;
};
// implementation of game asset table for accessing the
// assets/content created in the editor and sourced from
// the current workspace instead of from a file.
class PlayWindow::ResourceLoader : public gfx::ResourceLoader
{
public:
    ResourceLoader(const app::Workspace& workspace, const QString& gamedir)
        : mWorkspace(workspace)
        , mGameDir(gamedir)
    {}
    // Resource loader implementation
    virtual std::string ResolveURI(ResourceType type, const std::string& URI) const override
    {
        auto it = mFileMaps.find(URI);
        if (it != mFileMaps.end())
            return it->second;

        // called when the gfx system wants to resolve a file.
        // the app could have hardcoded paths that are relative to it's working dir.
        // the app could have paths that are encoded in the assets. For example ws://foo/bar.meh.png
        // the encoded case is resolved here using the workspace as the resolver.
        if (base::StartsWith(URI, "ws://") ||
            base::StartsWith(URI, "app://") ||
            base::StartsWith(URI, "fs://"))
            return mWorkspace.ResolveURI(type, URI);

        // What to do with paths such as "textures/UFO/ufo.png" ?
        // the application expects this to be relative and to be resolved
        // based on the current working directory when the application
        // is launched.
        const auto& resolved = app::JoinPath(mGameDir, app::FromUtf8(URI));
        const auto& ret = app::ToUtf8(resolved);
        mFileMaps[URI] = ret;
        DEBUG("Mapping gfx resource '%1' => '%2'", URI, resolved);
        return ret;
    }

private:
    const app::Workspace& mWorkspace;
    const QString mGameDir;
    mutable std::unordered_map<std::string, std::string> mFileMaps;
};

// implement base::logger and forward the log events
// to an app::EventLog object and to base logger
class PlayWindow::SessionLogger : public base::Logger
{
public:
    SessionLogger()
    {
        // we already have a time-stamp baked in, in the log
        // data coming from the game.
        mLogger.setShowTime(false);
        // the base/logger doesn't use log tags unfortunately
        // so it's useless.
        mLogger.setShowTag(false);
    }
    // base::Logger implementation
    virtual void Write(base::LogEvent type, const char* file, int line, const char* msg) override
    {
        // this one is not implemented since we're implementing
        // only the alternative with pre-formatted messages.
    }
    virtual void Write(base::LogEvent type, const char* msg) override
    {
        LogEvent event;
        if (type == base::LogEvent::Debug)
            event.type = app::Event::Type::Debug;
        else if (type == base::LogEvent::Info)
            event.type = app::Event::Type::Info;
        else if (type == base::LogEvent::Warning)
            event.type = app::Event::Type::Warning;
        else if (type == base::LogEvent::Error)
            event.type = app::Event::Type::Error;

        event.msg = QString::fromUtf8(msg);
        // make sure the log event doesn't end with carriage return/new line
        // characters since these can create confusing output in the listview.
        // i.e. if there's not enough space to dispay multiple rows of text
        // (see size hint role in eventlog) the text will be elided with ellipses
        if (event.msg.endsWith('\n'))
            event.msg.chop(1);
        if (event.msg.endsWith('\r'))
            event.msg.chop(1);

        // this write could be called by some other thread in the
        // application such as the audio thread.
        // so thread safely enqueue the event into a buffer
        // and then it's dispatched from there late (by this
        // app's main thread to the app::EventLog)
        std::unique_lock<std::mutex> lock(mMutex);
        mBuffer.push_back(event);
    }
    virtual void Flush() override
    { /* no op */ }

    void Dispatch()
    {
        // Dispatch to app::EventLog.
        std::unique_lock<std::mutex> lock(mMutex);
        for (const auto& event : mBuffer)
        {
            mLogger.write(event.type, event.msg, mLogTag);
        }
        mBuffer.clear();
    }
    void Clear()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mBuffer.clear();
        mLogger.clear();
    }

    void SetLogTag(const QString& tag)
    { mLogTag = tag; }

    app::EventLog* GetModel()
    { return &mLogger; }
private:
    struct LogEvent {
        QString msg;
        app::Event::Type type;
    };

    app::EventLog mLogger;
    std::mutex mMutex;
    std::vector<LogEvent> mBuffer;
    QString mLogTag;
};

PlayWindow::PlayWindow(app::Workspace& workspace) : mWorkspace(workspace)
{
    DEBUG("Create PlayWindow");
    mLogger = std::make_unique<SessionLogger>();

    mEventLog.SetModel(mLogger->GetModel());
    mEventLog.setSourceModel(mLogger->GetModel());

    mUI.setupUi(this);
    mUI.actionClose->setShortcut(QKeySequence::Close);
    mUI.log->setModel(&mEventLog);
    mUI.statusbar->insertPermanentWidget(0, mUI.statusBarFrame);

    const auto& settings = mWorkspace.GetProjectSettings();
    setWindowTitle(settings.application_name);
    mLogger->SetLogTag(settings.application_name);

    mPreviousWorkingDir = QDir::currentPath();
    mCurrentWorkingDir  = settings.working_folder;
    mCurrentWorkingDir.replace("${workspace}", mWorkspace.GetDir());
    DEBUG("Changing current directory to '%1'", mCurrentWorkingDir);

    mSurface = new QWindow();
    mSurface->setSurfaceType(QWindow::OpenGLSurface);
    mSurface->installEventFilter(this);
    // the container takes ownership of the window.
    mContainer = QWidget::createWindowContainer(mSurface, this);
    mContainer->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::MinimumExpanding);
    mUI.verticalLayout->addWidget(mContainer);

    // the default configuration has been set in main
    mContext.create();
    mContext.makeCurrent(mSurface);
    mWindowContext = std::make_unique<WindowContext>(&mContext, mSurface);

    // create new resource loader based on the current workspace
    // and its content.
    mResourceLoader = std::make_unique<ResourceLoader>(mWorkspace, mCurrentWorkingDir);
}

PlayWindow::~PlayWindow()
{
    Shutdown();

    QDir::setCurrent(mPreviousWorkingDir);

    DEBUG("Destroy PlayWindow");
}

void PlayWindow::Update(double dt)
{
    if (!mApp || !mInitDone)
        return;

    TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);

    mContext.makeCurrent(mSurface);

    // this is the real wall time elapsed since the previous iteration
    // of the "game loop".
    // On each iteration of the loop we measure the time
    // spent producing a frame. the time is then used to take
    // some number of simulation steps in order for the simulations
    // to catch up for the *next* frame.
    const auto& settings = mWorkspace.GetProjectSettings();
    const auto previous_frame_time = ElapsedSeconds();
    const auto time_step = 1.0 / settings.updates_per_second;
    const auto tick_step = 1.0 / settings.ticks_per_second;

    if (!mPaused)
    {
        mTimeAccum += previous_frame_time;
    }

    try
    {
        bool quit = false;
        game::App::Request request;
        while (mApp->GetNextRequest(&request))
        {
            if (const auto* ptr = std::get_if<game::App::ResizeWindow>(&request))
                ResizeSurface(ptr->width, ptr->height);
            else if (const auto* ptr = std::get_if<game::App::MoveWindow>(&request))
                this->move(ptr->xpos, ptr->ypos);
            else if (const auto* ptr = std::get_if<game::App::SetFullscreen>(&request))
                SetFullscreen(ptr->fullscreen);
            else if (const auto* ptr = std::get_if<game::App::ToggleFullscreen>(&request))
                ToggleFullscreen();
            else if (const auto* ptr = std::get_if<game::App::QuitApp>(&request))
                quit = true;
        }
        if (!mApp->IsRunning() || quit)
        {
            // trigger close event.
            this->close();
            return;
        }

        // do simulation/animation update steps.
        while (mTimeAccum >= time_step)
        {
            // if the simulation step takes more real time than
            // what the time step is worth we're going to start falling
            // behind, i.e. the frame times will grow and and for the
            // bigger time values more simulation steps need to be taken
            // which will slow things down even more.
            mApp->Update(mTimeTotal, time_step);
            mTimeTotal += time_step;
            mTimeAccum -= time_step;

            // put some accumulated time towards ticking the game.
            mTickAccum += time_step;
        }

        // do game tick steps
        auto tick_time = mTimeTotal;
        while (mTickAccum >= tick_step)
        {
            mApp->Tick(tick_time);
            tick_time += tick_step;
            mTickAccum -= tick_step;
        }
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in App::Update: '%1'", e.what());
    }
}

void PlayWindow::Render()
{
    if (!mApp || !mInitDone)
        return;

    TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);

    mContext.makeCurrent(mSurface);
    try
    {
        // ask the application to draw the current frame.
        mApp->Draw();

        mNumFrames++;
        mNumFramesTotal++;
        SetValue(mUI.frames, mNumFramesTotal);
        SetValue(mUI.time, mTimeTotal);

        const auto elapsed = mFrameTimer.elapsed();
        if (elapsed >= 1000)
        {
            const auto seconds = elapsed / 1000.0;
            const auto fps = mNumFrames / seconds;
            game::App::Stats stats;
            stats.num_frames_rendered = mNumFramesTotal;
            stats.total_game_time     = mTimeTotal;
            stats.total_wall_time     = CurrentRuntime();
            stats.current_fps         = fps;
            mApp->UpdateStats(stats);

            SetValue(mUI.fps, fps);

            mNumFrames = 0;
            mFrameTimer.restart();
        }
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in App::Draw: '%1'", e.what());
    }
}

void PlayWindow::Tick()
{
    // flush the buffer logger to the main log.
    mLogger->Dispatch();
}

bool PlayWindow::LoadGame()
{
    TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);

    const auto& settings = mWorkspace.GetProjectSettings();
    const auto& library  = mWorkspace.MapFileToFilesystem(settings.GetApplicationLibrary());
    mLibrary.setFileName(library);
    mLibrary.setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!mLibrary.load())
    {
        ERROR("Failed to load library '%1' (%2)", library, mLibrary.errorString());
        return false;
    }

    mGameLibMakeApp            = (MakeAppFunc)mLibrary.resolve("MakeApp");
    mGameLibSetResourceLoader  = (SetResourceLoaderFunc)mLibrary.resolve("SetResourceLoader");
    mGameLibSetGlobalLogger    = (SetGlobalLoggerFunc)mLibrary.resolve("SetGlobalLogger");
    if (!mGameLibMakeApp)
        ERROR("Failed to resolve 'MakeApp'.");
    else if (!mGameLibSetResourceLoader)
        ERROR("Failed to resolve 'SetResourceLoader'.");
    else if (!mGameLibSetGlobalLogger)
        ERROR("Failed to resolve 'SetGlobalLogger'.");
    if (!mGameLibMakeApp || !mGameLibSetGlobalLogger || !mGameLibSetResourceLoader)
        return false;

    mGameLibSetGlobalLogger(mLogger.get());
    mGameLibSetResourceLoader(mResourceLoader.get());

    std::unique_ptr<game::App> app;
    app.reset(mGameLibMakeApp());
    if (!app)
    {
        ERROR("Failed to create app instance.");
        return false;
    }
    mApp = std::move(app);

    // do one time delayed init on timer.
    QTimer::singleShot(10, this, &PlayWindow::DoAppInit);
    return true;
}

void PlayWindow::Shutdown()
{
    mContext.makeCurrent(mSurface);
    try
    {
        if (mApp)
        {
            DEBUG("Shutting down game...");
            TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);
            mApp->Save();
            mApp->Shutdown();
        }
    }
    catch (const std::exception &e)
    {
        ERROR("Exception in app close: '%1'", e.what());
    }
    mApp.reset();

    if (mGameLibSetResourceLoader)
        mGameLibSetResourceLoader(nullptr);
    if (mGameLibSetGlobalLogger)
        mGameLibSetGlobalLogger(nullptr);

    mLibrary.unload();

    mGameLibMakeApp = nullptr;
    mGameLibSetGlobalLogger = nullptr;
    mGameLibSetResourceLoader = nullptr;
}

void PlayWindow::LoadState()
{
    // if this is the first the project is launched resize
    // the rendering surface to the initial size specified
    // in the project settings. otherwise use the size
    // from the previous run.
    // note that the application is later able to request
    // a different size however. (See the requests processing)
    const auto& project = mWorkspace.GetProjectSettings();
    const auto& window_geometry = mWorkspace.GetUserProperty("play_window_geometry", QByteArray());
    const auto& toolbar_and_dock_state = mWorkspace.GetUserProperty("play_window_toolbar_and_dock_state", QByteArray());
    const unsigned log_bits = mWorkspace.GetUserProperty("play_window_log_bits", mEventLog.GetShowBits());

    mEventLog.SetShowBits(log_bits);
    mEventLog.invalidate();
    mUI.actionLogShowDebug->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Debug));
    mUI.actionLogShowInfo->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Info));
    mUI.actionLogShowWarning->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Warning));
    mUI.actionLogShowError->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Error));

    if (!window_geometry.isEmpty())
        restoreGeometry(window_geometry);

    if (!toolbar_and_dock_state.isEmpty())
        restoreState(toolbar_and_dock_state);

    // try to resize. See the comments above.
    if (window_geometry.isEmpty())
    {
        ResizeSurface(project.window_width, project.window_height);
    }

    // try to disable the resizing of the rendering surface.
    // probably won't work as expected.
    if (!project.window_can_resize)
    {
        const auto w = project.window_width;
        const auto h = project.window_height;
        mSurface->setMaximumSize(QSize(w, h));
        mSurface->setMinimumSize(QSize(w, h));
        mSurface->setBaseSize(QSize(w, h));
    }

    const bool show_status_bar = mWorkspace.GetUserProperty("play_window_show_statusbar",
                                                            mUI.statusbar->isVisible());
    const bool show_eventlog = mWorkspace.GetUserProperty("play_window_show_eventlog",
                                                          mUI.dockWidget->isVisible());
    QSignalBlocker foo(mUI.actionViewEventlog);
    QSignalBlocker bar(mUI.actionViewStatusbar);
    mUI.actionViewEventlog->setChecked(show_eventlog);
    mUI.actionViewStatusbar->setChecked(show_status_bar);
    mUI.statusbar->setVisible(show_status_bar);
    mUI.dockWidget->setVisible(show_eventlog);
}

void PlayWindow::SaveState()
{
    mWorkspace.SetUserProperty("play_window_show_statusbar", mUI.statusbar->isVisible());
    mWorkspace.SetUserProperty("play_window_show_eventlog", mUI.dockWidget->isVisible());
    mWorkspace.SetUserProperty("play_window_geometry", saveGeometry());
    mWorkspace.SetUserProperty("play_window_toolbar_and_dock_state", saveState());
    mWorkspace.SetUserProperty("play_window_log_bits", mEventLog.GetShowBits());
}

void PlayWindow::DoAppInit()
{
    if (!mApp)
        return;

    // assumes that the current working directory has not been changed!
    const auto& host_app_path = QCoreApplication::applicationFilePath();

    TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);
    try
    {
        mContext.makeCurrent(mSurface);

        const auto& settings = mWorkspace.GetProjectSettings();
        std::vector<std::string> args;
        std::vector<const char*> arg_pointers;
        args.push_back(app::ToUtf8(host_app_path));
        // todo: deal with arguments in quotes and with spaces for example "foo bar"
        const QStringList& list = settings.command_line_arguments.split(" ", QString::SkipEmptyParts);
        for (const auto& arg : list)
        {
            args.push_back(app::ToUtf8(arg));
        }
        for (const auto& str : args)
        {
            arg_pointers.push_back(str.c_str());
        }
        mApp->ParseArgs(static_cast<int>(arg_pointers.size()), &arg_pointers[0]);

        game::App::Environment env;
        env.classlib = &mWorkspace;
        mApp->SetEnvironment(env);

        const auto surface_width  = mSurface->width();
        const auto surface_height = mSurface->height();
        mApp->Init(mWindowContext.get(), surface_width, surface_height);

        game::App::EngineConfig config;
        config.ticks_per_second   = settings.ticks_per_second;
        config.updates_per_second = settings.updates_per_second;
        config.physics.num_velocity_iterations = settings.num_velocity_iterations;
        config.physics.num_position_iterations = settings.num_position_iterations;
        config.physics.gravity = settings.gravity;
        config.physics.scale   = settings.physics_scale;
        config.default_mag_filter = settings.default_mag_filter;
        config.default_min_filter = settings.default_min_filter;

        mApp->SetEngineConfig(config);
        mApp->Load();
        mApp->Start();

        // try to give the keyboard focus to the window
        //mSurface->requestActivate();

        mFrameTimer.start();
        mInitDone = true;
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in app init: '%1'", e.what());
    }
}

void PlayWindow::on_actionPause_triggered()
{
    mPaused = mUI.actionPause->isChecked();
}

void PlayWindow::on_actionClose_triggered()
{
    mClosed = true;
}

void PlayWindow::on_actionClearLog_triggered()
{
    mLogger->Clear();
}
void PlayWindow::on_actionLogShowDebug_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Debug, val);
    mEventLog.invalidate();
}
void PlayWindow::on_actionLogShowInfo_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Info, val);
    mEventLog.invalidate();
}
void PlayWindow::on_actionLogShowWarning_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Warning, val);
    mEventLog.invalidate();
}
void PlayWindow::on_actionLogShowError_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Error, val);
    mEventLog.invalidate();
}
void PlayWindow::on_log_customContextMenuRequested(QPoint point)
{
    QMenu menu(this);
    menu.addAction(mUI.actionClearLog);
    menu.addSeparator();
    menu.addAction(mUI.actionLogShowDebug);
    menu.addAction(mUI.actionLogShowInfo);
    menu.addAction(mUI.actionLogShowWarning);
    menu.addAction(mUI.actionLogShowError);
    menu.exec(QCursor::pos());
}

void PlayWindow::closeEvent(QCloseEvent* event)
{
    DEBUG("Play window close event");
    event->ignore();
    // we could emit an event here to indicate that the window
    // is getting closed but that's a sure-fire way of getting
    // unwanted recursion that will fuck shit up.  (i.e this window
    // getting deleted which will run the destructor which will
    // make this function have an invalided *this* pointer. bad.
    // so instead of doing that we just set a flag and the
    // mainwindow will check from time to if the window object
    // should be deleted.
    mClosed = true;
}

bool PlayWindow::eventFilter(QObject* destination, QEvent* event)
{
    // we're only interesting intercepting our window events
    // that will be translated into wdk events and passed to the
    // application object.
    if (destination != mSurface)
        return QMainWindow::event(event);

    // no app? return
    if (!mApp || !mInitDone)
        return QMainWindow::event(event);

    // app not providing a listener? return
    auto* listener = mApp->GetWindowListener();
    if (!listener)
        return QMainWindow::event(event);

    TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);
    try
    {
        if (event->type() == QEvent::KeyPress)
        {
            const auto* key_event = static_cast<const QKeyEvent *>(event);

            wdk::WindowEventKeydown key;
            key.symbol    = MapVirtualKey(key_event->key());
            key.modifiers = MapKeyModifiers(key_event->modifiers());
            listener->OnKeydown(key);
            //DEBUG("Qt key down: %1 -> %2", key_event->key(), key.symbol);
            return true;
        }
        else if (event->type() == QEvent::KeyRelease)
        {
            const auto* key_event = static_cast<const QKeyEvent *>(event);

            wdk::WindowEventKeyup key;
            key.symbol    = MapVirtualKey(key_event->key());
            key.modifiers = MapKeyModifiers(key_event->modifiers());
            listener->OnKeyup(key);
            //DEBUG("Qt key up: %1 -> %2", key_event->key(), key.symbol);
            return true;
        }
        else if (event->type() == QEvent::MouseMove)
        {
            const auto* mouse = static_cast<const QMouseEvent*>(event);

            wdk::WindowEventMouseMove m;
            m.window_x = mouse->x();
            m.window_y = mouse->y();
            m.global_x = mouse->globalX();
            m.global_y = mouse->globalY();
            m.modifiers = MapKeyModifiers(mouse->modifiers());
            m.btn       = MapMouseButton(mouse->button());
            listener->OnMouseMove(m);
        }
        else if (event->type() == QEvent::MouseButtonPress)
        {
            const auto* mouse = static_cast<const QMouseEvent*>(event);

            wdk::WindowEventMousePress press;
            press.window_x = mouse->x();
            press.window_y = mouse->y();
            press.global_x = mouse->globalX();
            press.global_y = mouse->globalY();
            press.modifiers = MapKeyModifiers(mouse->modifiers());
            press.btn       = MapMouseButton(mouse->button());
            listener->OnMousePress(press);
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            const auto* mouse = static_cast<const QMouseEvent*>(event);

            wdk::WindowEventMouseRelease release;
            release.window_x = mouse->x();
            release.window_y = mouse->y();
            release.global_x = mouse->globalX();
            release.global_y = mouse->globalY();
            release.modifiers = MapKeyModifiers(mouse->modifiers());
            release.btn       = MapMouseButton(mouse->button());
            listener->OnMouseRelease(release);
        }
        if (event->type() == QEvent::Resize)
        {
            const auto width = mSurface->width();
            const auto height = mSurface->height();
            mApp->OnRenderingSurfaceResized(width, height);

            // try to give the keyboard focus to the window
            //mSurface->requestActivate();
            return true;
        }
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in app '%1'", e.what());
    }

    return QMainWindow::event(event);
}

void PlayWindow::ResizeSurface(unsigned width, unsigned height)
{
    const auto old_surface_width  = mSurface->width();
    const auto old_surface_height = mSurface->height();

    // The QWindow (which is our rendering surface) is embedded inside a
    // widget. Direct calls trying to control the window's dimensions are
    // not recommended. So if the application asks for a rendering surface
    // to be resized we need to resize this QMainWindow.
    // But for this we need to know what is the size difference between the
    // actual rendering surface window size and this window. Then based on
    // that we assume that the difference would be constant and adding the
    // extra size would result in the desired rendering surface size.
    const auto window_width = this->width();
    const auto window_height = this->height();

    const auto width_extra = window_width - old_surface_width;
    const auto height_extra = window_height - old_surface_height;

    // warning this will generate a resize event which call back into
    // the app through OnRenderingSurfaceResize. Careful not to have
    // any unwanted recursion here!
    this->resize(width + width_extra, height + height_extra);
}

void PlayWindow::SetFullscreen(bool fullscreen)
{
    if (fullscreen)
        showFullScreen();
    else showNormal();

    mSurface->requestActivate();
}
void PlayWindow::ToggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else showFullScreen();

    mSurface->requestActivate();
}

} // namespace
