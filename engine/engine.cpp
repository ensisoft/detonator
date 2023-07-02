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

#include "base/bitflag.h"
#include "base/logging.h"
#include "base/trace.h"
#include "game/entity.h"
#include "game/treeop.h"
#include "game/tilemap.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/resource.h"
#include "graphics/material.h"
#include "graphics/utility.h"
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
#include "engine/state.h"
#include "uikit/window.h"
#include "uikit/painter.h"
#include "uikit/state.h"

namespace
{

// Default game engine implementation. Implements the main App interface
// which is the interface that enables the game host to communicate
// with the application/game implementation in order to update/tick/etc.
// the game and also to handle input from keyboard and mouse.
class GameStudioEngine final : public engine::Engine,
                               public wdk::WindowListener
{
public:
    GameStudioEngine() : mDebugPrints(10)
    {}

    virtual bool GetNextRequest(Request* out) override
    { return mRequests.GetNext(out); }

    virtual void Init(const InitParams& init) override
    {
        DEBUG("Engine initializing. [surface=%1x%2]", init.surface_width, init.surface_height);
        mAudio = std::make_unique<engine::AudioEngine>(init.application_name);
        mAudio->SetClassLibrary(mClasslib);
        mAudio->SetLoader(mAudioLoader);
        mAudio->Start();
        mDevice  = dev::CreateDevice(init.context)->GetSharedGraphicsDevice();
        mPainter = gfx::Painter::Create(mDevice);
        mPainter->SetSurfaceSize(init.surface_width, init.surface_height);
        mPainter->SetEditingMode(init.editing_mode);
        mSurfaceWidth  = init.surface_width;
        mSurfaceHeight = init.surface_height;
        mRuntime = std::make_unique<engine::LuaRuntime>("lua", init.game_script, mGameHome, init.application_name);
        mRuntime->SetClassLibrary(mClasslib);
        mRuntime->SetPhysicsEngine(&mPhysics);
        mRuntime->SetStateStore(&mStateStore);
        mRuntime->SetAudioEngine(mAudio.get());
        mRuntime->SetDataLoader(mEngineDataLoader);
        mRuntime->SetEditingMode(init.editing_mode);
        mRuntime->SetPreviewMode(init.preview_mode);
        mRuntime->Init();
        mRuntime->SetSurfaceSize(init.surface_width, init.surface_height);
        mUIStyle.SetClassLibrary(mClasslib);
        mUIStyle.SetDataLoader(mEngineDataLoader);
        mUIPainter.SetPainter(mPainter.get());
        mUIPainter.SetStyle(&mUIStyle);
        mRenderer.SetClassLibrary(mClasslib);
        mRenderer.SetEditingMode(init.editing_mode);
        mRenderer.SetName("Engine");
        mRenderer.EnableEffect(engine::Renderer::Effects::Bloom, true);
        mPhysics.SetClassLibrary(mClasslib);
        mFlags.set(Flags::EditingMode,     init.editing_mode);
        mFlags.set(Flags::Running,         true);
        mFlags.set(Flags::Fullscreen,      false);
        mFlags.set(Flags::BlockKeyboard,   false);
        mFlags.set(Flags::BlockMouse,      false);
        mFlags.set(Flags::ShowMouseCursor, true);
        mFlags.set(Flags::ShowDebugs,      true);
        mFlags.set(Flags::EnableBloom,     true);
        mFlags.set(Flags::EnablePhysics,   true);
    }
    virtual bool Load() override
    {
        DEBUG("Loading game state.");
        mRuntime->LoadGame();
        return true;
    }

    virtual void Start() override
    {
        DEBUG("Starting game play.");
        mRuntime->StartGame();
    }
    virtual void SetDebugOptions(const DebugOptions& debug) override
    {
        mDebug = debug;

        if (mAudio)
            mAudio->SetDebugPause(debug.debug_pause);
    }
    virtual void DebugPrintString(const std::string& message) override
    {
        DebugPrint print;
        print.message = message;
        mDebugPrints.push_back(std::move(print));
    }
    virtual void SetTracer(base::Trace* tracer) override
    { base::SetThreadTrace(tracer); }

    virtual void SetEnvironment(const Environment& env) override
    {
        mClasslib         = env.classlib;
        mEngineDataLoader = env.engine_loader;
        mAudioLoader      = env.audio_loader;
        mGameLoader       = env.game_loader;
        mDirectory        = env.directory;
        mGameHome         = env.game_home;

        // set the unfortunate global gfx loader
        gfx::SetResourceLoader(env.graphics_loader);
        DEBUG("Game install directory: '%1'.", env.directory);
        DEBUG("Game home: '%1'.", env.game_home);
        DEBUG("User home: '%1'.", env.user_home);
    }
    virtual void SetEngineConfig(const EngineConfig& conf) override
    {
        audio::Format audio_format;
        audio_format.channel_count = static_cast<unsigned>(conf.audio.channels); // todo: use enum
        audio_format.sample_rate   = conf.audio.sample_rate;
        audio_format.sample_type   = conf.audio.sample_type;
        mAudio->SetFormat(audio_format);
        mAudio->SetBufferSize(conf.audio.buffer_size);
        mAudio->EnableCaching(conf.audio.enable_pcm_caching);
        DEBUG("Configure audio engine. [format=%1 buff_size=%2ms]", audio_format, conf.audio.buffer_size);

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

        auto mouse_drawable = mClasslib->FindDrawableClassById(conf.mouse_cursor.drawable);
        DEBUG("Mouse material='%1',drawable='%2'", conf.mouse_cursor.material, conf.mouse_cursor.drawable);
        if (!mouse_drawable)
        {
            WARN("No such mouse cursor drawable found. [drawable='%1']", conf.mouse_cursor.drawable);
            mouse_drawable = std::make_shared<gfx::CursorClass>(gfx::CursorClass::Shape::Arrow);
            mCursorSize    = glm::vec2(20.0f, 20.0f);
            mCursorHotspot = glm::vec2(0.0f, 0.0f);
        }
        else
        {
            mCursorSize    = conf.mouse_cursor.size;
            mCursorHotspot = conf.mouse_cursor.hotspot;
        }
        auto mouse_material = mClasslib->FindMaterialClassById(conf.mouse_cursor.material);
        if (!mouse_material)
        {
            WARN("No such mouse cursor material found. [material='%1']", conf.mouse_cursor.material);
            auto material = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color);
            material->SetBaseColor(gfx::Color::HotPink);
        }
        mMouseDrawable = gfx::CreateDrawableInstance(mouse_drawable);
        mMouseMaterial = gfx::CreateMaterialInstance(mouse_material);
        mCursorUnits   = conf.mouse_cursor.units;

        mFlags.set(Flags::ShowMouseCursor, conf.mouse_cursor.show);
        mFlags.set(Flags::EnablePhysics, conf.physics.enabled);
    }

    virtual void Draw() override
    {
        mDevice->BeginFrame();
        mDevice->ClearColor(mClearColor);
        // rendering surface dimensions.
        const float surf_width  = (float)mSurfaceWidth;
        const float surf_height = (float)mSurfaceHeight;
        // get the game's logical viewport into the game world.
        const auto& game_view = mRuntime->GetViewport();
        // map the logical viewport to some area in the rendering surface
        // so that the rendering area (the device viewport) has the same
        // aspect ratio as the logical viewport.
        const float game_view_width  = game_view.GetWidth();
        const float game_view_height = game_view.GetHeight();
        // the scaling factor for mapping game units to rendering surface (pixel) units.
        const float game_scale  = std::min(surf_width / game_view_width, surf_height / game_view_height);

        if (mScene)
        {
            TRACE_SCOPE("DrawScene");

            gfx::Transform view;
            // set the game view transform. moving the viewport is the same as
            // moving the objects in the opposite direction relative to the viewport.
            view.Translate(-game_view.GetX(), -game_view.GetY());

            // low level draw packet filter for culling draw packets
            // that fall outside the current viewport.
            class Culler : public engine::EntityInstanceDrawHook {
            public:
                Culler(const engine::FRect& view_rect,
                       const glm::mat4& view_matrix)
                  : mViewRect(view_rect)
                  , mViewMatrix(view_matrix)
                {}
                virtual bool InspectPacket(const game::EntityNode* node, engine::DrawPacket& packet) override
                {
                    const auto& rect = game::ComputeBoundingRect(mViewMatrix * packet.transform);
                    if (!DoesIntersect(rect, mViewRect))
                        return false;
                    return true;
                }
            private:
                const engine::FRect mViewRect;
                const glm::mat4 mViewMatrix;
            };
            const gfx::FRect viewport(0.0f, 0.0f, game_view_width, game_view_height);
            Culler cull(viewport, view.GetAsMatrix());

            engine::Renderer::Surface surface;
            surface.viewport = GetViewport();
            surface.size     = base::USize(mSurfaceWidth, mSurfaceHeight);
            mRenderer.SetSurface(surface);

            engine::Renderer::Camera camera;
            camera.viewport = viewport;
            camera.rotation = 0.0f;
            camera.scale    = glm::vec2{1.0f, 1.0f};
            camera.position = glm::vec2{game_view.GetX(), game_view.GetY()};
            mRenderer.SetCamera(camera);


            if (mFlags.test(Flags::EditingMode))
            {
                ConfigureRendererForScene();
            }
            TRACE_CALL("Renderer::BeginFrame", mRenderer.BeginFrame());
            TRACE_CALL("Renderer::DrawScene", mRenderer.Draw(*mPainter, &cull));
            TRACE_CALL("Renderer::EndFrame", mRenderer.EndFrame());
            TRACE_CALL("DebugDraw", DrawDebugObjects());
        }

        if (auto* ui = GetUI())
        {
            TRACE_SCOPE("DrawUI");

            const auto& rect   = ui->GetBoundingRect();
            const float width  = rect.GetWidth();
            const float height = rect.GetHeight();
            const float scale  = std::min(surf_width / width, surf_height / height);
            const float device_viewport_width = width * scale;
            const float device_viewport_height = height * scale;

            mPainter->SetPixelRatio(glm::vec2(1.0f, 1.0f));
            mPainter->SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, width, height));
            // Set the actual device viewport for rendering into the window surface.
            // the viewport retains the UI's aspect ratio and is centered in the middle
            // of the rendering surface.
            mPainter->SetViewport((surf_width - device_viewport_width)*0.5,
                                  (surf_height - device_viewport_height)*0.5,
                                  device_viewport_width, device_viewport_height);
            mPainter->ResetViewMatrix();
            TRACE_CALL("UI::Paint", ui->Paint(mUIState, mUIPainter, base::GetTime(), nullptr));
        }

        TRACE_ENTER(DebugDrawing);
        if (mDebug.debug_show_fps || mDebug.debug_show_msg || mDebug.debug_draw || mFlags.test(Flags::ShowMouseCursor))
        {
            mPainter->SetPixelRatio(glm::vec2(1.0f, 1.0f));
            mPainter->SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, surf_width, surf_height));
            mPainter->SetViewport(0, 0, surf_width, surf_height);
            mPainter->ResetViewMatrix();
        }
        if (mDebug.debug_show_fps)
        {
            char hallelujah[512] = {0};
            std::snprintf(hallelujah, sizeof(hallelujah) - 1,
            "FPS: %.2f wall time: %.2f frames: %u",
                mLastStats.current_fps, mLastStats.total_wall_time, mLastStats.num_frames_rendered);

            const gfx::FRect rect(10, 10, 500, 20);
            gfx::FillRect(*mPainter, rect, gfx::Color4f(gfx::Color::Black, 0.6f));
            gfx::DrawTextRect(*mPainter, hallelujah,
                mDebug.debug_font, 14, rect, gfx::Color::HotPink,
                gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
        }
        if (mDebug.debug_show_msg && mFlags.test(Flags::ShowDebugs))
        {
            gfx::FRect rect(10, 30, 500, 20);
            for (const auto& print : mDebugPrints)
            {
                gfx::FillRect(*mPainter, rect, gfx::Color4f(gfx::Color::Black, 0.6f));
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
        TRACE_LEAVE(DebugDrawing);

        if (mFlags.test(Flags::ShowMouseCursor))
        {
            // scale the cursor size based on the requested units of the cursor size.
            const auto& size   = mCursorUnits == EngineConfig::MouseCursorUnits::Units
                                 ? mCursorSize * game_scale
                                 : mCursorSize;
            const auto& offset = -mCursorHotspot*size;
            gfx::FRect rect;
            rect.Resize(size.x, size.y);
            rect.Move(mCursorPos.x, mCursorPos.y);
            rect.Translate(offset.x, offset.y);
            gfx::FillShape(*mPainter, rect, *mMouseDrawable, *mMouseMaterial);
        }

        TRACE_CALL("Device::Swap", mDevice->EndFrame(true));
        // Note that we *don't* call CleanGarbage here since currently there should
        // be nothing that is creating needless GPU resources.
    }

    virtual void BeginMainLoop() override
    {
        mRuntime->SetFrameNumber(mFrameCounter);

        // service the audio system once.
        std::vector<engine::AudioEvent> audio_events;
        TRACE_CALL("Audio::Update", mAudio->Update(&audio_events));
        for (const auto& event : audio_events)
        {
            mRuntime->OnAudioEvent(event);
        }
    }

    virtual void Step() override
    {
        mStepForward = true;
    }

    virtual void Update(float dt) override
    {
        // Game play update. NOT the place for any kind of
        // real time/wall time subsystem (such as audio) service
        if (mDebug.debug_pause && !mStepForward)
            return;

        // there's plenty of information about different ways to write a basic
        // game rendering loop. here are some suggested references:
        // https://gafferongames.com/post/fix_your_timestep/
        // Game Engine Architecture by Jason Gregory
        mTimeAccum += dt;

        // do simulation/animation update steps.
        while (mTimeAccum >= mGameTimeStep)
        {
            // current game time from which we step forward
            // in ticks later on.
            auto tick_time = mGameTimeTotal;

            if (mScene)
            {
                TRACE_CALL("Scene::BeginLoop", mScene->BeginLoop());
                TRACE_CALL("Runtime:BeginLoop", mRuntime->BeginLoop());
            }

            // Call UpdateGame with the *current* time. I.e. the game
            // is advancing one time step from current mGameTimeTotal.
            // this is consistent with the tick time accumulation below.
            TRACE_CALL("UpdateGame", UpdateGame(mGameTimeTotal, mGameTimeStep));
            mGameTimeTotal += mGameTimeStep;
            mTimeAccum -= mGameTimeStep;
            mTickAccum += mGameTimeStep;

            // PostUpdate allows the game to perform activities with consistent
            // world state after everything has settled down. It might be tempting
            // to bake the functionality of "Rebuild" in the scene in the loop
            // end functionality and let the game perform the "PostUpdate" actions in
            // the Update function. But this has the problem that during the call
            // to Update (on each entity instance) the world doesn't yet have
            // consistent state because not every object that needs to move has
            // moved. This might lead to incorrect conclusions when for exampling
            // trying to detect whether things are colling/overlapping. For example
            // if entity A's Update function updates A's position and checks whether
            // A is hitting some other object those other objects may or may not have
            // been moved already. To resolve this issue the game should move entity A
            // in the Update function and then check for the collisions/overlap/whatever
            // in the PostUpdate with consistent world state.
            if (mScene)
            {
                // make sure to do this first in order to allow the scene to rebuild
                // the spatial indices etc. before the game's PostUpdate runs.
                TRACE_CALL("Scene::Rebuild", mScene->Rebuild());
                // using the time we've arrived to now after having taken the previous
                // delta step forward in game time.
                TRACE_CALL("Runtime::PostUpdate", mRuntime->PostUpdate(mGameTimeTotal));
            }

            // todo: add mGame->PostUpdate here if needed
            // TRACE_CALL("Game::PostUpdate", mGame->PostUpdate(mGameTimeTotal))

            // It might be tempting to use the tick functionality to perform
            // some type of movement in some types of games. For example
            // a tetris-clone could move the pieces on tick steps instead of
            // continuous updates. But to keep things simple the engine is only
            // promising consistent world state to exist during the call to
            // PostUpdate. In order to support a simple use case such as
            // "move on tick" the game should set a flag on the Tick, perform
            // move on update when the flag is set and then clear the flag.
            while (mTickAccum >= mGameTickStep)
            {
                TRACE_CALL("Runtime::Tick", mRuntime->Tick(tick_time, mGameTickStep));
                mTickAccum -= mGameTickStep;
                tick_time += mGameTickStep;
            }

            if (mScene)
            {
                TRACE_CALL("Runtime::EndLoop", mRuntime->EndLoop());
                TRACE_CALL("Scene::EndLoop", mScene->EndLoop());
            }

            mStepForward = false;
        }
        TRACE_CALL("GameActions", PerformGameActions(dt));
    }
    virtual void EndMainLoop() override
    {
        mFrameCounter++;
    }

    virtual void Stop() override
    {
        mRuntime->StopGame();
    }

    virtual void Save() override
    {
        mRuntime->SaveGame();
    }

    virtual void Shutdown() override
    {
        DEBUG("Engine shutdown");
        mAudio.reset();

        gfx::SetResourceLoader(nullptr);
        mDevice.reset();

        audio::ClearCaches();
    }
    virtual bool IsRunning() const override
    { return mFlags.test(Flags::Running); }
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

        if (!mDebug.debug_pause)
        {
            for (auto it = mDebugPrints.begin(); it != mDebugPrints.end();)
            {
                auto& print = *it;
                if (--print.lifetime < 0)
                {
                    it = mDebugPrints.erase(it);
                } else ++it;
            }
        }
    }
    virtual bool GetStats(Stats* stats) const override
    {
        gfx::Device::ResourceStats rs;
        mDevice->GetResourceStats(&rs);

        stats->total_game_time         = mGameTimeTotal;
        stats->static_vbo_mem_use      = rs.static_vbo_mem_use;
        stats->static_vbo_mem_alloc    = rs.static_vbo_mem_alloc;
        stats->dynamic_vbo_mem_alloc   = rs.dynamic_vbo_mem_alloc;
        stats->dynamic_vbo_mem_use     = rs.dynamic_vbo_mem_use;
        stats->streaming_vbo_mem_alloc = rs.streaming_vbo_mem_alloc;
        stats->streaming_vbo_mem_use   = rs.streaming_vbo_mem_use;
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
    virtual void ReloadResources(unsigned bits) override
    {
        // okay a bit weird this function is about "reload"
        // but we're deleting here.. so for now we just delete
        // stuff and that will cause reload when stuff is needed
        // to draw again. this must be done this way since the
        // device objects (such as textures) don't know where the
        // data has come from. alternative would be to go over the
        // materials and their textures/programs etc but that's more work

        if (bits & (unsigned)Engine::ResourceType::Textures)
            mDevice->DeleteTextures();
        if (bits & (unsigned)Engine::ResourceType::Shaders)
        {
            mDevice->DeleteShaders();
            mDevice->DeletePrograms();
        }
    }

    virtual void OnRenderingSurfaceResized(unsigned width, unsigned height) override
    {
        // ignore accidental superfluous notifications.
        if (width == mSurfaceWidth && height == mSurfaceHeight)
            return;

        DEBUG("Rendering surface resized. [width=%1, height=%2]", width, height);
        mSurfaceWidth = width;
        mSurfaceHeight = height;
        mPainter->SetSurfaceSize(width, height);
        mRuntime->SetSurfaceSize(width, height);
    }
    virtual void OnEnterFullScreen() override
    {
        DEBUG("Enter full screen mode.");
        mFlags.set(Flags::Fullscreen, true);
    }
    virtual void OnLeaveFullScreen() override
    {
        DEBUG("Leave full screen mode.");
        mFlags.set(Flags::Fullscreen, false);
    }

    // WindowListener
    virtual void OnWantClose(const wdk::WindowEventWantClose&) override
    {
        // todo: handle ending play, saving game etc.
        mFlags.set(Flags::Running, false);
    }
    virtual void OnKeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (mFlags.test(Flags::BlockKeyboard))
            return;

        SendUIKeyEvent(key, &uik::Window::KeyDown);

        mRuntime->OnKeyDown(key);
    }
    virtual void OnKeyUp(const wdk::WindowEventKeyUp& key) override
    {       
        if (mFlags.test(Flags::BlockKeyboard))
            return;

        SendUIKeyEvent(key, &uik::Window::KeyUp);

        mRuntime->OnKeyUp(key);
    }
    virtual void OnChar(const wdk::WindowEventChar& text) override
    {
        if (mFlags.test(Flags::BlockKeyboard))
            return;

        mRuntime->OnChar(text);
    }
    virtual void OnMouseMove(const wdk::WindowEventMouseMove& mouse) override
    {
        if (mFlags.test(Flags::BlockMouse))
            return;

        mCursorPos.x = mouse.window_x;
        mCursorPos.y = mouse.window_y;

        if (HaveOpenUI())
            SendUIMouseEvent(MapUIMouseEvent(mouse), &uik::Window::MouseMove);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMouseMove);
    }
    virtual void OnMousePress(const wdk::WindowEventMousePress& mouse) override
    {
        if (mFlags.test(Flags::BlockMouse))
            return;

        if (HaveOpenUI())
            SendUIMouseEvent(MapUIMouseEvent(mouse), &uik::Window::MousePress);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMousePress);
    }
    virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse) override
    {
        if (mFlags.test(Flags::BlockMouse))
            return;

        if (HaveOpenUI())
            SendUIMouseEvent(MapUIMouseEvent(mouse), &uik::Window::MouseRelease);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMouseRelease);
    }
private:
    engine::IRect GetViewport() const
    {
        // get the game's logical viewport into the game world.
        const auto& view = mRuntime->GetViewport();
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

    using GameMouseFunc = void (engine::GameRuntime::*)(const engine::MouseEvent&);
    void SendGameMouseEvent(const engine::MouseEvent& mickey, GameMouseFunc which)
    {
        auto* game = mRuntime.get();
        (game->*which)(mickey);
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
        const auto& projection   = gfx::MakeOrthographicProjection(mRuntime->GetViewport());
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

    using UIKeyFunc = std::vector<uik::Window::WidgetAction> (uik::Window::*)(const uik::Window::KeyEvent&, uik::State&);
    template<typename WdkEvent>
    void SendUIKeyEvent(const WdkEvent& key, UIKeyFunc which)
    {
        auto* ui = GetUI();
        if (ui == nullptr)
            return;
        if (!ui->TestFlag(uik::Window::Flags::EnableVirtualKeys))
            return;

        const auto vk = mUIKeyMap.MapKey(key.symbol, key.modifiers);
        if (vk == uik::VirtualKey::None)
            return;

        uik::Window::KeyEvent event;
        event.key  = vk;
        event.time = base::GetTime();
        const auto& actions = (ui->*which)(event, mUIState);
        for (const auto& action : actions)
        {
            mRuntime->OnUIAction(ui, action);
            //DEBUG("Widget action: '%1'", action.type);
        }
    }

    using UIMouseFunc = std::vector<uik::Window::WidgetAction> (uik::Window::*)(const uik::Window::MouseEvent&, uik::State&);
    void SendUIMouseEvent(const uik::Window::MouseEvent& mickey, UIMouseFunc which)
    {
        auto* ui = GetUI();

        const auto& actions = (ui->*which)(mickey, mUIState);
        for (const auto& action : actions)
        {
            mRuntime->OnUIAction(ui, action);
            //DEBUG("Widget action: '%1'", action.type);
        }
    }
    template<typename WdkMouseEvent>
    uik::Window::MouseEvent MapUIMouseEvent(const WdkMouseEvent& mickey) const
    {
        const auto* ui     = GetUI();
        const auto& rect   = ui->GetBoundingRect();
        const float width  = rect.GetWidth();
        const float height = rect.GetHeight();
        const float scale  = std::min((float)mSurfaceWidth/width, (float)mSurfaceHeight/height);
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
        if (mUIStack.empty())
            return nullptr;
        return mUIStack.top().get();
    }
    inline const uik::Window* GetUI() const
    {
        if (mUIStack.empty())
            return nullptr;
        return mUIStack.top().get();
    }
    bool HaveOpenUI() const
    { return !mUIStack.empty(); }

    void LoadStyle(const std::string& uri)
    {
        // todo: if the style loading somehow fails, then what?
        mUIStyle.ClearProperties();
        mUIStyle.ClearMaterials();

        auto data = mEngineDataLoader->LoadEngineDataUri(uri);
        if (!data)
        {
            ERROR("Failed to load UI style. [uri='%1']", uri);
            return;
        }

        if (!mUIStyle.LoadStyle(*data))
        {
            ERROR("Failed to parse UI style. [uri='%1']", uri);
            return;
        }
        DEBUG("Loaded UI style file successfully. [uri='%1']", uri);
    }

    void LoadKeyMap(const std::string& uri)
    {
        mUIKeyMap.Clear();

        auto data = mEngineDataLoader->LoadEngineDataUri(uri);
        if (!data)
        {
            ERROR("Failed to load UI keymap data. [uri='%1']", uri);
            return;
        }
        if (!mUIKeyMap.LoadKeys(*data))
        {
            ERROR("Failed to parse UI keymap. [uri='%1']", uri);
            return;
        }
        DEBUG("Loaded UI keymap successfully. [uri='%1']", uri);
    }

    void OnAction(const engine::OpenUIAction& action)
    {
        auto window = action.ui;

        LoadStyle(window->GetStyleName());
        LoadKeyMap(window->GetKeyMapFile());

        mUIState.Clear();
        window->Style(mUIPainter);
        window->Show(mUIState);
        // push the window to the top of the UI stack. this is the new
        // currently active UI
        mUIStack.push(window);

        mUIPainter.DeleteMaterialInstances();

        auto* ui = mUIStack.top().get();
        mRuntime->OnUIOpen(ui);
        mRuntime->SetCurrentUI(ui);
    }
    void OnAction(const engine::CloseUIAction& action)
    {
        // get the current top UI (if any) and close it and
        // pop it off the UI stack.
        if (auto* ui = GetUI())
        {
            mRuntime->OnUIClose(ui, action.result);
            mUIStack.pop();
        }
        mRuntime->SetCurrentUI(GetUI());

        // If there's another UI in the UI stack then reapply
        // styling information.
        if (auto* ui = GetUI())
        {
            LoadStyle(ui->GetStyleName());
            LoadKeyMap(ui->GetKeyMapFile());

            mUIPainter.DeleteMaterialInstances();
            mUIState.Clear();

            ui->Style(mUIPainter);
            ui->Show(mUIState);
        }
    }
    void OnAction(engine::PlayAction& action)
    {
        mScene = std::move(action.scene);
        if (mFlags.test(Flags::EnablePhysics))
        {
            mPhysics.DeleteAll();
            mPhysics.CreateWorld(*mScene);
        }
        mRenderer.CreateRenderStateFromScene(*mScene);
        ConfigureRendererForScene();

        const auto& klass = mScene->GetClass();
        if (klass.HasTilemap())
        {
            const auto& mapId = klass.GetTilemapId();
            const auto& map   = mClasslib->FindTilemapClassById(mapId);
            if (map == nullptr)
            {
                ERROR("Failed to load tilemap class object. [id='%1']", mapId);
            }
            else
            {
                mTilemap = game::CreateTilemap(map);              
                mTilemap->Load(*mGameLoader, 1024); // todo:
            }
        }

        mRuntime->BeginPlay(mScene.get());
    }
    void OnAction(const engine::SuspendAction& action)
    {

    }
    void OnAction(const engine::ResumeAction& action)
    {

    }
    void OnAction(const engine::EndPlayAction& action)
    {
        if (!mScene)
            return;
        mRuntime->EndPlay(mScene.get());
        mScene.reset();
        mTilemap.reset();
    }
    void OnAction(const engine::QuitAction& action)
    {
        // todo: cleanup?
        mRequests.Quit(action.exit_code);
    }

    void OnAction(const engine::DebugClearAction& action)
    {
        mDebugPrints.clear();
    }
    void OnAction(const engine::DebugPrintAction& action)
    {
        if (action.clear)
            mDebugPrints.clear();
        DebugPrint print;
        print.message = action.message;
        mDebugPrints.push_back(std::move(print));
    }

    void OnAction(const engine::DelayAction& action)
    {
        mActionDelay = math::clamp(0.0f, action.seconds, action.seconds);
        DEBUG("Action delay: %1 s", mActionDelay);
    }
    void OnAction(const engine::ShowDebugAction& action)
    {
        mFlags.set(Flags::ShowDebugs, action.show);
        DEBUG("Show debugs is %1", action.show ? "ON" : "OFF");
    }
    void OnAction(const engine::ShowMouseAction& action)
    {
        mFlags.set(Flags::ShowMouseCursor, action.show);
        DEBUG("Mouse cursor is %1", action.show ? "ON" : "OFF");
    }
    void OnAction(const engine::BlockKeyboardAction& action)
    {
        mFlags.set(Flags::BlockKeyboard, action.block);
        DEBUG("Keyboard block is %1", action.block ? "ON" : "OFF");
    }
    void OnAction(const engine::BlockMouseAction& action)
    {
        mFlags.set(Flags::BlockMouse, action.block);
        DEBUG("Mouse block is %1", action.block ? "ON" : "OFF");
    }
    void OnAction(const engine::GrabMouseAction& action)
    {
        mRequests.GrabMouse(action.grab);
        DEBUG("Requesting to mouse grabbing. [grabbing=%1]", action.grab ? "enable" : "disable");
    }
    void OnAction(const engine::RequestFullScreenAction& action)
    {
        mRequests.SetFullScreen(action.full_screen);
        DEBUG("Requesting window mode change. [mode=%1]", action.full_screen ? "FullScreen" : "Window");
    }
    void OnAction(const engine::PostEventAction& action)
    {
        mRuntime->OnGameEvent(action.event);
    }
    void OnAction(const engine::ShowDeveloperUIAction& action)
    {
        mRequests.ShowDeveloperUI(action.show);
        DEBUG("Requesting developer UI. [show=%1]", action.show);
    }
    void OnAction(const engine::DebugDrawCircle& draw)
    {
        mDebugDraws.emplace_back(draw);
    }
    void OnAction(const engine::DebugDrawLine& draw)
    {
        mDebugDraws.emplace_back(draw);
    }
    void OnAction(const engine::DebugDrawRect& draw)
    {
        mDebugDraws.emplace_back(draw);
    }
    void OnAction(const engine::DebugPauseAction& pause)
    {
        mRequests.DebugPause(pause.pause);
    }

    void OnAction(const engine::EnableEffectAction& action)
    {
        DEBUG("Enable disable rendering effect. [name='%1', value=%2]", action.name, action.value ? "enable" : "disable");
        if (action.name == "Bloom")
            mFlags.set(Flags::EnableBloom, action.value);
        else WARN("Unidentified effect name. [effect='%1']", action.name);
        ConfigureRendererForScene();
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
    void UpdateGame(double game_time,  double dt)
    {
        if (mScene)
        {
            std::vector<game::Scene::Event> events;
            TRACE_CALL("Scene::Update", mScene->Update(dt, &events));
            TRACE_BLOCK("Scene::Events",
                for (const auto& event : events)
                {
                    mRuntime->OnSceneEvent(event);
                }
            );
            if (mPhysics.HaveWorld())
            {
                std::vector<engine::ContactEvent> contacts;
                // Apply any pending changes such as velocity updates,
                // rigid body flag state changes, new rigid bodies etc.
                // changes to the physics world.
                TRACE_CALL("Physics::UpdateWorld", mPhysics.UpdateWorld(*mScene));
                // Step the simulation forward.
                TRACE_CALL("Physics::Step", mPhysics.Step(&contacts));
                // Update the result of the physics simulation from the
                // physics world to the scene and its entities.
                TRACE_CALL("Physics::UpdateScene", mPhysics.UpdateScene(*mScene));
                // dispatch the contact events (if any).
                TRACE_BLOCK("Physics::ContactEvents",
                    for (const auto& contact : contacts)
                    {
                        mRuntime->OnContactEvent(contact);
                    }
                );
            }

            // Update renderers data structures from the scene.
            // This involves creating new render nodes for new entities
            // that have been spawned etc.
            TRACE_CALL("Renderer::UpdateState", mRenderer.UpdateRenderStateFromScene(*mScene));
            // Update the rendering state, animate materials and drawables.
            TRACE_CALL("Renderer::Update", mRenderer.Update(game_time, dt));
        }

        TRACE_CALL("Runtime::Update", mRuntime->Update(game_time, dt));

        if (auto* ui = GetUI())
        {
            // the Game UI runs in "game time". this is in contrast to
            // any possible "in game debug/system menu" that would be
            // running in real wall time.

            // Update the UI materials in order to do material animation.
            TRACE_CALL("UI Painter::Update", mUIPainter.Update(game_time, dt));

            // Update the UI widgets in order to do widget animation.
            TRACE_CALL("UI Window::Update", ui->Update(mUIState, game_time, dt));

            const auto& actions = ui->PollAction(mUIState, game_time, dt);
            for (const auto& action : actions)
            {
                mRuntime->OnUIAction(ui, action);
            }
        }

        gfx::Drawable::Environment env; // todo:
        mMouseMaterial->Update(dt);
        mMouseDrawable->Update(env, dt);
    }
    void PerformGameActions(float dt)
    {
        // todo: the action processing probably needs to be split
        // into game-actions and non-game actions. For example the
        // game might insert an additional delay in order to transition
        // from one game state to another but likely want to transition
        // to full screen mode in real time. (non game action)
        mActionDelay = math::clamp(0.0f, mActionDelay, mActionDelay - dt);
        if (mActionDelay > 0.0f)
            return;

        engine::Action action;
        while (mRuntime->GetNextAction(&action))
        {
            std::visit([this](auto& variant_value) {
                this->OnAction(variant_value);
            }, action);
            // action delay can be changed by the delay action.
            if (mActionDelay > 0.0f)
                break;
        }
    }

    void ConfigureRendererForScene()
    {
        if (auto* bloom = mScene->GetBloom(); bloom && mFlags.test(Flags::EnableBloom))
        {
            engine::Renderer::BloomParams bloom_params;
            bloom_params.threshold = bloom->threshold;
            bloom_params.red       = bloom->red;
            bloom_params.green     = bloom->green;
            bloom_params.blue      = bloom->blue;
            mRenderer.EnableEffect(engine::Renderer::Effects::Bloom, true);
            mRenderer.SetBloom(bloom_params);
        }
        else
        {
            mRenderer.EnableEffect(engine::Renderer::Effects::Bloom, false);
        }
    }


    void DrawDebugObjects()
    {
        if (!mDebug.debug_draw)
            goto beach;

         TRACE_BLOCK("DebugDrawLines",
            if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::GameDebugDraw))
            {
                for (const auto& draw: mDebugDraws)
                {
                    if (const auto* ptr = std::get_if<engine::DebugDrawLine>(&draw))
                        gfx::DrawLine(*mPainter, ptr->a, ptr->b, ptr->color, ptr->width);
                    else if (const auto* ptr = std::get_if<engine::DebugDrawRect>(&draw))
                        gfx::DrawRect(*mPainter, gfx::FRect(ptr->top_left, ptr->bottom_right), ptr->color, ptr->width);
                    else if (const auto* ptr = std::get_if<engine::DebugDrawCircle>(&draw))
                        gfx::DrawCircle(*mPainter, gfx::FCircle(ptr->center, ptr->radius), ptr->color, ptr->width);
                    else BUG("Missing debug draw implementation");
                }
            }
        );

        TRACE_BLOCK("DebugDrawPhysics",
            if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::PhysicsBody))
            {
                if (mPhysics.HaveWorld())
                {
                    mPhysics.DebugDrawObjects(*mPainter);
                }
            }
        );

        TRACE_CALL("DebugDrawScene", DebugDrawScene());

        beach:
            mDebugDraws.clear();
    }

    void DebugDrawScene() const
    {
        if (!mScene)
            return;

        // this debug drawing is provided for the game developer to help them
        // see where the spatial nodes are, not for the engine developer to
        // debug the engine code. So this means that we assume that the engine
        // code is correct and the spatial index correctly reflects the nodes
        // and their positions. Thus the debug drawing can be based on the
        // entity/node iteration instead of iterating over the items in the
        // spatial index. (Which is a function that doesn't event exist yet).
        for (size_t i = 0; i < mScene->GetNumEntities(); ++i)
        {
            const auto& entity = mScene->GetEntity(i);
            for (size_t j = 0; j < entity.GetNumNodes(); ++j)
            {
                const auto& node = entity.GetNode(j);
                if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::SpatialIndex))
                {
                    if (!node.HasSpatialNode())
                        continue;

                    const auto& aabb = mScene->FindEntityNodeBoundingRect(&entity, &node);
                    gfx::DrawRect(*mPainter, aabb, gfx::Color::Gray, 1.0f);
                }
            }
            if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::BoundingRect))
            {
                const auto rect = mScene->FindEntityBoundingRect(&entity);
                gfx::DrawRectOutline(*mPainter, rect, gfx::Color::Yellow, 1.0f);
            }
            if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::BoundingBox))
            {
                // todo: need to implement the minimum bounding box computation first
            }
        }
    }

private:
    enum class Flags {
        // flag to indicate editing mode, i.e. run by the editor and
        // any static content is actually not static.
        EditingMode,
        // flag to indicate whether the app is still running or not.
        Running,
        // flag to indicate whether currently in fullscreen or not.
        Fullscreen,
        // flag to control keyboard event blocking. Note that the game
        // might still be polling the keyboard through a direct call
        // to read the keyboard state, so the flag  applies only to the
        // keyboard window events.
        BlockKeyboard,
        // flag to control mouse event blocking.
        BlockMouse,
        // flag to control whether to show (or not) mouse cursor
        ShowMouseCursor,
        // flag to control the debug string printing.
        ShowDebugs,
        // flag to control physics world creation.
        EnablePhysics,
        // master flag to control bloom PP in the renderer, controlled by the game.
        EnableBloom
    };
    // current engine flags to control execution etc.
    base::bitflag<Flags> mFlags;
    unsigned mFrameCounter = 0;
    // current FBO 0 surface sizes
    unsigned mSurfaceWidth  = 0;
    unsigned mSurfaceHeight = 0;
    // current mouse cursor details
    EngineConfig::MouseCursorUnits mCursorUnits = EngineConfig::MouseCursorUnits::Pixels;
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
    // Engine data loader for the engine and the for
    // the subsystems that don't have their own specific loader.
    engine::Loader* mEngineDataLoader = nullptr;
    // audio stream loader
    audio::Loader* mAudioLoader = nullptr;
    // Game data loader.
    game::Loader* mGameLoader = nullptr;
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
    engine::UIKeyMap mUIKeyMap;
    // The audio engine.
    std::unique_ptr<engine::AudioEngine> mAudio;
    // The game runtime that runs the actual game logic.
    std::unique_ptr<engine::GameRuntime> mRuntime;
    // Current game scene or nullptr if no scene.
    std::unique_ptr<game::Scene> mScene;
    // Current tilemap or nullptr if no map.
    std::unique_ptr<game::Tilemap> mTilemap;
    // The UI stack onto which UIs are opened.
    // The top of the stack is the currently "active" UI
    // that gets the mouse/keyboard input events. It's
    // possible that the stack is empty if the game is
    // not displaying any UI
    std::stack<std::shared_ptr<uik::Window>> mUIStack;
    // Transient UI state.
    uik::State mUIState;
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
    // Time to consume until game actions are processed.
    float mActionDelay = 0.0;
    // The size of the game time step (in seconds) to take for each
    // update of the game simulation state.
    float mGameTimeStep = 0.0f;
    // The size of the game tick step (in seconds) to take for each
    // tick of the game state
    float mGameTickStep = 0.0f;
    // accumulation counters for keeping track of left over time
    // available for taking update and tick steps.
    float mTickAccum = 0.0f;
    float mTimeAccum = 0.0f;
    // Total accumulated game time so far.
    double mGameTimeTotal = 0.0f;
    // The bitbag for storing game state.
    engine::KeyValueStore mStateStore;
    // Debug draw actions.
    using DebugDraw = std::variant<engine::DebugDrawLine, engine::DebugDrawRect, engine::DebugDrawCircle>;
    // Current debug draw actions that are queued for the
    // next draw. next call to draw will draw them and clear
    // the vector.
    std::vector<DebugDraw> mDebugDraws;
    // debug stepping flag to control taking a single update step.
    bool mStepForward = false;
};

} //namespace

extern "C" {
GAMESTUDIO_EXPORT engine::Engine* Gamestudio_CreateEngine()
{
#if defined(NDEBUG)
    DEBUG("DETONATOR 2D Engine in release build. *Kapow!*");
#else
    DEBUG("DETONATOR 2D Engine in DEBUG build. *pof*");
#endif
    return new GameStudioEngine;
}
} // extern C
