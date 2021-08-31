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

#include "base/logging.h"
#include "game/entity.h"
#include "game/treeop.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/resource.h"
#include "graphics/material.h"
#include "engine/main/interface.h"
#include "engine/audio.h"
#include "engine/classlib.h"
#include "engine/renderer.h"
#include "engine/event.h"
#include "engine/physics.h"
#include "engine/game.h"
#include "engine/lua.h"
#include "engine/format.h"
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
class GameStudioEngine : public engine::Engine,
                         public wdk::WindowListener
{
public:
    GameStudioEngine() : mDebugPrints(10)
    {}

    virtual bool GetNextRequest(Request* out) override
    { return mRequests.GetNext(out); }

    // Application implementation
    virtual void Start()
    {
        DEBUG("Engine starting.");
        mAudio->Start();

        mGame->LoadGame(mClasslib);
    }
    virtual void Init(const InitParams& init) override
    {
        DEBUG("Engine initializing. Surface %1x%2", init.surface_width, init.surface_height);
        mAudio = std::make_unique<engine::AudioEngine>(init.application_name);
        mAudio->SetClassLibrary(mClasslib);
        mAudio->SetLoader(mAudioLoader);
        mDevice  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, init.context);
        mPainter = gfx::Painter::Create(mDevice);
        mPainter->SetSurfaceSize(init.surface_width, init.surface_height);
        mSurfaceWidth  = init.surface_width;
        mSurfaceHeight = init.surface_height;
        mGame = std::make_unique<engine::LuaGame>(mDirectory + "/lua", init.game_script, mGameHome, init.application_name);
        mGame->SetPhysicsEngine(&mPhysics);
        mGame->SetAudioEngine(mAudio.get());
        mScripting = std::make_unique<engine::ScriptEngine>(mDirectory + "/lua");
        mScripting->SetClassLibrary(mClasslib);
        mScripting->SetPhysicsEngine(&mPhysics);
        mScripting->SetAudioEngine(mAudio.get());
        mUIStyle.SetClassLibrary(mClasslib);
        mUIPainter.SetPainter(mPainter.get());
        mUIPainter.SetStyle(&mUIStyle);
        mRenderer.SetClassLibrary(mClasslib);
        mPhysics.SetClassLibrary(mClasslib);
    }
    virtual void SetDebugOptions(const DebugOptions& debug) override
    {
        mDebug = debug;
        base::EnableDebugLog(mDebug.debug_log);

        if (mAudio)
            mAudio->SetDebugPause(debug.debug_pause);
    }
    virtual void DebugPrintString(const std::string& message) override
    {
        DebugPrint print;
        print.message = message;
        mDebugPrints.push_back(std::move(print));
    }

    virtual void SetEnvironment(const Environment& env) override
    {
        mClasslib       = env.classlib;
        mGameDataLoader = env.game_data_loader;
        mAudioLoader    = env.audio_loader;
        mDirectory      = env.directory;
        mGameHome       = env.game_home;

        // set the unfortunate global gfx loader
        gfx::SetResourceLoader(env.graphics_loader);
        DEBUG("Install directory '%1'.", env.directory);
        DEBUG("Game home '%1'.", env.game_home);
        DEBUG("User home '%1'.", env.user_home);
    }
    virtual void SetEngineConfig(const EngineConfig& conf) override
    {
        audio::Format audio_format;
        audio_format.channel_count = static_cast<unsigned>(conf.audio.channels); // todo: use enum
        audio_format.sample_rate   = conf.audio.sample_rate;
        audio_format.sample_type   = conf.audio.sample_type;
        mAudio->SetFormat(audio_format);
        mAudio->SetBufferSize(conf.audio.buffer_size);
        DEBUG("Audio format set to %1", audio_format);
        DEBUG("Audio buffer size set to %1ms", conf.audio.buffer_size);

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
        auto mouse_drawble = mClasslib->FindDrawableClassById(conf.mouse_cursor.drawable);
        DEBUG("Mouse material='%1',drawable='%2'", conf.mouse_cursor.material, conf.mouse_cursor.drawable);
        if (!mouse_drawble) {
            WARN("No such mouse cursor drawable found '%1'.", conf.mouse_cursor.drawable);
            mouse_drawble = std::make_shared<gfx::CursorClass>(gfx::CursorClass::Shape::Arrow);
            mCursorSize    = glm::vec2(20.0f, 20.0f);
            mCursorHotspot = glm::vec2(0.0f, 0.0f);
        } else {
            mCursorSize    = conf.mouse_cursor.size;
            mCursorHotspot = conf.mouse_cursor.hotspot;
        }
        auto mouse_material = mClasslib->FindMaterialClassById(conf.mouse_cursor.material);
        if (!mouse_material) {
            WARN("No such mouse cursor drawable found '%1'.", conf.mouse_cursor.material);
            mouse_material = std::make_shared<gfx::ColorClass>(gfx::Color::HotPink);
        }
        mMouseDrawable = gfx::CreateDrawableInstance(mouse_drawble);
        mMouseMaterial = gfx::CreateMaterialInstance(mouse_material);
        mShowMouseCursor = conf.mouse_cursor.show;
    }

    virtual void Draw() override
    {
        mDevice->BeginFrame();
        mDevice->ClearColor(mClearColor);

        const float surf_width  = (float)mSurfaceWidth;
        const float surf_height = (float)mSurfaceHeight;

        if (mScene)
        {

            // get the game's logical viewport into the game world.
            const auto& view = mGame->GetViewport();
            // map the logical viewport to some area in the rendering surface
            // so that the rendering area (the device viewport) has the same
            // aspect ratio as the logical viewport.
            const float width  = view.GetWidth();
            const float height = view.GetHeight();
            const float scale  = std::min(surf_width / width, surf_height / height);
            // low level draw packet filter for culling draw packets
            // that fall outside of the current viewport.
            class Culler : public engine::EntityInstanceDrawHook {
            public:
                Culler(const engine::FRect& view) : mViewRect(view)
                {}
                virtual bool InspectPacket(const game::EntityNode* node, engine::DrawPacket& packet) override
                {
                    const auto& rect = game::ComputeBoundingRect(packet.transform);
                    if (!DoesIntersect(rect, mViewRect))
                        return false;
                    return true;
                }
            private:
                const engine::FRect& mViewRect;
            };
            Culler cull(view);

            // set the actual device viewport for rendering into the window buffer.
            // the device viewport retains the game's logical viewport aspect ratio
            // and is centered in the middle of the rendering surface.
            mPainter->SetViewport(GetViewport());
            // set the logical viewport to whatever the game has set it.
            mPainter->SetOrthographicView(view);
            // set the pixel ratio for mapping game units to rendering surface units.
            mPainter->SetPixelRatio(glm::vec2(scale, scale));

            gfx::Transform transform;
            mRenderer.BeginFrame();
            mRenderer.Draw(*mScene , *mPainter , transform, nullptr, &cull);
            if (mDebug.debug_draw && mPhysics.HaveWorld())
            {
                mPhysics.DebugDrawObjects(*mPainter , transform);
            }
            mRenderer.EndFrame();
        }

        if (auto* ui = GetUI())
        {
            const float width  = ui->GetWidth();
            const float height = ui->GetHeight();
            const float scale  = std::min(surf_width / width, surf_height / height);
            const float device_viewport_width = width * scale;
            const float device_viewport_height = height * scale;

            mPainter->SetPixelRatio(glm::vec2(1.0f, 1.0f));
            mPainter->SetOrthographicView(0, 0, width, height);
            // Set the actual device viewport for rendering into the window surface.
            // the viewport retains the UI's aspect ratio and is centered in the middle
            // of the rendering surface.
            mPainter->SetViewport((surf_width - device_viewport_width)*0.5,
                                  (surf_height - device_viewport_height)*0.5,
                                  device_viewport_width, device_viewport_height);
            ui->Paint(mUIState, mUIPainter, base::GetTime(), nullptr);
        }

        if (mDebug.debug_show_fps || mDebug.debug_show_msg || mDebug.debug_draw || mShowMouseCursor)
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
        if (mDebug.debug_show_msg && mShowDebugs)
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
        if (mDebug.debug_draw)
        {
            // visualize the game's logical viewport in the window.
            gfx::DrawRectOutline(*mPainter, gfx::FRect(GetViewport()), gfx::Color::Green, 1.0f);
        }

        if (mShowMouseCursor)
        {
            const auto& offset = -mCursorHotspot*mCursorSize;
            gfx::FRect rect;
            rect.Resize(mCursorSize.x, mCursorSize.y);
            rect.Move(mCursorPos.x, mCursorPos.y);
            rect.Translate(offset.x, offset.y);
            gfx::FillShape(*mPainter, rect, *mMouseDrawable, *mMouseMaterial);
        }

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

    virtual void Update(double dt) override
    {
        if (mDebug.debug_pause)
            dt = 0.0;

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
            mUIPainter.Update(mGameTimeTotal, dt);
        }

        mActionDelay = math::clamp(0.0f, mActionDelay, mActionDelay - (float)dt);
    }
    virtual void EndMainLoop() override
    {
        if (mScene)
        {
            mScripting->EndLoop();
            mScene->EndLoop();
        }

        if (mActionDelay > 0.0f)
            return;

        engine::Action action;
        while (mGame->GetNextAction(&action) || mScripting->GetNextAction(&action))
        {
            if (auto* ptr = std::get_if<engine::ShowMouseAction>(&action))
                ShowMouseCursor(ptr->show);
            else if (auto* ptr = std::get_if<engine::BlockMouseAction>(&action))
                BlockKeyboard(ptr->block);
            else if (auto* ptr = std::get_if<engine::BlockMouseAction>(&action))
                BlockMouse(ptr->block);
            else if (auto* ptr = std::get_if<engine::GrabMouseAction>(&action))
                GrabMouse(ptr->grab);
            else if (auto* ptr = std::get_if<engine::OpenUIAction>(&action))
                OpenUI(ptr->ui);
            else if (auto* ptr = std::get_if<engine::CloseUIAction>(&action))
                CloseUI(ptr->result);
            else if (auto* ptr = std::get_if<engine::PlayAction>(&action))
                PlayGame(ptr->klass);
            else if (auto* ptr = std::get_if<engine::SuspendAction>(&action))
                SuspendGame();
            else if (auto* ptr = std::get_if<engine::ResumeAction>(&action))
                ResumeGame();
            else if (auto* ptr = std::get_if<engine::QuitAction>(&action))
                QuitGame(ptr->exit_code);
            else if (auto* ptr = std::get_if<engine::StopAction>(&action))
                StopGame();
            else if (auto* ptr = std::get_if<engine::RequestFullScreenAction>(&action))
                RequestFullScreen(ptr->full_screen);
            else if (auto* ptr = std::get_if<engine::DebugClearAction>(&action))
                DebugClear();
            else if (auto* ptr = std::get_if<engine::DebugPrintAction>(&action))  {
                DebugPrintString(ptr->message, ptr->clear);
            } else if (auto* ptr = std::get_if<engine::DelayAction>(&action)) {
                DelayGame(ptr->seconds);
                break;
            }
        }
    }

    virtual void Shutdown() override
    {
        DEBUG("Engine shutdown");
        mAudio.reset();

        gfx::SetResourceLoader(nullptr);
        mDevice.reset();
    }
    virtual bool IsRunning() const override
    { return mRunning; }
    virtual wdk::WindowListener* GetWindowListener() override
    { return this; }

    virtual void SetHostStats(const HostStats& stats) override
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
    virtual bool GetStats(Stats* stats) const override
    {
        stats->total_game_time = mGameTimeTotal;
        return true;
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
        if (mBlockKeyboard)
            return;
        mGame->OnKeyDown(key);
        if (mScene)
        {
            mScripting->OnKeyDown(key);
        }
    }
    virtual void OnKeyup(const wdk::WindowEventKeyup& key) override
    {       
        if (mBlockKeyboard)
            return;
        mGame->OnKeyUp(key);
        if (mScene)
        {
            mScripting->OnKeyUp(key);
        }
    }
    virtual void OnChar(const wdk::WindowEventChar& text) override
    {
        if (mBlockKeyboard)
            return;
        mGame->OnChar(text);
    }
    virtual void OnMouseMove(const wdk::WindowEventMouseMove& mouse) override
    {
        if (mBlockMouse)
            return;
        mCursorPos.x = mouse.window_x;
        mCursorPos.y = mouse.window_y;

        if (HaveOpenUI())
            SendUIMouseEvent(MapUIMouseEvent(mouse), &uik::Window::MouseMove);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::Game::OnMouseMove);
        SendEntityScriptMouseEvent(mickey, &engine::ScriptEngine::OnMouseMove);
    }
    virtual void OnMousePress(const wdk::WindowEventMousePress& mouse) override
    {
        if (mBlockMouse)
            return;

        if (HaveOpenUI())
            SendUIMouseEvent(MapUIMouseEvent(mouse), &uik::Window::MousePress);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::Game::OnMousePress);
        SendEntityScriptMouseEvent(mickey, &engine::ScriptEngine::OnMousePress);
    }
    virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse) override
    {
        if (mBlockMouse)
            return;

        if (HaveOpenUI())
            SendUIMouseEvent(MapUIMouseEvent(mouse), &uik::Window::MouseRelease);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::Game::OnMouseRelease);
        SendEntityScriptMouseEvent(mickey, &engine::ScriptEngine::OnMouseRelease);
    }
private:
    engine::IRect GetViewport() const
    {
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
        return engine::IRect(device_viewport_x, device_viewport_y,
                             device_viewport_width, device_viewport_height);
    }

    using GameMouseFunc = void (engine::Game::*)(const engine::MouseEvent&);
    void SendGameMouseEvent(const engine::MouseEvent& mickey, GameMouseFunc which)
    {
        auto* game = mGame.get();
        (game->*which)(mickey);
    }
    using EntityScriptMouseFunc = void (engine::ScriptEngine::*)(const engine::MouseEvent&);
    void SendEntityScriptMouseEvent(const engine::MouseEvent& mickey, EntityScriptMouseFunc which)
    {
        if (!mScene)
            return;
        auto* scripting = mScripting.get();
        (scripting->*which)(mickey);
    }

    template<typename WdkMouseEvent>
    engine::MouseEvent MapGameMouseEvent(const WdkMouseEvent& mickey) const
    {
        const auto& viewport = GetViewport();
        const float width  = viewport.GetWidth();
        const float height = viewport.GetHeight();
        const auto& point  = viewport.MapToLocal(mickey.window_x, mickey.window_y);
        const auto point_norm_x  =  (point.GetX() / width) * 2.0 - 1.0f;
        const auto point_norm_y  =  (point.GetY() / height) * -2.0 + 1.0f;
        const auto& projection   = gfx::Painter::MakeOrthographicProjection(mGame->GetViewport());
        const auto& point_scene  = glm::inverse(projection) * glm::vec4(point_norm_x,
                                                                        point_norm_y, 1.0f, 1.0f);
        engine::MouseEvent event;
        event.window_coord = glm::vec2(mickey.window_x, mickey.window_y);
        event.scene_coord  = glm::vec2(point_scene.x, point_scene.y);
        event.over_scene   = viewport.TestPoint(mickey.window_x, mickey.window_y);
        event.btn  = mickey.btn;
        event.mods = mickey.modifiers;
        return event;
    }

    using UIMouseFunc = uik::Window::WidgetAction (uik::Window::*)(const uik::Window::MouseEvent&, uik::State&);
    void SendUIMouseEvent(const uik::Window::MouseEvent& mickey, UIMouseFunc which)
    {
        auto* ui = GetUI();

        auto action = (ui->*which)(mickey, mUIState);
        if (action.type == uik::WidgetActionType::None)
            return;
        mGame->OnUIAction(action);
        //DEBUG("Widget action: '%1'", action.type);
    }
    template<typename WdkMouseEvent>
    uik::Window::MouseEvent MapUIMouseEvent(const WdkMouseEvent& mickey) const
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
        event.time   = base::GetTime();
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


    void OpenUI(engine::ClassHandle<uik::Window> window)
    {
        // todo: if the style loading somehow fails, then what?
        mUIStyle.ClearProperties();
        mUIStyle.ClearMaterials();
        mUIState.Clear();

        auto style_data = mGameDataLoader->LoadGameData(window->GetStyleName());
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
            auto style_data = mGameDataLoader->LoadGameData(ui->GetStyleName());
            if (!style_data || !mUIStyle.LoadStyle(*style_data))
            {
                ERROR("The UI style ('%1') could not be loaded.", ui->GetStyleName());
                return;
            }
            ui->Style(mUIPainter);
        }
    }
    void PlayGame(engine::ClassHandle<game::SceneClass> klass)
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
    void QuitGame(int exit_code)
    {
        // todo: cleanup?

        mRequests.Quit(exit_code);
    }
    void UpdateGame(double game_time,  double dt)
    {
        if (mScene)
        {
            mScene->Update(dt);
            if (mPhysics.HaveWorld())
            {
                std::vector<engine::ContactEvent> contacts;
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

        std::vector<engine::AudioEvent> audio_events;
        mAudio->Tick(&audio_events);
        for (const auto& event : audio_events)
        {
            mGame->OnAudioEvent(event);
        }

        mGame->Update(game_time, dt);

        mMouseMaterial->Update(dt);
        mMouseDrawable->Update(dt);
    }

    void TickGame(double game_time, double dt)
    {
        mGame->Tick(game_time, dt);
        if (mScene)
        {
            mScripting->Tick(game_time, dt);
        }
    }
    void DebugClear()
    { mDebugPrints.clear(); }
    void DebugPrintString(std::string msg, bool clear)
    {
        if (clear)
            mDebugPrints.clear();
        DebugPrint print;
        print.message = std::move(msg);
        mDebugPrints.push_back(std::move(print));
    }

    void DelayGame(float seconds)
    {
        mActionDelay = math::clamp(0.0f, seconds, seconds);
        DEBUG("Action delay: %1 s", mActionDelay);
    }
    void ShowMouseCursor(bool show)
    {
        mShowMouseCursor = show;
        DEBUG("Mouse cursor is %1", show ? "ON" : "OFF");
    }
    void BlockKeyboard(bool block)
    {
        mBlockKeyboard = block;
        DEBUG("Keyboard block is %1", block ? "ON" : "OFF");
    }
    void BlockMouse(bool block)
    {
        mBlockMouse = block;
        DEBUG("Mouse block is %1", block ? "ON" : "OFF");
    }
    void GrabMouse(bool grab)
    {
        mRequests.GrabMouse(grab);
        DEBUG("Requesting to %1 mouse grabbing.", grab ? "enable" : "disable");
    }
    void RequestFullScreen(bool full_screen)
    {
        mRequests.SetFullScreen(full_screen);
        DEBUG("Requesting %1 mode", full_screen ? "FullScreen" : "Window");
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
    // current mouse cursor details
    glm::vec2 mCursorPos;
    glm::vec2 mCursorHotspot;
    glm::vec2 mCursorSize;
    std::unique_ptr<gfx::Material> mMouseMaterial;
    std::unique_ptr<gfx::Drawable> mMouseDrawable;
    gfx::Color4f mClearColor = {0.2f, 0.3f, 0.4f, 1.0f};
    // game dir where the executable is.
    std::string mDirectory;
    // home directory for the game generated data.
    std::string mGameHome;
    // queue of outgoing requests regarding the environment
    // such as the window size/position etc that the game host
    // may/may not support.
    engine::AppRequestQueue mRequests;
    // Interface for accessing the game classes and resources
    // such as animations, materials etc.
    engine::ClassLibrary* mClasslib = nullptr;
    // Game data/content loader.
    engine::Loader* mGameDataLoader = nullptr;
    // audio stream loader
    audio::Loader* mAudioLoader = nullptr;
    // The graphics painter device.
    std::unique_ptr<gfx::Painter> mPainter;
    // The graphics device.
    std::shared_ptr<gfx::Device> mDevice;
    // The rendering subsystem.
    engine::Renderer mRenderer;
    // The physics subsystem.
    engine::PhysicsEngine mPhysics;
    // The UI painter for painting UIs
    engine::UIPainter mUIPainter;
    engine::UIStyle mUIStyle;
    // The audio engine.
    std::unique_ptr<engine::AudioEngine> mAudio;
    // The scripting subsystem.
    std::unique_ptr<engine::ScriptEngine> mScripting;
    // Current game scene or nullptr if no scene.
    std::unique_ptr<game::Scene> mScene;
    // Game logic implementation.
    std::unique_ptr<engine::Game> mGame;
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
    engine::Engine::DebugOptions mDebug;
    // last statistics about the rendering rate etc.
    engine::Engine::HostStats mLastStats;
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
    float mActionDelay = 0.0;
    // when true will discard keyboard events. note that the game
    // might still be polling the keyboard through a direct call
    // to read the keyboard state. this applies only to the kv events.
    bool mBlockKeyboard = false;
    // block mouse event processing.
    bool mBlockMouse = false;
    // whether to show or not mouse cursor
    bool mShowMouseCursor = true;
    // flag to control the debug string printing.
    bool mShowDebugs = true;
};

} //namespace

extern "C" {
GAMESTUDIO_EXPORT engine::Engine* Gamestudio_CreateEngine()
{
#if defined(NDEBUG)
    DEBUG("GameStudioEngine in release build.");
#else
    DEBUG("GameStudioEngine in DEBUG build.");
#endif
    return new GameStudioEngine;
}
} // extern C
