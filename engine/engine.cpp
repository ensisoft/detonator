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

#include "config.h"

// this translation unit implements the engine library interface.
#define GAMESTUDIO_GAMELIB_IMPLEMENTATION

#include "warnpush.h"
#  include <neargye/magic_enum.hpp>
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <stack>
#include <cstring>
#include <cmath>

#include "base/logging.h"
#include "base/format.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/resource.h"
#include "engine/main/interface.h"
#include "engine/engine.h"
#include "engine/animation.h"
#include "engine/classlib.h"
#include "engine/renderer.h"
#include "engine/entity.h"
#include "engine/physics.h"
#include "engine/game.h"
#include "engine/lua.h"
#include "engine/format.h"
#include "engine/treeop.h"
#include "engine/ui.h"
#include "uikit/window.h"
#include "uikit/painter.h"
#include "uikit/state.h"

namespace
{

// Default game engine implementation. Implements the main App interface
// which is the interface that enables the game host to communicate
// with the application/game implementation in order to update/tick/etc.
// the game and also to handle input from keyboard and mouse.
class DefaultGameEngine : public game::App,
                          public wdk::WindowListener
{
public:
    DefaultGameEngine() : mDebugPrints(10)
    {}

    virtual bool GetNextRequest(Request* out) override
    { return mRequests.GetNext(out); }

    // Application implementation
    virtual void Start()
    {
        DEBUG("Engine starting.");
        mGame->LoadGame(mClasslib);
    }
    virtual void Init(gfx::Device::Context* context, unsigned surface_width, unsigned surface_height) override
    {
        DEBUG("Engine initializing. Surface %1x%2", surface_width, surface_height);
        mDevice  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, context);
        mPainter = gfx::Painter::Create(mDevice);
        mPainter->SetSurfaceSize(surface_width, surface_height);
        mSurfaceWidth  = surface_width;
        mSurfaceHeight = surface_height;
        mGame = std::make_unique<game::LuaGame>(mDirectory + "/lua");
        mGame->SetPhysicsEngine(&mPhysics);
        mScripting = std::make_unique<game::ScriptEngine>(mDirectory + "/lua");
        mScripting->SetLoader(mClasslib);
        mScripting->SetPhysicsEngine(&mPhysics);
        mUIStyle.SetLoader(mClasslib);
        mUIPainter.SetPainter(mPainter.get());
        mUIPainter.SetStyle(&mUIStyle);
    }
    virtual void SetDebugOptions(const DebugOptions& debug) override
    {
        mDebug = debug;
        base::EnableDebugLog(mDebug.debug_log);
    }
    virtual void DebugPrintString(const std::string& message) override
    {
        DebugPrint print;
        print.message = message;
        mDebugPrints.push_back(std::move(print));
    }

    virtual void SetEnvironment(const Environment& env) override
    {
        mClasslib  = env.classlib;
        mContent   = env.content;
        mDirectory = env.directory;
        mRenderer.SetLoader(mClasslib);
        mPhysics.SetLoader(mClasslib);
        // set the unfortunate global gfx loader
        gfx::SetResourceLoader(env.loader);
    }
    virtual void SetEngineConfig(const EngineConfig& conf) override
    {
        mPhysics.SetScale(conf.physics.scale);
        mPhysics.SetGravity(conf.physics.gravity);
        mPhysics.SetNumPositionIterations(conf.physics.num_position_iterations);
        mPhysics.SetNumVelocityIterations(conf.physics.num_velocity_iterations);
        mPhysics.SetTimestep(1.0f / conf.updates_per_second);
        mDevice->SetDefaultTextureFilter(conf.default_min_filter);
        mDevice->SetDefaultTextureFilter(conf.default_mag_filter);
        mClearColor = conf.clear_color;
        mGameTimeStep = 1.0f / conf.updates_per_second;
        mGameTickStep = 1.0f / conf.ticks_per_second;
    }

    virtual void Draw() override
    {
        mDevice->BeginFrame();
        mDevice->ClearColor(mClearColor);

        // get the game's logical viewport into the game world.
        const auto& view = mGame->GetViewport();
        // map the logical viewport to some area in the rendering surface
        // so that the rendering area (the device viewport) has the same
        // aspect ratio as the logical viewport.
        const float width       = view.GetWidth();
        const float height      = view.GetHeight();
        const float surf_width  = (float)mSurfaceWidth;
        const float surf_height = (float)mSurfaceHeight;
        const float scale = std::min(surf_width / width, surf_height / height);
        const float device_viewport_width = width * scale;
        const float device_viewport_height = height * scale;
        const float device_viewport_x = (surf_width - device_viewport_width) / 2;
        const float device_viewport_y = (surf_height - device_viewport_height) / 2;
        if (mDebug.debug_draw)
        {
            mPainter->SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
            mPainter->SetOrthographicView(0.0f , 0.0f , mSurfaceWidth , mSurfaceHeight);
            gfx::DrawRectOutline(*mPainter, gfx::FRect(device_viewport_x, device_viewport_y,
                                                     device_viewport_width, device_viewport_height),
                                 gfx::Color::Green, 1.0f);
        }

        // set the actual viewport for proper clipping.
        mPainter->SetViewport(device_viewport_x, device_viewport_y, device_viewport_width, device_viewport_height);
        // set the pixel ratio for mapping game units to rendering surface units.
        mPainter->SetPixelRatio(glm::vec2(scale, scale));

        mRenderer.BeginFrame();

        if (mScene)
        {
            // low level draw packet filter for culling draw packets
            // that fall outside of the current viewport.
            class Culler : public game::EntityInstanceDrawHook {
            public:
                Culler(const game::FRect& view) : mViewRect(view)
                {}
                virtual bool InspectPacket(const game::EntityNode* node, game::DrawPacket& packet) override
                {
                    const auto& rect = game::ComputeBoundingRect(packet.transform);
                    if (!DoesIntersect(rect, mViewRect))
                        return false;
                    return true;
                }
            private:
                const game::FRect& mViewRect;
            };
            Culler cull(view);
            // set the logical viewport to whatever the game
            // has set it.
            mPainter->SetOrthographicView(view);

            gfx::Transform transform;
            mRenderer.Draw(*mScene , *mPainter , transform, nullptr, &cull);
            if (mDebug.debug_draw && mPhysics.HaveWorld())
            {
                mPhysics.DebugDrawObjects(*mPainter , transform);
            }
        }

        if (auto* ui = GetUI())
        {
            const float width  = ui->GetWidth();
            const float height = ui->GetHeight();
            const float scale = std::min(surf_width / width, surf_height / height);
            const float device_viewport_width = width * scale;
            const float device_viewport_height = height * scale;

            mPainter->SetPixelRatio(glm::vec2(1.0f, 1.0f));
            mPainter->SetOrthographicView(0, 0, width, height);
            mPainter->SetViewport((surf_width - device_viewport_width)*0.5,
                                  (surf_height - device_viewport_height)*0.5,
                                  device_viewport_width, device_viewport_height);
            ui->Paint(mUIState, mUIPainter, base::GetRuntimeSec(), nullptr);
        }

        if (mDebug.debug_show_fps || mDebug.debug_show_msg)
        {
            mPainter->SetPixelRatio(glm::vec2(1.0f, 1.0f));
            mPainter->SetOrthographicView(0, 0, surf_width, surf_height);
            mPainter->SetViewport(0, 0, surf_width, surf_height);
        }
        if (mDebug.debug_show_fps)
        {
            char hallelujah[512] = {0};
            std::snprintf(hallelujah, sizeof(hallelujah) - 1,
            "FPS: %.2f wall time: %.2f frames: %u",
                mLastStats.current_fps, mLastStats.total_wall_time, mLastStats.num_frames_rendered);

            const gfx::FRect rect(10, 10, 500, 20);
            gfx::FillRect(*mPainter, rect, gfx::Color4f(gfx::Color::Black, 0.4f));
            gfx::DrawTextRect(*mPainter, hallelujah,
                mDebug.debug_font, 14, rect, gfx::Color::HotPink,
                gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
        }
        if (mDebug.debug_show_msg)
        {
            gfx::FRect rect(10, 30, 500, 20);
            for (const auto& print : mDebugPrints)
            {
                gfx::FillRect(*mPainter, rect, gfx::Color4f(gfx::Color::Black, 0.4f));
                gfx::DrawTextRect(*mPainter, print.message,
                    mDebug.debug_font, 14, rect, gfx::Color::HotPink,
                   gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
                rect.Translate(0, 20);
            }
        }

        mRenderer.EndFrame();
        mDevice->EndFrame(true);
        mDevice->CleanGarbage(120);
    }

    virtual void BeginMainLoop() override
    {
        if (mScene)
        {
            mScene->BeginLoop();
            mScripting->BeginLoop();
        }
    }

    virtual void Update(double wall_time, double dt) override
    {
        // there's plenty of information about different ways to write a basic
        // game rendering loop. here are some suggested references:
        // https://gafferongames.com/post/fix_your_timestep/
        // Game Engine Architecture by Jason Gregory
        mTimeAccum += dt;

        // do simulation/animation update steps.
        while (mTimeAccum >= mGameTimeStep)
        {
            mGameTimeTotal += mGameTimeStep;
            mTimeAccum -= mGameTimeStep;
            mTickAccum += mGameTimeStep;
            UpdateGame(mGameTimeTotal, mGameTimeStep);

            // put some accumulated time towards ticking game.
            auto tick_time = mGameTimeTotal;

            while (mTickAccum >= mGameTickStep)
            {
                TickGame(tick_time, mGameTickStep);
                mTickAccum -= mGameTickStep;
                tick_time += mGameTickStep;
            }
        }

        if (HaveOpenUI())
        {
            mUIPainter.Update(wall_time, dt);
        }
    }
    virtual void EndMainLoop() override
    {
        if (mScene)
        {
            mScripting->EndLoop();
            mScene->EndLoop();
        }

        game::Action action;
        while (mGame->GetNextAction(&action) || mScripting->GetNextAction(&action))
        {
            if (auto* ptr = std::get_if<game::OpenUIAction>(&action))
                OpenUI(ptr->ui);
            else if (auto* ptr = std::get_if<game::CloseUIAction>(&action))
                CloseUI(ptr->result);
            else if (auto* ptr = std::get_if<game::PlayAction>(&action))
                PlayGame(ptr->klass);
            else if (auto* ptr = std::get_if<game::SuspendAction>(&action))
                SuspendGame();
            else if (auto* ptr = std::get_if<game::ResumeAction>(&action))
                ResumeGame();
            else if (auto* ptr = std::get_if<game::QuitAction>(&action))
                QuitGame();
            else if (auto* ptr = std::get_if<game::StopAction>(&action))
                StopGame();
            else if (auto* ptr = std::get_if<game::DebugClearAction>(&action))
                mDebugPrints.clear();
            else if (auto* ptr = std::get_if<game::DebugPrintAction>(&action))  {
                if (ptr->clear)
                    mDebugPrints.clear();
                DebugPrintString(ptr->message);
            }
        }
    }

    virtual void Shutdown() override
    {
        DEBUG("Engine shutdown");
        gfx::SetResourceLoader(nullptr);
        mDevice.reset();
    }
    virtual bool IsRunning() const override
    { return mRunning; }
    virtual wdk::WindowListener* GetWindowListener() override
    { return this; }

    virtual void UpdateStats(const Stats& stats) override
    {
        if (mDebug.debug_show_fps)
        {
            mLastStats = stats;
        }
        if (mDebug.debug_print_fps)
        {
            DEBUG("fps: %1, wall_time: %2, frames: %3",
                  stats.current_fps, stats.total_wall_time, stats.num_frames_rendered);
        }

        for (auto it = mDebugPrints.begin(); it != mDebugPrints.end();)
        {
            auto& print = *it;
            if (--print.lifetime < 0) {
                it = mDebugPrints.erase(it);
            } else ++it;
        }
    }
    virtual void TakeScreenshot(const std::string& filename) const override
    {
        const auto& rgba = mDevice->ReadColorBuffer(mSurfaceWidth, mSurfaceHeight);
        // pre-multiply alpha, STB image write with semi transparent pixels
        // aren't really the expected output visually. should this just discard alpha?
        gfx::Bitmap<gfx::RGB> rgb;
        rgb.Resize(rgba.GetWidth(), rgba.GetHeight());
        for (unsigned y=0; y<rgba.GetHeight(); ++y) {
            for (unsigned x=0; x<rgba.GetWidth(); ++x) {
                const auto src = rgba.GetPixel(y, x);
                const auto alpha = src.a / 255.0;
                const auto dst = gfx::RGB(src.r * alpha, src.g * alpha, src.b * alpha);
                rgb.SetPixel(y, x, dst);
            }
        }
        gfx::WritePNG(rgb, filename);
        INFO("Wrote screenshot '%1'", filename);
    }

    virtual void OnRenderingSurfaceResized(unsigned width, unsigned height) override
    {
        DEBUG("Rendering surface resized to %1x%2", width, height);
        mSurfaceWidth = width;
        mSurfaceHeight = height;
        mPainter->SetSurfaceSize(width, height);
    }
    virtual void OnEnterFullScreen() override
    {
        DEBUG("Enter full screen mode.");
        mFullScreen = true;
    }
    virtual void OnLeaveFullScreen() override
    {
        DEBUG("Leave full screen mode.");
        mFullScreen = false;
    }

    // WindowListener
    virtual void OnWantClose(const wdk::WindowEventWantClose&) override
    {
        // todo: handle ending play, saving game etc.
        mRunning = false;
    }
    virtual void OnKeydown(const wdk::WindowEventKeydown& key) override
    {
        //DEBUG("OnKeyDown: %1, %2", ModifierString(key.modifiers), magic_enum::enum_name(key.symbol));
        mGame->OnKeyDown(key);
        if (mScene)
        {
            mScripting->OnKeyDown(key);
        }
    }
    virtual void OnKeyup(const wdk::WindowEventKeyup& key) override
    {
        //DEBUG("OnKeyUp: %1, %2", ModifierString(key.modifiers), magic_enum::enum_name(key.symbol));
        mGame->OnKeyUp(key);
        if (mScene)
        {
            mScripting->OnKeyUp(key);
        }
    }
    virtual void OnChar(const wdk::WindowEventChar& text) override
    {
        mGame->OnChar(text);
    }
    virtual void OnMouseMove(const wdk::WindowEventMouseMove& mouse) override
    {
        if (HaveOpenUI())
        {
            SendMouseEvent(MapMouseEvent(mouse), &uik::Window::MouseMove);
        }

        mGame->OnMouseMove(mouse);
    }
    virtual void OnMousePress(const wdk::WindowEventMousePress& mouse) override
    {
        if (HaveOpenUI())
        {
            SendMouseEvent(MapMouseEvent(mouse), &uik::Window::MousePress);
        }

        mGame->OnMousePress(mouse);
    }
    virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse) override
    {
        if (HaveOpenUI())
        {
            SendMouseEvent(MapMouseEvent(mouse), &uik::Window::MouseRelease);
        }

        mGame->OnMouseRelease(mouse);
    }
private:
    using MouseFunc = uik::Window::WidgetAction (uik::Window::*)(const uik::Window::MouseEvent&, uik::State&);
    void SendMouseEvent(const uik::Window::MouseEvent& mickey, MouseFunc which)
    {
        auto* ui = GetUI();

        auto action = (ui->*which)(mickey, mUIState);
        if (action.type == uik::WidgetActionType::None)
            return;
        mGame->OnUIAction(action);
        //DEBUG("Widget action: '%1'", action.type);
    }
    template<typename WdkMouseEvent>
    uik::Window::MouseEvent MapMouseEvent(const WdkMouseEvent& mickey) const
    {
        const auto* ui     = GetUI();
        const float width  = ui->GetWidth();
        const float height = ui->GetHeight();
        const float scale = std::min((float)mSurfaceWidth/width, (float)mSurfaceHeight/height);
        const glm::vec2 window_size(width, height);
        const glm::vec2 surface_size(mSurfaceWidth, mSurfaceHeight);
        const glm::vec2 viewport_size = glm::vec2(width, height) * scale;
        const glm::vec2 viewport_origin = (surface_size - viewport_size) * glm::vec2(0.5, 0.5);
        const glm::vec2 mickey_pos_win(mickey.window_x, mickey.window_y);
        const glm::vec2 mickey_pos_uik = (mickey_pos_win - viewport_origin) / scale;

        uik::Window::MouseEvent event;
        event.time   = base::GetRuntimeSec();
        event.button = MapMouseButton(mickey.btn);
        event.native_mouse_pos = uik::FPoint(mickey_pos_win.x, mickey_pos_win.y);
        event.window_mouse_pos = uik::FPoint(mickey_pos_uik.x, mickey_pos_uik.y);
        //DEBUG("win: %1, uik: %2", event.native_mouse_pos, event.window_mouse_pos);
        return event;
    }
    uik::MouseButton MapMouseButton(const wdk::MouseButton btn) const
    {
        if (btn == wdk::MouseButton::None)
            return uik::MouseButton::None;
        else if (btn == wdk::MouseButton::Left)
            return uik::MouseButton::Left;
        else if (btn == wdk::MouseButton::Right)
            return uik::MouseButton::Right;
        else if (btn == wdk::MouseButton::Wheel)
            return uik::MouseButton::Wheel;
        else if (btn == wdk::MouseButton::WheelScrollUp)
            return uik::MouseButton::WheelUp;
        else if (btn == wdk::MouseButton::WheelScrollDown)
            return uik::MouseButton::WheelDown;
        WARN("Unmapped wdk mouse button '%1'", btn);
        return uik::MouseButton::None;
    }
    inline uik::Window* GetUI()
    {
        if (mUI.empty())
            return nullptr;
        return mUI.top().get();
    }
    inline const uik::Window* GetUI() const
    {
        if (mUI.empty())
            return nullptr;
        return mUI.top().get();
    }
    bool HaveOpenUI() const
    { return !mUI.empty(); }


    void OpenUI(game::ClassHandle<uik::Window> window)
    {
        // todo: if the style loading somehow fails, then what?
        mUIStyle.ClearProperties();
        mUIStyle.ClearMaterials();
        mUIState.Clear();

        auto style_data = mContent->LoadGameData(window->GetStyleName());
        if (!style_data || !mUIStyle.LoadStyle(*style_data))
        {
            ERROR("The UI style ('%1') could not be loaded.", window->GetStyleName());
            return;
        }
        DEBUG("Loaded UI style '%1'", window->GetStyleName());

        // there's no "class" object for the UI system so we're just
        // going to create a mutable copy and put that on the UI stack.
        auto ui = std::make_unique<uik::Window>(*window);
        ui->Style(mUIPainter);
        // push the window to the top of the UI stack. this is the new
        // currently active UI
        mUI.push(std::move(ui));
        mUIPainter.DeleteMaterialInstances();

        mGame->OnUIOpen(mUI.top().get());
    }
    void CloseUI(int result)
    {
        if (mUI.empty())
            return;

        mGame->OnUIClose(mUI.top().get(), result);
        mUI.pop();

        if (auto* ui = GetUI())
        {
            mUIPainter.DeleteMaterialInstances();
            mUIStyle.ClearProperties();
            mUIStyle.ClearMaterials();
            mUIState.Clear();
            auto style_data = mContent->LoadGameData(ui->GetStyleName());
            if (!style_data || !mUIStyle.LoadStyle(*style_data))
            {
                ERROR("The UI style ('%1') could not be loaded.", ui->GetStyleName());
                return;
            }
            ui->Style(mUIPainter);
        }
    }
    void PlayGame(game::ClassHandle<game::SceneClass> klass)
    {
        mScene = game::CreateSceneInstance(klass);
        mPhysics.DeleteAll();
        mPhysics.CreateWorld(*mScene);
        mScripting->BeginPlay(mScene.get());
        mGame->BeginPlay(mScene.get());
    }
    void SuspendGame()
    {

    }
    void ResumeGame()
    {

    }
    void StopGame()
    {
        mGame->EndPlay(mScene.get());
        mScripting->EndPlay(mScene.get());
        mScene.reset();
    }
    void QuitGame()
    {
        // todo: cleanup?

        mRequests.Quit();
    }
    void UpdateGame(double game_time,  double dt)
    {
        if (mScene)
        {
            mScene->Update(dt);
            if (mPhysics.HaveWorld())
            {
                std::vector<game::ContactEvent> contacts;
                mPhysics.Tick(&contacts);
                mPhysics.UpdateScene(*mScene);
                for (const auto& contact : contacts)
                {
                    mGame->OnContactEvent(contact);
                    mScripting->OnContactEvent(contact);
                }
            }
            mRenderer.Update(*mScene, game_time, dt);
            mScripting->Update(game_time, dt);
        }
        mGame->Update(game_time, dt);
    }

    void TickGame(double game_time, double dt)
    {
        mGame->Tick(game_time, dt);
        if (mScene)
        {
            mScripting->Tick(game_time, dt);
        }
    }

    static std::string ModifierString(wdk::bitflag<wdk::Keymod> mods)
    {
        std::string ret;
        if (mods.test(wdk::Keymod::Control))
            ret += "Ctrl+";
        if (mods.test(wdk::Keymod::Shift))
            ret += "Shift+";
        if (mods.test(wdk::Keymod::Alt))
            ret += "Alt+";
        if (!ret.empty())
            ret.pop_back();
        return ret;
    }

private:
    unsigned mSurfaceWidth  = 0;
    unsigned mSurfaceHeight = 0;
    gfx::Color4f mClearColor = {0.2f, 0.3f, 0.4f, 1.0f};
    // game dir where the executable is.
    std::string mDirectory;
    // queue of outgoing requests regarding the environment
    // such as the window size/position etc that the game host
    // may/may not support.
    game::AppRequestQueue mRequests;
    // Interface for accessing the game classes and resources
    // such as animations, materials etc.
    game::ClassLibrary* mClasslib = nullptr;
    // Game data/content loader.
    game::GameDataLoader* mContent = nullptr;
    // The graphics painter device.
    std::unique_ptr<gfx::Painter> mPainter;
    // The graphics device.
    std::shared_ptr<gfx::Device> mDevice;
    // The rendering subsystem.
    game::Renderer mRenderer;
    // The physics subsystem.
    game::PhysicsEngine mPhysics;
    // The UI painter for painting UIs
    game::UIPainter mUIPainter;
    game::UIStyle mUIStyle;
    // The scripting subsystem.
    std::unique_ptr<game::ScriptEngine> mScripting;
    // Current game scene or nullptr if no scene.
    std::unique_ptr<game::Scene> mScene;
    // Game logic implementation.
    std::unique_ptr<game::Game> mGame;
    // The UI stack onto which UIs are opened.
    // The top of the stack is the currently "active" UI
    // that gets the mouse/keyboard input events. It's
    // possible that the stack is empty if the game is
    // not displaying any UI
    std::stack<std::unique_ptr<uik::Window>> mUI;
    // Transient UI state.
    uik::State mUIState;
    // flag to indicate whether the app is still running or not.
    bool mRunning = true;
    // a flag to indicate whether currently in fullscreen or not.
    bool mFullScreen = false;
    // current debug options.
    game::App::DebugOptions mDebug;
    // last statistics about the rendering rate etc.
    game::App::Stats mLastStats;
    // list of current debug print messages that
    // get printed to the display.
    struct DebugPrint {
        std::string message;
        short lifetime = 3;
    };
    boost::circular_buffer<DebugPrint> mDebugPrints;
    // The size of the game time step (in seconds) to take for each
    // update of the game simulation state.
    float mGameTimeStep = 0.0f;
    // The size of the game tick step (in seconds) to take for each
    // tick of the game staete.
    float mGameTickStep = 0.0f;
    // accumulation counters for keeping track of left over time
    // available for taking update and tick steps.
    float mTickAccum = 0.0f;
    float mTimeAccum = 0.0f;
    // Total accumulated game time so far.
    double mGameTimeTotal = 0.0f;
};

} //namespace

extern "C" {
GAMESTUDIO_EXPORT game::App* Gamestudio_CreateApp()
{
    DEBUG("Engine");
    return new DefaultGameEngine;
}
} // extern C
