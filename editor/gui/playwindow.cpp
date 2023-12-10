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
#include "device/device.h"
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
        {Qt::Key_Backtab,   wdk::Keysym::Tab}, // really. wtf?
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

class PlayWindow::WindowContext : public dev::Context
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
    virtual Version GetVersion() const override
    {
        return Version::OpenGL_ES3;
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
class PlayWindow::ResourceLoader : public gfx::Loader,
                                   public engine::Loader,
                                   public audio::Loader,
                                   public game::Loader
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
    virtual gfx::ResourceHandle LoadResource(const gfx::Loader::ResourceDesc& desc) override
    {
        const auto& URI = desc.uri;

        auto it = mGraphicsBuffers.find(URI);
        if (it != mGraphicsBuffers.end())
            return it->second;

        const auto& file = ResolveURI(URI);
        DEBUG("URI '%1' => '%2'", URI, file);
        auto buffer = app::GraphicsBuffer::LoadFromFile(file);
        mGraphicsBuffers[URI] = buffer;
        return buffer;
    }
    virtual engine::EngineDataHandle LoadEngineDataUri(const std::string& URI) const override
    {
        auto it = mEngineDataBuffers.find(URI);
        if (it != mEngineDataBuffers.end())
            return it->second;

        const auto& file = ResolveURI(URI);
        DEBUG("URI '%1' => '%2'", URI, file);
        auto buffer = app::EngineBuffer::LoadFromFile(file);
        mEngineDataBuffers[URI] = buffer;
        return buffer;
    }
    virtual engine::EngineDataHandle LoadEngineDataFile(const std::string& filename) const override
    {
        // expect this to be a path relative to the content path
        // (which is the workspace path here)
        // this loading function is only used to load the Lua files
        // which don't yet proper resource URIs. When that is fixed
        // this function can go away!
        const auto& file = app::JoinPath(mWorkspace.GetDir(), app::FromUtf8(filename));

        return app::EngineBuffer::LoadFromFile(file);
    }
    virtual engine::EngineDataHandle LoadEngineDataId(const std::string& id) const override
    {
        return mWorkspace.LoadEngineDataId(id);
    }

    virtual audio::SourceStreamHandle OpenAudioStream(const std::string& URI,
        AudioIOStrategy strategy, bool enable_file_caching) const override
    {
        const auto& file = ResolveURI(URI);
        DEBUG("URI '%1' => '%2'", URI, file);
        return audio::OpenFileStream(app::ToUtf8(file), strategy, enable_file_caching);
    }
    // game::Loader implementation
    virtual game::TilemapDataHandle LoadTilemapData(const game::Loader::TilemapDataDesc& desc) const override
    {
        const auto& file = ResolveURI(desc.uri);
        DEBUG("URI '%1' => '%2'", desc.uri, file);
        if (desc.read_only)
            return app::TilemapMemoryMap::OpenFilemap(file);

        return app::TilemapBuffer::LoadFromFile(file);
    }

    std::size_t GetBufferCacheSize() const
    {
        size_t ret = 0;
        for (const auto& pair : mEngineDataBuffers)
            ret += pair.second->GetByteSize();
        for (const auto& pair : mGraphicsBuffers)
            ret += pair.second->GetByteSize();
        return ret;
    }
    void BlowCaches()
    {
        mEngineDataBuffers.clear();
        mGraphicsBuffers.clear();
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
        WARN("Unmapped resource URI. [uri='%1']", URI);

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
    mutable std::unordered_map<std::string, engine::EngineDataHandle> mEngineDataBuffers;
    mutable std::unordered_map<std::string, gfx::ResourceHandle> mGraphicsBuffers;
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

class PlayWindow::ClassLibrary : public engine::ClassLibrary
{
public:
    ClassLibrary(const app::Workspace& workspace)
      : mWorkspace(workspace)
    {}
    virtual ClassHandle<const audio::GraphClass> FindAudioGraphClassById(const std::string& id) const override
    {
        return mWorkspace.FindAudioGraphClassById(id);
    }
    virtual ClassHandle<const audio::GraphClass> FindAudioGraphClassByName(const std::string& name) const override
    {
        return mWorkspace.FindAudioGraphClassByName(name);
    }
    virtual ClassHandle<const uik::Window> FindUIByName(const std::string& name) const override
    {
        return mWorkspace.FindUIByName(name);
    }
    virtual ClassHandle<const uik::Window> FindUIById(const std::string& id) const override
    {
        return mWorkspace.FindUIById(id);
    }
    virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassByName(const std::string& name) const override
    {
        return mWorkspace.FindMaterialClassByName(name);
    }
    virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& id) const override
    {
        return mWorkspace.FindMaterialClassById(id);
    }
    virtual ClassHandle<const gfx::DrawableClass> FindDrawableClassById(const std::string& id) const override
    {
        return mWorkspace.FindDrawableClassById(id);
    }
    virtual ClassHandle<const game::EntityClass> FindEntityClassByName(const std::string& name) const override
    {
        if (name == "_entity_preview_")
            return mEntityPreview;
        return mWorkspace.FindEntityClassByName(name);
    }
    virtual ClassHandle<const game::EntityClass> FindEntityClassById(const std::string& id) const override
    {
        if (id == "_entity_preview_")
            return mEntityPreview;
        return mWorkspace.FindEntityClassById(id);
    }
    virtual ClassHandle<const game::SceneClass> FindSceneClassByName(const std::string& name) const override
    {
        if (name == "_entity_preview_scene_")
            return mEntityPreviewScene;
        else if (name == "_scene_preview_")
            return mScenePreview;
        return mWorkspace.FindSceneClassByName(name);
    }
    virtual ClassHandle<const game::SceneClass> FindSceneClassById(const std::string& id) const override
    {
        if (id == "_entity_preview_scene_")
            return mEntityPreviewScene;
        else if (id == "_scene_preview_")
            return mScenePreview;
        return mWorkspace.FindSceneClassById(id);
    }
    virtual ClassHandle<const game::TilemapClass> FindTilemapClassById(const std::string& id) const override
    {
        return mWorkspace.FindTilemapClassById(id);
    }
    void SetScenePreview(const std::shared_ptr<const game::SceneClass>& scene)
    { mScenePreview = scene; }
    void SetEntityPreviewScene(const std::shared_ptr<const game::SceneClass>& klass)
    { mEntityPreviewScene = klass; }
    void SetEntityPreview(const std::shared_ptr<const game::EntityClass>& klass)
    { mEntityPreview = klass; }

private:
    const app::Workspace& mWorkspace;
    std::shared_ptr<const game::SceneClass> mEntityPreviewScene;
    std::shared_ptr<const game::EntityClass> mEntityPreview;
    std::shared_ptr<const game::SceneClass> mScenePreview;
};


PlayWindow::PlayWindow(app::Workspace& workspace, bool is_separate_process) : mWorkspace(workspace), mEventQueue(200)
{
    DEBUG("Create PlayWindow");
    mLogger = std::make_unique<SessionLogger>();

    mAppEventLog.SetModel(mLogger->GetModel());
    mAppEventLog.setSourceModel(mLogger->GetModel());

    mUI.setupUi(this);
    //mUI.actionClose->setShortcut(QKeySequence::Close); // use ours
    mUI.log->setModel(&mAppEventLog);
    mUI.statusbar->insertPermanentWidget(0, mUI.statusBarFrame);
    SetVisible(mUI.problem, false);
    SetEnabled(mUI.actionStep, false);

    const auto& settings = mWorkspace.GetProjectSettings();

    const auto& resolutions = app::ListResolutions();
    for (unsigned i=0; i<resolutions.size(); ++i)
    {
        const auto& rez = resolutions[i];
        QAction* action = mUI.menuResize->addAction(QString("%1 (%2x%3)")
            .arg(rez.name)
            .arg(rez.width)
            .arg(rez.height));
        action->setData(i);
        connect(action, &QAction::triggered, this, &PlayWindow::SelectResolution);
    }
    mUI.menuResize->addSeparator();
    QAction* game_default_res = mUI.menuResize->addAction(QString("%1 (%2x%3)")
            .arg("Game Default")
            .arg(settings.window_width)
            .arg(settings.window_height));
    game_default_res->setData(-1);
    connect(game_default_res, &QAction::triggered, this, &PlayWindow::SelectResolution);

    SetWindowTitle(this, settings.application_name);
    mLogger->SetLogTag(settings.application_name);

    mHostWorkingDir = QDir::currentPath();
    mGameWorkingDir = settings.working_folder;
    mGameWorkingDir.replace("${workspace}", mWorkspace.GetDir());
    DEBUG("Host working directory set to '%1'", mHostWorkingDir);
    DEBUG("Game working directory set to '%1'", mGameWorkingDir);

    // Set default surface format.
    // note that the alpha channel is not used on purpose.
    // using an alpha channel will cause artifacts with alpha
    // compositing window compositor such as picom. i.e. the
    // background surfaces in the compositor's window stack will
    // show through. in terms of alpha blending the game content
    // whether the destination color buffer has alpha channel or
    // not should be irrelevant.
    QSurfaceFormat format;
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(0); // no alpha channel
    format.setStencilBufferSize(8);
    format.setDepthBufferSize(24);
    format.setSamples(settings.multisample_sample_count);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setColorSpace(settings.config_srgb ? QSurfaceFormat::ColorSpace::sRGBColorSpace
                                              : QSurfaceFormat::ColorSpace::DefaultColorSpace);

    // So the problem is that if the play window is being used from the editor's main
    // process setting swap interval will jank things unexpectedly because the thread
    // will block on swap. this does not play ball with having multiple OpenGL windows.
    // If there's another OpenGL window (such as GfxWidget) that has swap interval set
    // to 1 (i.e. VSYNC) then the frame rate will halve. (two waits on swap...)
    // When running in process and using vsync the rendering should be moved into a
    // separate thread, or then we just simply ignore the flag.
    if (is_separate_process)
        format.setSwapInterval(settings.window_vsync ? 1 : 0);
    else format.setSwapInterval(0);

    mSurface = new QWindow();
    mSurface->setFormat(format);
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

void PlayWindow::RunGameLoopOnce()
{
    if (!mEngine || !mInitDone)
        return;

    TemporaryCurrentDirChange cwd(mGameWorkingDir);

    mContext.makeCurrent(mSurface);
    try
    {
        auto* listener = mEngine->GetWindowListener();
        bool quit = false;

        // indicate beginning of the main loop iteration.
        mEngine->BeginMainLoop();

        // if we have an event log that is being replayed then source
        // the window input events from the log.
        if (mWinEventLog && mWinEventLog->IsPlaying())
        {
            ASSERT(mEventQueue.empty());
            mWinEventLog->Replay(*listener, mTimer.SinceStart());
        }
        else
        {
            for (auto it = mEventQueue.begin(); it != mEventQueue.end(); ++it)
            {
                const auto& event = *it;
                if (const auto* ptr = std::get_if<wdk::WindowEventResize>(&event)) {
                    mEngine->OnRenderingSurfaceResized(ptr->width, ptr->height);
                    mEngine->DebugPrintString(base::FormatString("Surface resized to %1x%2", ptr->width, ptr->height));
                    ActivateWindow();
                }
                else if (const auto* ptr = std::get_if<wdk::WindowEventMouseRelease>(&event))
                    listener->OnMouseRelease(*ptr);
                else if (const auto* ptr = std::get_if<wdk::WindowEventMousePress>(&event))
                    listener->OnMousePress(*ptr);
                else if (const auto* ptr = std::get_if<wdk::WindowEventMouseMove>(&event))
                    listener->OnMouseMove(*ptr);
                else if (const auto* ptr = std::get_if<wdk::WindowEventKeyUp>(&event))
                    listener->OnKeyUp(*ptr);
                else if (const auto* ptr = std::get_if<wdk::WindowEventKeyDown>(&event))
                    listener->OnKeyDown(*ptr);
                else BUG("Unhandled window event type.");
            }
            mEventQueue.clear();
        }

        // Process pending application requests if any.
        engine::Engine::Request request;
        while (mEngine->GetNextRequest(&request))
        {
            if (const auto* ptr = std::get_if<engine::Engine::ResizeSurface>(&request))
                ResizeSurface(ptr->width, ptr->height);
            else if (const auto* ptr = std::get_if<engine::Engine::SetFullScreen>(&request))
                AskSetFullScreen(ptr->fullscreen);
            else if (const auto* ptr = std::get_if<engine::Engine::ToggleFullScreen>(&request))
                AskToggleFullScreen();
            else if (const auto* ptr = std::get_if<engine::Engine::DebugPause>(&request))
                DebugPause(ptr->pause);
            else if (const auto* ptr = std::get_if<engine::Engine::ShowMouseCursor>(&request)) {
                if (ptr->show) {
                    mContainer->setCursor(Qt::ArrowCursor);
                    mSurface->setCursor(Qt::ArrowCursor);
                } else {
                    mContainer->setCursor(Qt::BlankCursor);
                    mSurface->setCursor(Qt::BlankCursor);
                }
            } else if (const auto* ptr = std::get_if<engine::Engine::QuitApp>(&request)) {
                INFO("Quit with exit code %1", ptr->exit_code);
                quit = true;
            }
        }

        // This is the real wall time elapsed rendering the previous frame.
        // For each iteration of the loop we measure the time
        // spent producing a frame and that time is then used to take
        // some number of simulation steps in order for the simulations
        // to catch up for the *next* frame.
        const auto time_step = mTimer.Delta();
        const auto wall_time = mTimer.SinceStart();
        // ask the application to take its simulation steps.
        mEngine->Update(time_step);

        // ask the application to draw the current frame.
        mEngine->Draw();

        if (mWinEventLog)
            mWinEventLog->SetTime(wall_time);

        engine::Engine::Stats stats;
        if (mEngine->GetStats(&stats))
        {
            SetValue(mUI.gameTime, stats.total_game_time);
            const auto kb = 1024.0; // * 1024.0;
            const auto vbo_use = stats.static_vbo_mem_use +
                                 stats.streaming_vbo_mem_use +
                                 stats.dynamic_vbo_mem_use;
            const auto vbo_alloc = stats.static_vbo_mem_alloc +
                                   stats.streaming_vbo_mem_alloc +
                                   stats.dynamic_vbo_mem_alloc;
            SetValue(mUI.statVBO, QString("%1/%2 kB")
                    .arg(vbo_use / kb, 0, 'f', 1, ' ').arg(vbo_alloc / kb, 0, 'f', 1, ' '));
        }

        mNumFrames++;
        mNumFramesTotal++;
        SetValue(mUI.frames, mNumFramesTotal);
        SetValue(mUI.wallTime, wall_time);

        const auto elapsed = mFrameTimer.elapsed();
        if (elapsed >= 1000)
        {
            const auto seconds = elapsed / 1000.0;
            const auto fps = mNumFrames / seconds;
            engine::Engine::HostStats stats;
            stats.num_frames_rendered = mNumFramesTotal;
            stats.total_wall_time     = wall_time;
            stats.current_fps         = fps;
            mEngine->SetHostStats(stats);

            const auto cache = mResourceLoader->GetBufferCacheSize();
            const auto megs  = cache / (1024.0 * 1024.0);
            SetValue(mUI.statFileCache, QString("%1 MB").arg(megs, 0, 'f', 1, ' '));
            SetValue(mUI.fps, fps);
            mNumFrames = 0;
            mFrameTimer.restart();
        }

        // indicate end of iteration.
        mEngine->EndMainLoop();

        if (!mEngine->IsRunning() || quit)
        {
            // trigger close event.
            this->close();
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
    if (mWinEventLog && mWinEventLog->IsClosed())
    {
        mWorkspace.SetUserProperty("play_window_event_dlg_geom", mWinEventLog->saveGeometry());
        mWinEventLog->close();
        mWinEventLog.reset();
    }

    // flush the buffer logger to the main log.
    mLogger->Dispatch();
}

bool PlayWindow::LoadGame(bool clean_game_home)
{
    if (!LoadLibrary())
        return false;

    // Another workaround for Qt bugs has been created and the timer
    // based workaround is now here only for posterity.
    //QTimer::singleShot(10, this, &PlayWindow::InitGame);

    // call directly now.
    InitGame(clean_game_home);
    return true;
}

bool PlayWindow::LoadPreview(const std::shared_ptr<const game::EntityClass>& entity)
{
    if (!LoadLibrary())
        return false;

    mClassLibrary = std::make_unique<ClassLibrary>(mWorkspace);
    mClassLibrary->SetEntityPreview(entity);
    // when doing a preview for an entity we must setup a temporary / dummy scene
    // in order to be able to spawn the entity into the scene in case none exist
    // by this special name.
    const auto& name = entity->GetName();
    if (auto klass = mWorkspace.FindSceneClassByName(base::FormatString("_%1_preview_scene_", name)))
        mClassLibrary->SetEntityPreviewScene(klass);
    else if (auto klass = mWorkspace.FindSceneClassByName("_entity_preview_scene_"))
        mClassLibrary->SetEntityPreviewScene(klass);
    else {
        auto dummy_klass = std::make_shared<game::SceneClass>();
        dummy_klass->SetName("_entity_preview_scene_");
        mClassLibrary->SetEntityPreviewScene(dummy_klass);
    }

    // haha, this also doesn't work.. how in the f**k is this even possible??
    //SetWindowTitle(this, entity->GetName());

    QTimer::singleShot(10, [entity, this]() {
        SetWindowTitle(this, entity->GetName());
    });

    const auto& settings = mWorkspace.GetProjectSettings();
    InitPreview(settings.preview_entity_script);
    return true;
}

bool PlayWindow::LoadPreview(const std::shared_ptr<const game::SceneClass>& scene)
{
    if(!LoadLibrary())
        return false;

    mClassLibrary = std::make_unique<ClassLibrary>(mWorkspace);
    mClassLibrary->SetScenePreview(scene);

    QTimer::singleShot(10, [scene, this]() {
        SetWindowTitle(this, scene->GetName());
    });

    const auto& settings = mWorkspace.GetProjectSettings();
    InitPreview(settings.preview_scene_script);
    return true;
}

void PlayWindow::Shutdown()
{
    mContext.makeCurrent(mSurface);
    try
    {
        if (mEngine)
        {
            DEBUG("Shutting down game...");
            TemporaryCurrentDirChange cwd(mGameWorkingDir);
            mEngine->Stop();
            mEngine->Save();
            mEngine->Shutdown();
        }
    }
    catch (const std::exception &e)
    {
        DEBUG("Exception in app shutdown.");
        ERROR(e.what());
    }
    mEngine.reset();

    if (mGameLibSetGlobalLogger)
        mGameLibSetGlobalLogger(nullptr, false, false, false, false);

    mLibrary.unload();
    mGameLibCreateEngine = nullptr;
    mGameLibSetGlobalLogger = nullptr;
}

void PlayWindow::LoadState(const QString& key_prefix, const QWidget* parent)
{
    // IMPORTANT:
    // Keep in mind that if LoadState is called *before* the window is shown
    // the status bars, event logs etc. are *not* visible, which means their visibility
    // cannot be used as the proper initial state. But if LoadState is called after the window
    // is visible then the UX is a bit janky since the window will first appear in
    // the default size and then be re-adjusted, so it'd be better to set the window
    // size before showing the window.

    // If this is the first time the project/game is launched then
    // resize the rendering surface to the initial size specified
    // in the project settings. Otherwise, use the size saved in
    // the user properties after the previous run. Note that in either
    // case the game itself is able to request a different window size
    // as well as try to go into full screen mode.
    const auto& window_geometry        = mWorkspace.GetUserProperty(key_prefix + "_geometry", QByteArray());
    const auto& toolbar_and_dock_state = mWorkspace.GetUserProperty(key_prefix + "_toolbar_and_dock_state", QByteArray());
    const unsigned log_bits            = mWorkspace.GetUserProperty(key_prefix + "_log_bits", mAppEventLog.GetShowBits());
    const auto& log_filter             = mWorkspace.GetUserProperty(key_prefix + "_log_filter", QString());
    const bool log_filter_case_sens    = mWorkspace.GetUserProperty(key_prefix + "_log_filter_case_sensitive", true);
    mAppEventLog.SetFilterStr(log_filter, log_filter_case_sens);
    mAppEventLog.SetShowBits(log_bits);
    mAppEventLog.invalidate();
    mUI.actionLogShowDebug->setChecked(mAppEventLog.IsShown(app::EventLogProxy::Show::Debug));
    mUI.actionLogShowInfo->setChecked(mAppEventLog.IsShown(app::EventLogProxy::Show::Info));
    mUI.actionLogShowWarning->setChecked(mAppEventLog.IsShown(app::EventLogProxy::Show::Warning));
    mUI.actionLogShowError->setChecked(mAppEventLog.IsShown(app::EventLogProxy::Show::Error));

    if (!window_geometry.isEmpty())
        restoreGeometry(window_geometry);

    if (!toolbar_and_dock_state.isEmpty())
        restoreState(toolbar_and_dock_state);

    // try to resize. See the comments above.
    if (window_geometry.isEmpty())
    {
        // Do this on the timer so that we hopefully have the QSurface
        // with initial size. if we try to get the size without window
        // being shown the size will be 0!
        QTimer::singleShot(100, [this, parent]() {
            const auto& settings = mWorkspace.GetProjectSettings();
            ResizeSurface(settings.window_width, settings.window_height);

            // resize and relocate on the desktop, by default the window seems
            // to be at a position that requires it to be immediately used and
            // resize by the user. ugh
            if (parent)
            {
                const auto width  = this->width();
                const auto height = this->height();
                const auto parent_pos = parent->mapToGlobal(parent->pos());
                const auto xpos = parent_pos.x() + (parent->width() - width) / 2;
                const auto ypos = parent_pos.y() + (parent->height() - height) / 2;
                this->move(xpos, ypos);
            }
        });
    }

    const bool show_status_bar = mWorkspace.GetUserProperty(key_prefix + "_show_statusbar", true);
    const bool show_eventlog   = mWorkspace.GetUserProperty(key_prefix + "_show_eventlog", true);
    const bool debug_draw      = mWorkspace.GetUserProperty(key_prefix + "_debug_draw", mUI.actionToggleDebugDraw->isChecked());
    const bool debug_log       = mWorkspace.GetUserProperty(key_prefix + "_debug_log", mUI.actionToggleDebugLog->isChecked());
    const bool debug_msg       = mWorkspace.GetUserProperty(key_prefix + "_debug_msg", mUI.actionToggleDebugMsg->isChecked());
    SetValue(mUI.actionToggleDebugMsg, debug_msg);
    SetValue(mUI.actionToggleDebugLog, debug_log);
    SetValue(mUI.actionToggleDebugDraw, debug_draw);
    SetValue(mUI.logFilter, log_filter);
    SetValue(mUI.logFilterCaseSensitive, log_filter_case_sens);
    SetValue(mUI.actionViewStatusbar, show_status_bar);
    SetValue(mUI.actionViewEventlog, show_eventlog);
    SetVisible(mUI.statusbar, show_status_bar);
    SetVisible(mUI.dockWidget, show_eventlog);
}

void PlayWindow::SaveState(const QString& key_prefix)
{
    mWorkspace.SetUserProperty(key_prefix + "_show_statusbar", mUI.statusbar->isVisible());
    mWorkspace.SetUserProperty(key_prefix + "_show_eventlog", mUI.dockWidget->isVisible());
    mWorkspace.SetUserProperty(key_prefix + "_geometry", saveGeometry());
    mWorkspace.SetUserProperty(key_prefix + "_toolbar_and_dock_state", saveState());
    mWorkspace.SetUserProperty(key_prefix + "_log_bits", mAppEventLog.GetShowBits());
    mWorkspace.SetUserProperty(key_prefix + "_debug_draw", (bool)GetValue(mUI.actionToggleDebugDraw));
    mWorkspace.SetUserProperty(key_prefix + "_debug_log", (bool)GetValue(mUI.actionToggleDebugLog));
    mWorkspace.SetUserProperty(key_prefix + "_debug_msg", (bool)GetValue(mUI.actionToggleDebugMsg));
    mWorkspace.SetUserProperty(key_prefix + "_log_filter", (QString)GetValue(mUI.logFilter));
    mWorkspace.SetUserProperty(key_prefix + "_log_filter_case_sensitive", (bool)GetValue(mUI.logFilterCaseSensitive));
}

void PlayWindow::ShowWithWAR()
{
    // This code tries to workaround Qt bugs related to the QWindow
    // that we're using as the OpenGL rendering surface here.
    // Issues are: things like raise() or requestActive or setWindowTitle
    // don't always work. Seems like issues are probably related to X11 and
    // whether the window is mapped or not, i.e. calling those functions
    // "too soon" fails and the keyboard focus persistently stays on the
    // log filter QLineEdit. #%"%¤½!"#!

    // show *this* window.
    show();

    clearFocus();
    mUI.log->clearFocus();
    mUI.logFilter->clearFocus();

    for (unsigned i=0; i<1000; ++i)
    {
        const bool is_visible = mSurface->isVisible();
        const bool has_size = mSurface->width() && mSurface->height();
        if (is_visible && has_size)
            break;

        qApp->processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // put the QWindow (used as OpenGL rendering surface) on top and try
    // to give it the initial keyboard focus. Without this the keyboard
    // focus is on the log filter QLineEdit which is annoying since the
    // initial key presses don't go the game.
    mSurface->raise();
    mSurface->requestActivate();

    // Random timer interval to the rescue! Go Qt!
    QTimer::singleShot(100, [this]() {
        mUI.log->clearFocus();
        mUI.logFilter->clearFocus();
        this->clearFocus();
        mSurface->raise();
        mSurface->requestActivate();
    });
}

// This function is very similar with InitPreview but subtly different
// so make sure you cross-check changes properly.
void PlayWindow::InitGame(bool clean_game_home)
{
    if (!mEngine)
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
        mEngine->ParseArgs(static_cast<int>(arg_pointers.size()), &arg_pointers[0]);

        SetDebugOptions();

        QString user_home = QDir::homePath();
        QString game_home = settings.game_home;
        QString editor_home = app::JoinPath(user_home, ".GameStudio");
        game_home.replace("${workspace}", mWorkspace.GetDir());
        game_home.replace("${user-home}", user_home);
        game_home.replace("${game-id}", settings.application_identifier);
        game_home.replace("${game-ver}", settings.application_version);
        game_home.replace("${game-home}", app::JoinPath(editor_home, settings.application_identifier));
        DEBUG("User home is '%1'", user_home);
        DEBUG("Game home is '%1'", game_home);

        if (clean_game_home)
        {
            QDir dir(game_home);
            if (dir.exists())
            {
                const auto& cant_hire = dir.canonicalPath();
                if (cant_hire == user_home)
                    WARN("Game home points to user home. Refusing to delete. You're welcome.");
                else if (cant_hire == editor_home)
                    WARN("Game home points to editor home. Refusing to delete. You're welcome.");
                else if (cant_hire == mWorkspace.GetDir())
                    WARN("Game home points to project workspace. Refusing to delete. You're welcome.");
                else {
                    DEBUG("Deleted game home directory. [dir='%1']", game_home);
                    dir.removeRecursively();
                }
            }
        }

        app::MakePath(editor_home);
        app::MakePath(game_home);

        engine::Engine::Environment env;
        env.classlib           = &mWorkspace;
        env.engine_loader      = mResourceLoader.get();
        env.graphics_loader    = mResourceLoader.get();
        env.audio_loader       = mResourceLoader.get();
        env.game_loader        = mResourceLoader.get();
        env.directory          = app::ToUtf8(mGameWorkingDir);
        env.user_home          = app::ToUtf8(QDir::toNativeSeparators(user_home));
        env.game_home          = app::ToUtf8(QDir::toNativeSeparators(game_home));
        mEngine->SetEnvironment(env);

        engine::Engine::InitParams params;
        params.editing_mode     = true; // allow changes to "static" content take place.
        params.preview_mode     = false;
        params.game_script      = app::ToUtf8(settings.game_script);
        params.application_name = app::ToUtf8(settings.application_name);
        params.context          = mWindowContext.get();
        params.surface_width    = mSurface->width();
        params.surface_height   = mSurface->height();
        mEngine->Init(params);

        SetEngineConfig();

        if (!mEngine->Load())
        {
            Barf("Engine failed to load. Please see the log for more details.");
            return;
        }

        mEngine->Start();
        mTimer.Start();
        mFrameTimer.start();
        mInitDone = true;

        SetEnabled(mUI.toolBar,         true);
        SetEnabled(mUI.menuApplication, true);
        SetEnabled(mUI.menuSurface,     true);
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in engine init. [what='%1']", e.what());
        Barf(e.what());
    }
}

// This function is very similar with InitGame but subtly different
// so make sure you cross-check changes properly.
void PlayWindow::InitPreview(const QString& script)
{
    if (!mEngine)
        return;

    // assumes that the current working directory has not been changed!
    const auto& host_app_path = QCoreApplication::applicationFilePath();

    TemporaryCurrentDirChange cwd(mGameWorkingDir);
    try
    {
        mContext.makeCurrent(mSurface);

        const auto& settings = mWorkspace.GetProjectSettings();

        // we're going to skip calling ParseArgs here for now.

        SetDebugOptions();

        engine::Engine::Environment env;
        env.classlib           = mClassLibrary.get(); // use the proxy
        env.engine_loader      = mResourceLoader.get();
        env.graphics_loader    = mResourceLoader.get();
        env.audio_loader       = mResourceLoader.get();
        env.game_loader        = mResourceLoader.get();
        env.directory          = app::ToUtf8(mGameWorkingDir);
        // User home and game home will be unset for now.
        // todo: maybe use some temp folder ?
        // env.user_home          = ...
        // env.game_home          = ...
        mEngine->SetEnvironment(env);

        engine::Engine::InitParams params;
        params.editing_mode     = true; // allow changes to "static" content take place.
        params.preview_mode     = true; // yes, we're doing preview now!
        params.game_script      = app::ToUtf8(script);
        params.application_name = app::ToUtf8(settings.application_name);
        params.context          = mWindowContext.get();
        params.surface_width    = mSurface->width();
        params.surface_height   = mSurface->height();
        mEngine->Init(params);

        SetEngineConfig();

        if (!mEngine->Load())
        {
            Barf("Engine failed to load. Please see the log for more details.");
            return;
        }

        mEngine->Start();
        mTimer.Start();
        mFrameTimer.start();
        mInitDone = true;

        SetEnabled(mUI.toolBar,         true);
        SetEnabled(mUI.menuApplication, true);
        SetEnabled(mUI.menuSurface,     true);
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in engine init. [what='%1']", e.what());
        Barf(e.what());
    }
}

void PlayWindow::ActivateWindow()
{
    //mSurface->setKeyboardGrabEnabled(true);
    mSurface->raise();
    mSurface->requestActivate();
}

void PlayWindow::SelectResolution()
{
    auto* action = qobject_cast<QAction*>(sender());

    const auto index = action->data().toInt();
    const auto& list = app::ListResolutions();
    if (index == -1)
    {
        const auto& settings = mWorkspace.GetProjectSettings();
        ResizeSurface(settings.window_width, settings.window_height);
    }
    else
    {
        const auto& rez  = list[index];
        ResizeSurface(rez.width, rez.height);
    }
}

void PlayWindow::on_actionPause_toggled(bool val)
{
    SetDebugOptions();
    SetEnabled(mUI.actionStep, val);
}

void PlayWindow::on_actionStep_triggered()
{
    if (!mEngine)
        return;

    mEngine->Step();
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
    mAppEventLog.SetVisible(app::EventLogProxy::Show::Debug, val);
    mAppEventLog.invalidate();
}
void PlayWindow::on_actionLogShowInfo_toggled(bool val)
{
    mAppEventLog.SetVisible(app::EventLogProxy::Show::Info, val);
    mAppEventLog.invalidate();
}
void PlayWindow::on_actionLogShowWarning_toggled(bool val)
{
    mAppEventLog.SetVisible(app::EventLogProxy::Show::Warning, val);
    mAppEventLog.invalidate();
}
void PlayWindow::on_actionLogShowError_toggled(bool val)
{
    mAppEventLog.SetVisible(app::EventLogProxy::Show::Error, val);
    mAppEventLog.invalidate();
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
    if (!mEngine || !mInitDone)
        return;
    TemporaryCurrentDirChange cwd(mGameWorkingDir);
    try
    {
        mContext.makeCurrent(mSurface);
        mEngine->TakeScreenshot("screenshot.png");
        INFO("Wrote screenshot '%1'", "screenshot.png");
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in Engine::TakeScreenshot.");
        ERROR(e.what());
    }
}

void PlayWindow::on_actionEventLog_triggered()
{
    if (!mWinEventLog)
    {
        mWinEventLog.reset(new DlgEventLog(this));
        QByteArray geom;
        if (mWorkspace.GetUserProperty("play_window_event_dlg_geom", &geom))
            mWinEventLog->restoreGeometry(geom);
    }
    if (!mWinEventLog->isVisible())
        mWinEventLog->show();
}

void PlayWindow::on_actionReloadShaders_triggered()
{
    if (!mEngine || !mInitDone)
        return;
    mEngine->ReloadResources((unsigned)engine::Engine::ResourceType::Shaders);
    mResourceLoader->BlowCaches();
}
void PlayWindow::on_actionReloadTextures_triggered()
{
    if (!mEngine || !mInitDone)
        return;
    mEngine->ReloadResources((unsigned)engine::Engine::ResourceType::Textures);
    mResourceLoader->BlowCaches();
}

void PlayWindow::on_btnApplyFilter_clicked()
{
    mAppEventLog.SetFilterStr(GetValue(mUI.logFilter), GetValue(mUI.logFilterCaseSensitive));
    mAppEventLog.invalidate();
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
    if ((destination != mSurface) || !mEngine || !mInitDone)
        return QMainWindow::event(event);

    if (mWinEventLog && mWinEventLog->IsPlaying())
        return QMainWindow::event(event);

    ASSERT(!mEventQueue.full());

    if (event->type() == QEvent::KeyPress)
    {
        const auto* key_event = static_cast<const QKeyEvent *>(event);

        // this will collide with the application if the app wants to also use
        // the F11/F7/F9 key for something. But there aren't a lot of possibilities here..
        if (key_event->key() == Qt::Key_F11 && InFullScreen())
            SetFullScreen(false);
        else if (key_event->key() == Qt::Key_F7 && InFullScreen())
            mUI.actionPause->trigger();
        else if (key_event->key() == Qt::Key_F9 && InFullScreen())
            mUI.actionScreenshot->trigger();

        wdk::WindowEventKeyDown key;
        key.symbol    = MapVirtualKey(key_event->key());
        key.modifiers = MapKeyModifiers(key_event->modifiers());
        mEventQueue.push_back(key);

        if (mWinEventLog)
            mWinEventLog->RecordEvent(key, mTimer.SinceStart());
    }
    else if (event->type() == QEvent::KeyRelease)
    {
        const auto* key_event = static_cast<const QKeyEvent *>(event);

        wdk::WindowEventKeyUp key;
        key.symbol    = MapVirtualKey(key_event->key());
        key.modifiers = MapKeyModifiers(key_event->modifiers());
        mEventQueue.push_back(key);

        if (mWinEventLog)
            mWinEventLog->RecordEvent(key, mTimer.SinceStart());
    }
    else if (event->type() == QEvent::MouseMove)
    {
        const auto* mouse = static_cast<const QMouseEvent*>(event);

        wdk::WindowEventMouseMove move;
        move.window_x = mouse->x();
        move.window_y = mouse->y();
        move.global_x = mouse->globalX();
        move.global_y = mouse->globalY();
        move.modifiers = MapKeyModifiers(mouse->modifiers());
        move.btn       = MapMouseButton(mouse->button());
        mEventQueue.push_back(move);

        if (mWinEventLog)
            mWinEventLog->RecordEvent(move, mTimer.SinceStart());
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
        mEventQueue.push_back(press);

        if (mWinEventLog)
            mWinEventLog->RecordEvent(press, mTimer.SinceStart());
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
        mEventQueue.push_back(release);

        if (mWinEventLog)
            mWinEventLog->RecordEvent(release, mTimer.SinceStart());
    }
    if (event->type() == QEvent::Resize)
    {
        wdk::WindowEventResize resize;
        resize.width  = mSurface->width();
        resize.height = mSurface->height();
        mEventQueue.push_back(resize);
    }
    else return QMainWindow::event(event);

    return true;
}

void PlayWindow::DebugPause(bool pause)
{
    SetValue(mUI.actionPause, pause);
    SetEnabled(mUI.actionStep, pause);
    SetDebugOptions();
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
        mEngine->OnEnterFullScreen();

        mEngine->DebugPrintString("Press F11 to return to windowed mode.");
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
        mEngine->OnLeaveFullScreen();
    }
    // todo: should really only set this flag when the window *did* go into
    // fullscreen mode.
    mFullScreen = fullscreen;

    ActivateWindow();

    SetDebugOptions();
}

void PlayWindow::SetDebugOptions() const
{
    engine::Engine::DebugOptions debug;
    debug.debug_draw_flags.set_from_value(~0);
    debug.debug_pause     = GetValue(mUI.actionPause);
    debug.debug_draw      = GetValue(mUI.actionToggleDebugDraw);
    debug.debug_show_msg  = GetValue(mUI.actionToggleDebugMsg);
    debug.debug_font      = "app://fonts/orbitron-medium.otf";
    debug.debug_show_fps  = InFullScreen();
    debug.debug_print_fps = false;
    mEngine->SetDebugOptions(debug);

    // right now we only have a UI for toggling the debug logs,
    // so keep everything else turned on
    const auto log_debug = GetValue(mUI.actionToggleDebugLog);
    const auto log_warn  = true;
    const auto log_info  = true;
    const auto log_error = true;
    mGameLibSetGlobalLogger(mLogger.get(), log_debug, log_warn, log_info, log_error);
}

void PlayWindow::SetEngineConfig() const
{
    const auto& settings = mWorkspace.GetProjectSettings();

    engine::Engine::EngineConfig config;
    config.ticks_per_second                = settings.ticks_per_second;
    config.updates_per_second              = settings.updates_per_second;
    config.physics.enabled                 = settings.enable_physics;
    config.physics.num_velocity_iterations = settings.num_velocity_iterations;
    config.physics.num_position_iterations = settings.num_position_iterations;
    config.physics.gravity                 = settings.physics_gravity;
    config.physics.scale                   = settings.physics_scale;
    config.default_mag_filter              = settings.default_mag_filter;
    config.default_min_filter              = settings.default_min_filter;
    config.clear_color                     = ToGfx(settings.clear_color);
    config.mouse_cursor.show               = settings.mouse_pointer_visible;
    config.mouse_cursor.material           = app::ToUtf8(settings.mouse_pointer_material);
    config.mouse_cursor.drawable           = app::ToUtf8(settings.mouse_pointer_drawable);
    config.mouse_cursor.hotspot            = settings.mouse_pointer_hotspot;
    config.mouse_cursor.size               = settings.mouse_pointer_size;
    config.audio.sample_type               = settings.audio_sample_type;
    config.audio.sample_rate               = settings.audio_sample_rate;
    config.audio.buffer_size               = settings.audio_buffer_size;
    config.audio.channels                  = settings.audio_channels;
    config.audio.enable_pcm_caching        = settings.enable_audio_pcm_caching;
    if (settings.mouse_pointer_units == app::Workspace::ProjectSettings::MousePointerUnits::Pixels)
        config.mouse_cursor.units = engine::Engine::EngineConfig::MouseCursorUnits::Pixels;
    else if (settings.mouse_pointer_units == app::Workspace::ProjectSettings::MousePointerUnits::Units)
        config.mouse_cursor.units = engine::Engine::EngineConfig::MouseCursorUnits::Units;
    else BUG("Unhandled mouse cursor/pointer units.");

    mEngine->SetEngineConfig(config);
}

void PlayWindow::Barf(const std::string& msg)
{
    mEngine.reset();
    mContainer->setVisible(false);
    mUI.problem->setVisible(true);
    SetValue(mUI.lblError, msg);
    SetEnabled(mUI.toolBar,         false);
    SetEnabled(mUI.menuApplication, false);
    SetEnabled(mUI.menuSurface,     false);
}

bool PlayWindow::LoadLibrary()
{
    TemporaryCurrentDirChange cwd(mGameWorkingDir);
    const auto& settings = mWorkspace.GetProjectSettings();
    const auto& library  = mWorkspace.MapFileToFilesystem(settings.GetApplicationLibrary());
    mLibrary.setFileName(library);
    mLibrary.setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!mLibrary.load())
    {
        Barf("Failed to load engine library.");
        ERROR("Failed to load engine library. [file='%1, error='%2']", library, mLibrary.errorString());
        return false;
    }

    mGameLibCreateEngine    = (Gamestudio_CreateEngineFunc)mLibrary.resolve("Gamestudio_CreateEngine");
    mGameLibSetGlobalLogger = (Gamestudio_SetGlobalLoggerFunc)mLibrary.resolve("Gamestudio_SetGlobalLogger");
    if (!mGameLibCreateEngine)
        ERROR("Failed to resolve 'Gamestudio_CreateEngine'.");
    else if (!mGameLibSetGlobalLogger)
        ERROR("Failed to resolve 'Gamestudio_SetGlobalLogger'.");
    if (!mGameLibCreateEngine || !mGameLibSetGlobalLogger)
    {
        Barf("Failed to resolve engine library entry points.");
        return false;
    }

    // right now we only have a UI for toggling the debug logs,
    // so keep everything else turned on
    const auto log_debug = GetValue(mUI.actionToggleDebugLog);
    const auto log_warn  = true;
    const auto log_info  = true;
    const auto log_error = true;
    mGameLibSetGlobalLogger(mLogger.get(), log_debug, log_warn, log_info, log_error);

    std::unique_ptr<engine::Engine> engine(mGameLibCreateEngine());
    if (!engine)
    {
        Barf("Failed to create engine instance.");
        ERROR("Failed to create engine instance.");
        return false;
    }
    mEngine = std::move(engine);
    return true;
}


} // namespace
