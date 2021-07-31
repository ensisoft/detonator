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

#include "base/assert.h"
#include "base/logging.h"
#include "base/utility.h"
#include "audio/loader.h"
#include "editor/app/buffer.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/gui/playwindow.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/utility.h"
#include "engine/main/interface.h"
#include "engine/loader.h"
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
    TemporaryCurrentDirChange(const QString& current)
        : mCurrent(current)
        , mPrevious(QDir::currentPath())
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
        {Qt::Key_Escape,    wdk::Keysym::Escape},
        {Qt::Key_Plus,      wdk::Keysym::Plus},
        {Qt::Key_Minus,     wdk::Keysym::Minus}
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
        // Try to avoid Qt error about calling swap buffers on non-exposed window
        // resulting in undefined behavior
        if (mSurface->isExposed())
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
    void SetSurface(QWindow* surface)
    { mSurface = surface; }
private:
    QOpenGLContext* mContext = nullptr;
    QWindow* mSurface = nullptr;
};
// implementation of game asset table for accessing the
// assets/content created in the editor and sourced from
// the current workspace instead of from a file.
class PlayWindow::ResourceLoader : public gfx::ResourceLoader,
                                   public game::GameDataLoader,
                                   public audio::Loader
{
public:
    ResourceLoader(const app::Workspace& workspace,
                   const QString& gamedir,
                   const QString& hostdir)
        : mWorkspace(workspace)
        , mGameDir(gamedir)
        , mHostDir(hostdir)
    {}
    // Resource loader implementation
    virtual gfx::ResourceHandle LoadResource(const std::string& URI) override
    {
        const auto& file = ResolveURI(URI);
        DEBUG("URI '%1' => '%2'", URI, file);
        return app::GraphicsFileBuffer::LoadFromFile(file);
    }
    virtual game::GameDataHandle LoadGameData(const std::string& URI) const override
    {
        const auto& file = ResolveURI(URI);
        DEBUG("URI '%1' => '%2'", URI, file);
        return app::GameDataFileBuffer::LoadFromFile(file);
    }
    virtual game::GameDataHandle LoadGameDataFromFile(const std::string& filename) const override
    {
        return app::GameDataFileBuffer::LoadFromFile(app::FromUtf8(filename));
    }
    virtual std::ifstream OpenStream(const std::string& URI) const override
    {
        const auto& file = ResolveURI(URI);
        DEBUG("URI '%1' => '%2'", URI, file);
        auto stream = app::OpenBinaryIStream(file);
        if (!stream.is_open())
            ERROR("Failed to open '%1'.", file);
        return stream;
    }
private:
    QString ResolveURI(const std::string& URI) const
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
        {
            const auto& ret = mWorkspace.MapFileToFilesystem(app::FromUtf8(URI));
            mFileMaps[URI]  = ret;
            return ret;
        }
        WARN("Unmapped resource URI '%1'", URI);

        // What to do with paths such as "textures/UFO/ufo.png" ?
        // the application expects this to be relative and to be resolved
        // based on the current working directory when the application
        // is launched.
        const auto& ret = app::JoinPath(mGameDir, app::FromUtf8(URI));
        mFileMaps[URI] = ret;
        return ret;
    }
private:
    const app::Workspace& mWorkspace;
    const QString mGameDir;
    const QString mHostDir;
    mutable std::unordered_map<std::string, QString> mFileMaps;
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
        // i.e. if there's not enough space to display multiple rows of text
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
    mUI.problem->setVisible(false);

    const auto& settings = mWorkspace.GetProjectSettings();
    setWindowTitle(settings.application_name);
    mLogger->SetLogTag(settings.application_name);

    mHostWorkingDir = QDir::currentPath();
    mGameWorkingDir = settings.working_folder;
    mGameWorkingDir.replace("${workspace}", mWorkspace.GetDir());
    DEBUG("Host working directory set to '%1'", mHostWorkingDir);
    DEBUG("Game working directory set to '%1'", mGameWorkingDir);

    mSurface = new QWindow();
    mSurface->setSurfaceType(QWindow::OpenGLSurface);
    mSurface->installEventFilter(this);
    // the container takes ownership of the window.
    mContainer = QWidget::createWindowContainer(mSurface, this);
    mContainer->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::MinimumExpanding);
    if (!settings.window_cursor)
    {
        mContainer->setCursor(Qt::BlankCursor);
        mSurface->setCursor(Qt::BlankCursor);
    }
    mUI.verticalLayout->addWidget(mContainer);

    // the default configuration has been set in main
    mContext.create();
    mContext.makeCurrent(mSurface);
    mWindowContext = std::make_unique<WindowContext>(&mContext, mSurface);

    // create new resource loader based on the current workspace
    // and its content.
    mResourceLoader = std::make_unique<ResourceLoader>(mWorkspace, mGameWorkingDir, mHostWorkingDir);
}

PlayWindow::~PlayWindow()
{
    Shutdown();

    QDir::setCurrent(mHostWorkingDir);

    DEBUG("Destroy PlayWindow");
}

void PlayWindow::BeginMainLoop()
{
    if (!mApp || !mInitDone)
        return;
    TemporaryCurrentDirChange cwd(mGameWorkingDir);

    mContext.makeCurrent(mSurface);
    try
    {
        mApp->BeginMainLoop();
    }
    catch (const std::exception& e)
    {
        DEBUG("Exception in App::BeginMainLoop.");
        Barf(e.what());
    }
}

void PlayWindow::EndMainLoop()
{
    if (!mApp || !mInitDone)
        return;
    TemporaryCurrentDirChange cwd(mGameWorkingDir);

    mContext.makeCurrent(mSurface);
    try
    {
        mApp->EndMainLoop();
    }
    catch (const std::exception& e)
    {
        DEBUG("Exception in App::EndMainLoop.");
        Barf(e.what());
    }
}

void PlayWindow::RunOnce()
{
    if (!mApp || !mInitDone)
        return;

    TemporaryCurrentDirChange cwd(mGameWorkingDir);

    mContext.makeCurrent(mSurface);
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
            else if (const auto* ptr = std::get_if<game::App::SetFullScreen>(&request))
                AskSetFullScreen(ptr->fullscreen);
            else if (const auto* ptr = std::get_if<game::App::ToggleFullScreen>(&request))
                AskToggleFullScreen();
            else if (const auto* ptr = std::get_if<game::App::ShowMouseCursor>(&request)) {
                if (ptr->show) {
                    mContainer->setCursor(Qt::ArrowCursor);
                    mSurface->setCursor(Qt::ArrowCursor);
                } else {
                    mContainer->setCursor(Qt::BlankCursor);
                    mSurface->setCursor(Qt::BlankCursor);
                }
            } else if (const auto* ptr = std::get_if<game::App::QuitApp>(&request)) {
                INFO("Quit with exit code %1", ptr->exit_code);
                quit = true;
            }
        }
        if (!mApp->IsRunning() || quit)
        {
            // trigger close event.
            this->close();
            return;
        }

        // this is the real wall time elapsed rendering the previous
        // for each iteration of the loop we measure the time
        // spent producing a frame. the time is then used to take
        // some number of simulation steps in order for the simulations
        // to catch up for the *next* frame.
        const auto time_step = mPaused ? 0.0 : ElapsedSeconds();
        mTimeTotal += time_step;

        // ask the application to take its simulation steps.
        mApp->Update(mTimeTotal, time_step);

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
            stats.total_wall_time     = mTimeTotal;
            stats.current_fps         = fps;
            mApp->UpdateStats(stats);

            SetValue(mUI.fps, fps);
            mNumFrames = 0;
            mFrameTimer.restart();
        }
    }
    catch (const std::exception& e)
    {
        DEBUG("Exception in App Update/Draw.");
        Barf(e.what());
    }
}

void PlayWindow::NonGameTick()
{
    // flush the buffer logger to the main log.
    mLogger->Dispatch();
}

bool PlayWindow::LoadGame()
{
    TemporaryCurrentDirChange cwd(mGameWorkingDir);

    const auto& settings = mWorkspace.GetProjectSettings();
    const auto& library  = mWorkspace.MapFileToFilesystem(settings.GetApplicationLibrary());
    mLibrary.setFileName(library);
    mLibrary.setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!mLibrary.load())
    {
        ERROR("Failed to load library '%1' (%2)", library, mLibrary.errorString());
        return false;
    }

    mGameLibCreateApp       = (Gamestudio_CreateAppFunc)mLibrary.resolve("Gamestudio_CreateApp");
    mGameLibSetGlobalLogger = (Gamestudio_SetGlobalLoggerFunc)mLibrary.resolve("Gamestudio_SetGlobalLogger");
    if (!mGameLibCreateApp)
        ERROR("Failed to resolve 'Gamestudio_CreateApp'.");
    else if (!mGameLibSetGlobalLogger)
        ERROR("Failed to resolve 'Gamestudio_SetGlobalLogger'.");
    if (!mGameLibCreateApp || !mGameLibSetGlobalLogger)
        return false;

    const auto debug_log = GetValue(mUI.actionToggleDebugLog);
    mGameLibSetGlobalLogger(mLogger.get(), debug_log);

    std::unique_ptr<game::App> app;
    app.reset(mGameLibCreateApp());
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
            TemporaryCurrentDirChange cwd(mGameWorkingDir);
            mApp->Save();
            mApp->Shutdown();
        }
    }
    catch (const std::exception &e)
    {
        DEBUG("Exception in app shutdown.");
        ERROR(e.what());
    }
    mApp.reset();

    if (mGameLibSetGlobalLogger)
        mGameLibSetGlobalLogger(nullptr, false);

    mLibrary.unload();
    mGameLibCreateApp = nullptr;
    mGameLibSetGlobalLogger = nullptr;
}

void PlayWindow::LoadState()
{
    // if this is the first time the project is launched then
    // resize the rendering surface to the initial size specified
    // in the project settings. otherwise use the size saved from
    // the previous run.
    // note that the application is later able to request
    // a different size however. (See the requests processing)
    const auto& project = mWorkspace.GetProjectSettings();
    const auto& window_geometry = mWorkspace.GetUserProperty("play_window_geometry", QByteArray());
    const auto& toolbar_and_dock_state = mWorkspace.GetUserProperty("play_window_toolbar_and_dock_state", QByteArray());
    const unsigned log_bits = mWorkspace.GetUserProperty("play_window_log_bits", mEventLog.GetShowBits());
    const QString log_filter = mWorkspace.GetUserProperty("play_window_log_filter", QString());
    const bool log_filter_case_sens = mWorkspace.GetUserProperty("play_window_log_filter_case_sensitive", true);
    mEventLog.SetFilterStr(log_filter, log_filter_case_sens);
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

    const bool show_status_bar = mWorkspace.GetUserProperty("play_window_show_statusbar",
       mUI.statusbar->isVisible());
    const bool show_eventlog = mWorkspace.GetUserProperty("play_window_show_eventlog",
        mUI.dockWidget->isVisible());
    const bool debug_draw = mWorkspace.GetUserProperty("play_window_debug_draw",
       mUI.actionToggleDebugDraw->isChecked());
    const bool debug_log = mWorkspace.GetUserProperty("play_window_debug_log",
       mUI.actionToggleDebugLog->isChecked());
    const bool debug_msg = mWorkspace.GetUserProperty("play_window_debug_msg",
       mUI.actionToggleDebugMsg->isChecked());
    SetValue(mUI.actionToggleDebugMsg, debug_msg);
    SetValue(mUI.actionToggleDebugLog, debug_log);
    SetValue(mUI.actionToggleDebugDraw, debug_draw);
    SetValue(mUI.logFilter, log_filter);
    SetValue(mUI.logFilterCaseSensitive, log_filter_case_sens);
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
    mWorkspace.SetUserProperty("play_window_debug_draw", (bool)GetValue(mUI.actionToggleDebugDraw));
    mWorkspace.SetUserProperty("play_window_debug_log", (bool)GetValue(mUI.actionToggleDebugLog));
    mWorkspace.SetUserProperty("play_window_debug_msg", (bool)GetValue(mUI.actionToggleDebugMsg));
    mWorkspace.SetUserProperty("play_window_log_filter", (QString)GetValue(mUI.logFilter));
    mWorkspace.SetUserProperty("play_window_log_filter_case_sensitive", (bool)GetValue(mUI.logFilterCaseSensitive));
}

void PlayWindow::DoAppInit()
{
    if (!mApp)
        return;

    // assumes that the current working directory has not been changed!
    const auto& host_app_path = QCoreApplication::applicationFilePath();

    TemporaryCurrentDirChange cwd(mGameWorkingDir);
    try
    {
        mContext.makeCurrent(mSurface);

        const auto& settings = mWorkspace.GetProjectSettings();
        std::vector<std::string> args;
        std::vector<const char*> arg_pointers;
        args.push_back(app::ToUtf8(host_app_path));
        // todo: deal with arguments in quotes and with spaces for example "foo bar"
        const QStringList& list = settings.command_line_arguments.split(" ", Qt::SplitBehaviorFlags::KeepEmptyParts);
        for (const auto& arg : list)
        {
            args.push_back(app::ToUtf8(arg));
        }
        for (const auto& str : args)
        {
            arg_pointers.push_back(str.c_str());
        }
        mApp->ParseArgs(static_cast<int>(arg_pointers.size()), &arg_pointers[0]);

        SetDebugOptions();

        game::App::Environment env;
        env.classlib  = &mWorkspace;
        env.content   = mResourceLoader.get();
        env.loader    = mResourceLoader.get();
        env.directory = app::ToUtf8(mGameWorkingDir);
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
        config.clear_color = ToGfx(settings.clear_color);

        mApp->SetEngineConfig(config);
        mApp->Load();
        mApp->Start();

        // try to give the keyboard focus to the window
        // looks like this has to be done through a timer again.
        // calling the functions below directly don't achieve the
        // desired effect.
        //mSurface->setKeyboardGrabEnabled(true);
        //mSurface->raise();
        //mSurface->requestActivate();
        // set a timer to try to activate for keyboard input after some delay.
        QTimer::singleShot(100, this, &PlayWindow::ActivateWindow);

        mFrameTimer.start();
        mInitDone = true;
    }
    catch (const std::exception& e)
    {
        DEBUG("Exception in app init.");
        Barf(e.what());
    }
}

void PlayWindow::ActivateWindow()
{
    //mSurface->setKeyboardGrabEnabled(true);
    mSurface->raise();
    mSurface->requestActivate();
}

void PlayWindow::on_actionPause_triggered()
{
    mPaused = mUI.actionPause->isChecked();
    if (!mPaused)
        ElapsedSeconds(); // reset time to avoid big jumps in dt
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

void PlayWindow::on_actionToggleDebugDraw_toggled()
{
    SetDebugOptions();
}
void PlayWindow::on_actionToggleDebugLog_toggled()
{
    SetDebugOptions();
}
void PlayWindow::on_actionToggleDebugMsg_toggled()
{
    SetDebugOptions();
}

void PlayWindow::on_actionFullscreen_triggered()
{
    if (!InFullScreen())
    {
        SetFullScreen(true);
    }
    else
    {
        SetFullScreen(false);
    }
}

void PlayWindow::on_actionScreenshot_triggered()
{
    if (!mApp || !mInitDone)
        return;
    TemporaryCurrentDirChange cwd(mGameWorkingDir);
    try
    {
        mContext.makeCurrent(mSurface);
        mApp->TakeScreenshot("screenshot.png");
        INFO("Wrote screenshot '%1'", "screenshot.png");
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in App::TakeScreenshot.");
        ERROR(e.what());
    }
}

void PlayWindow::on_btnApplyFilter_clicked()
{
    mEventLog.SetFilterStr(GetValue(mUI.logFilter),GetValue(mUI.logFilterCaseSensitive));
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

    TemporaryCurrentDirChange cwd(mGameWorkingDir);
    try
    {
        if (event->type() == QEvent::KeyPress)
        {
            // this will collide with the application if the app wants to also use
            // the F11 key for something. But there aren't a lot of possibilities here..
            const auto* key_event = static_cast<const QKeyEvent *>(event);
            if (key_event->key() == Qt::Key_F11 && InFullScreen())
                SetFullScreen(false);
            else if (key_event->key() == Qt::Key_F7 && InFullScreen())
                mUI.actionPause->trigger();
            else if (key_event->key() == Qt::Key_F9 && InFullScreen())
                mUI.actionScreenshot->trigger();

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
            QTimer::singleShot(100, this, &PlayWindow::ActivateWindow);
            return true;
        }
    }
    catch (const std::exception& e)
    {
        DEBUG("Exception in app event handler.");
        Barf(e.what());
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

void PlayWindow::AskSetFullScreen(bool fullscreen)
{
    if (fullscreen && !InFullScreen())
    {
        QMessageBox msg(this);
        msg.setWindowTitle(tr("Enable Full Screen?"));
        msg.setText(tr("The application has requested to go into full screen mode. \n"
                       "Do you want to accept this?"));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Question);
        if (msg.exec() == QMessageBox::No)
            return;
    }
    SetFullScreen(fullscreen);
}
void PlayWindow::AskToggleFullScreen()
{
    AskSetFullScreen(!InFullScreen());
}

bool PlayWindow::InFullScreen() const
{
    return mFullScreen;
}

void PlayWindow::SetFullScreen(bool fullscreen)
{
    if (fullscreen && !InFullScreen())
    {
        // the QWindow cannot be set into full screen if it's managed by the
        // container, the way to get it out of the container is to re-parent to nullptr.
        mSurface->setParent(nullptr);
        // now try to go into full screen.
        mSurface->showFullScreen();
        // todo: this should probably only be called after some transition
        // event is detected indicating that the window did in fact go into
        // full screen mode.
        mApp->OnEnterFullScreen();

        mApp->DebugPrintString("Press F11 to return to windowed mode.");
    }
    else if (!fullscreen && InFullScreen())
    {
        // seems there aren't any other ways to go back into embedding the
        // QWindow inside this window and its widgets other than re-create
        // everything.
        // WARNING, deleting the container deletes the QWindow!
        // https://stackoverflow.com/questions/46003395/getting-qwindow-back-into-parent-qwidget-from-the-fullscreen

        // re-create a new rendering surface.
        QWindow* surface = new QWindow();
        surface->setSurfaceType(QWindow::OpenGLSurface);
        surface->installEventFilter(this);
        mContext.makeCurrent(surface);
        mWindowContext->SetSurface(surface);

        const auto& settings = mWorkspace.GetProjectSettings();
        // re-create the window container widget and place into the layout.
        delete mContainer;
        mContainer = QWidget::createWindowContainer(surface, this);
        mContainer->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::MinimumExpanding);
        if (!settings.window_cursor)
            mContainer->setCursor(Qt::BlankCursor);
        mUI.verticalLayout->addWidget(mContainer);
        mSurface = surface;
        if (!settings.window_cursor)
            mSurface->setCursor(Qt::BlankCursor);

        // todo: this can be wrong if the window never did go into fullscreen mode.
        mApp->OnLeaveFullScreen();
    }
    // todo: should really only set this flag when the window *did* go into
    // fullscreen mode.
    mFullScreen = fullscreen;

    ActivateWindow();

    SetDebugOptions();
}

void PlayWindow::SetDebugOptions() const
{
    game::App::DebugOptions debug;
    debug.debug_draw      = GetValue(mUI.actionToggleDebugDraw);
    debug.debug_log       = GetValue(mUI.actionToggleDebugLog);
    debug.debug_show_msg  = GetValue(mUI.actionToggleDebugMsg);
    debug.debug_font      = "app://fonts/orbitron-medium.otf";
    debug.debug_show_fps  = InFullScreen();
    debug.debug_print_fps = false;
    mApp->SetDebugOptions(debug);
}

void PlayWindow::Barf(const std::string& msg)
{
    ERROR(msg);
    mApp.reset();
    mContainer->setVisible(false);
    mUI.problem->setVisible(true);
    SetValue(mUI.lblError, msg);
}

} // namespace
