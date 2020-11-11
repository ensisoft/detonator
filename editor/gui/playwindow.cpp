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

#include "base/assert.h"
#include "base/logging.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/gui/playwindow.h"
#include "editor/gui/mainwidget.h"
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
        : mCurrent(current)
        , mPrevious(prev)
    {
        QDir::setCurrent(current);
    }
   ~TemporaryCurrentDirChange()
    {
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
// assets/contet created in the editor and sourced from
// the current workspace instead of from a file.
class PlayWindow::SessionAssets : public game::AssetTable
{
public:
    SessionAssets(const app::Workspace& workspace)
        : mWorkspace(workspace)
    {}
    virtual const game::AnimationClass* FindAnimationClassByName(const std::string& name) const override
    {
        auto klass = mWorkspace.GetAnimationClassByName(app::FromUtf8(name));
        return klass.get();
    }
    virtual const game::AnimationClass* FindAnimationClassById(const std::string& id) const override
    {
        auto klass = mWorkspace.GetAnimationClassById(app::FromUtf8(id));
        return klass.get();
    }

    virtual std::unique_ptr<game::Animation> CreateAnimationByName(const std::string& name) const override
    {
        auto klass = mWorkspace.GetAnimationClassByName(app::FromUtf8(name));
        klass->Prepare(mWorkspace);
        return game::CreateAnimationInstance(klass);
    }
    virtual std::unique_ptr<game::Animation> CreateAnimationById(const std::string& id) const override
    {
        auto klass = mWorkspace.GetAnimationClassById(app::FromUtf8(id));
        klass->Prepare(mWorkspace);
        return game::CreateAnimationInstance(klass);
    }

    virtual void LoadFromFile(const std::string&, const std::string&) override
    {
        // not implemented, loaded from workspace
    }
private:
    const app::Workspace& mWorkspace;
};

PlayWindow::PlayWindow(app::Workspace& workspace) : mWorkspace(workspace)
{
    DEBUG("Create playwindow");

    auto& logger = mLogger.GetLoggerUnsafe().GetLogger();

    mUI.setupUi(this);
    mUI.actionClose->setShortcut(QKeySequence::Close);
    mUI.log->setModel(&logger);
    connect(&logger, &app::EventLog::newEvent, this, &PlayWindow::NewLogEvent);

    const auto& settings = mWorkspace.GetProjectSettings();
    logger.SetLogTag(settings.application_name);
    setWindowTitle(settings.application_name);

    mPreviousWorkingDir = QDir::currentPath();
    mCurrentWorkingDir  = settings.working_folder;
    mCurrentWorkingDir.replace("${workspace}", mWorkspace.GetDir());
    DEBUG("Changing current directory to '%1'", mCurrentWorkingDir);

    mSurface = new QWindow();
    mSurface->setSurfaceType(QWindow::OpenGLSurface);
    mSurface->installEventFilter(this);
    // the container takes ownership of the window.
    mContainer = QWidget::createWindowContainer(mSurface, this);
    mUI.verticalLayout->addWidget(mContainer);

    // the default configuration has been set in main
    mContext.create();
    mContext.makeCurrent(mSurface);
    mWindowContext = std::make_unique<WindowContext>(&mContext, mSurface);

    // do one time delayed init on timer.
    QTimer::singleShot(10, this, &PlayWindow::DoInit);

    TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);

    QString library = settings.application_library;
#if defined(POSIX_OS) && defined(NDEBUG)
    library = app::JoinPath(mCurrentWorkingDir, "lib" + library + ".so");
#elif defined(POSIX_OS)
    library = app::JoinPath(mCurrentWorkingDir, "lib" + library + "d.so");
#elif defined(WINDOWS_OS)
    library = app::JoinPath(mCurrentWorkingDir, library + ".dll");
#endif

    mLibrary.setFileName(library);
    mLibrary.setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!mLibrary.load())
    {
        ERROR("Failed to load library '%1' (%2)", library, mLibrary.errorString());
        return;
    }

    auto* func = (MakeAppFunc)mLibrary.resolve("MakeApp");
    if (func == nullptr)
    {
        ERROR("Failed to resolve app entry point (MakeApp)");
        return;
    }
    auto* app = func(&mLogger);
    if (app == nullptr)
    {
        ERROR("Failed to create app instance.");
        return;
    }
    mApp.reset(app);

    // create new asset table based on the current workspace
    // and its content.
    mAssets = std::make_unique<SessionAssets>(mWorkspace);
    // Connect to a workspace signal for updating the asset table
    // when user saves edited content into the workspace.
    QObject::connect(&mWorkspace, &app::Workspace::ResourceUpdated,
        this, &PlayWindow::ResourceUpdated);
}

PlayWindow::~PlayWindow()
{
    mLibrary.unload();

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
        mUI.frames->setText(QString::number(mNumFramesTotal));
        mUI.time->setText(QString::number(mTimeTotal));

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

            mUI.fps->setText(QString::number(fps));

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
    auto logger = mLogger.GetLoggerSafe();
    logger->Dispatch();
}

void PlayWindow::DoInit()
{
    if (!mApp)
        return;

    // assumes that the current working directory has not been changed!
    const auto& host_app_path = QCoreApplication::applicationFilePath();

    // try to resize the rendering surface to the initial dimensions given
    // in the project settings.
    const auto& settings = mWorkspace.GetProjectSettings();
    ResizeSurface(settings.window_width, settings.window_height);

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
        env.gfx_factory = &mWorkspace;
        env.asset_table = mAssets.get();
        mApp->SetEnvironment(env);

        const auto surface_width  = mSurface->width();
        const auto surface_height = mSurface->height();
        mApp->Init(mWindowContext.get(), surface_width, surface_height);

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
    // trigger close event
    this->close();
}

void PlayWindow::on_tabWidget_currentChanged(int index)
{
    mUI.tabWidget->setTabIcon(1,QIcon("icons:log_info.png"));
}

void PlayWindow::ResourceUpdated(const app::Resource* resource)
{
    DEBUG("Update resource '%1'", resource->GetName());
}

void PlayWindow::NewLogEvent(const app::Event& event)
{
    if (mUI.tabWidget->currentWidget() == mUI.tabLog)
        return;

    if (event.type == app::Event::Type::Warning)
        mUI.tabWidget->setTabIcon(1, QIcon("icons:log_warning.png"));
    else if (event.type == app::Event::Type::Error)
        mUI.tabWidget->setTabIcon(1,QIcon("icons:log_error.png"));
}

void PlayWindow::closeEvent(QCloseEvent* event)
{
    DEBUG("Close event");

    event->accept();

    mContext.makeCurrent(mSurface);

    TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);

    try
    {
        mApp->Save();
        mApp->Shutdown();
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in app close: '%1'", e.what());
    }

    mApp.reset();

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
    if (!mApp)
        return QMainWindow::event(event);

    // app not providing a listener? return
    auto* listener = mApp->GetWindowListener();
    if (!listener)
        return QMainWindow::event(event);

    TemporaryCurrentDirChange cwd(mCurrentWorkingDir, mPreviousWorkingDir);

    if (event->type() == QEvent::KeyPress)
    {
        const auto* key_event = static_cast<const QKeyEvent*>(event);
        const auto mods = key_event->modifiers();

        wdk::WindowEventKeydown key;
        key.symbol = MapVirtualKey(key_event->key());

        //DEBUG("Qt key down: %1 -> %2", key_event->key(), key.symbol);

        if (mods & Qt::ShiftModifier)
            key.modifiers |= wdk::Keymod::Shift;
        if (mods & Qt::ControlModifier)
            key.modifiers |= wdk::Keymod::Control;
        if (mods & Qt::AltModifier)
            key.modifiers |= wdk::Keymod::Alt;
        listener->OnKeydown(key);
        return true;
    }
    else if (event->type() == QEvent::KeyRelease)
    {
        const auto* key_event = static_cast<const QKeyEvent*>(event);
        const auto mods = key_event->modifiers();

        wdk::WindowEventKeyup key;
        key.symbol = MapVirtualKey(key_event->key());

        //DEBUG("Qt key up: %1 -> %2", key_event->key(), key.symbol);

        if (mods & Qt::ShiftModifier)
            key.modifiers |= wdk::Keymod::Shift;
        if (mods & Qt::ControlModifier)
            key.modifiers |= wdk::Keymod::Control;
        if (mods & Qt::AltModifier)
            key.modifiers |= wdk::Keymod::Alt;
        listener->OnKeyup(key);
        return true;

    }
    else if (event->type() == QEvent::Resize)
    {
        const auto width  = mSurface->width();
        const auto height = mSurface->height();
        mApp->OnRenderingSurfaceResized(width, height);

        // try to give the keyboard focus to the window
        //mSurface->requestActivate();
        return true;
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
