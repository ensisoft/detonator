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
#define ENGINE_DLL_IMPLEMENTATION

#define ENGINE_USE_UPDATE_THREAD

#include "warnpush.h"
#  include <neargye/magic_enum.hpp>
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <stack>
#include <queue>

#include "base/bitflag.h"
#include "base/logging.h"
#include "base/trace.h"
#include "base/threadpool.h"
#include "audio/graph.h"
#include "game/entity.h"
#include "game/entity_node_drawable_item.h"
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
#include "graphics/material_instance.h"
#include "graphics/utility.h"
#include "graphics/simple_shape.h"
#include "graphics/texture_bitmap_buffer_source.h"
#include "engine/engine.h"
#include "engine/audio.h"
#include "engine/camera.h"
#include "engine/classlib.h"
#include "engine/renderer.h"
#include "engine/event.h"
#include "engine/physics.h"
#include "engine/game.h"
#include "engine/lua_game_runtime.h"
#include "engine/format.h"
#include "engine/library/library.h"
#include "engine/ui.h"
#include "engine/state.h"
#include "uikit/window.h"
#include "uikit/painter.h"
#include "uikit/state.h"

namespace
{
static const unsigned char DetonatorLOGO[] = {
#include "engine/engine-logo.h"
};
// we'll do this dynamically in loading screen.
static unsigned LogoWidth  = 0;
static unsigned LogoHeight = 0;

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

    virtual void Init(const InitParams& init, const EngineConfig& conf) override
    {
        DEBUG("Engine initializing.");
        audio::Format audio_format;
        audio_format.channel_count = static_cast<unsigned>(conf.audio.channels); // todo: use enum
        audio_format.sample_rate   = conf.audio.sample_rate;
        audio_format.sample_type   = conf.audio.sample_type;

        mAudio = std::make_unique<engine::AudioEngine>(init.application_name);
        mAudio->SetClassLibrary(mClasslib);
        mAudio->SetLoader(mAudioLoader);
        mAudio->SetFormat(audio_format);
        mAudio->SetBufferSize(conf.audio.buffer_size);
        mAudio->EnableCaching(conf.audio.enable_pcm_caching);
        mAudio->Start();

        auto device = dev::CreateDevice(init.context);
        mDevice = gfx::CreateDevice(device->GetSharedGraphicsDevice());
        mDevice->SetDefaultTextureFilter(conf.default_min_filter);
        mDevice->SetDefaultTextureFilter(conf.default_mag_filter);

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
        mUIEngine.SetClassLibrary(mClasslib);
        mUIEngine.SetLoader(mEngineDataLoader);
        mUIEngine.SetSurfaceSize(init.surface_width, init.surface_height);
        mUIEngine.SetEditingMode(init.editing_mode);
        mRenderer.SetClassLibrary(mClasslib);
        mRenderer.SetEditingMode(init.editing_mode);
        mRenderer.SetName("Engine");
        mRenderer.EnableEffect(engine::Renderer::Effects::Bloom, true);

        mPhysics.SetClassLibrary(mClasslib);
        mPhysics.SetScale(conf.physics.scale);
        mPhysics.SetGravity(conf.physics.gravity);
        mPhysics.SetNumPositionIterations(conf.physics.num_position_iterations);
        mPhysics.SetNumVelocityIterations(conf.physics.num_velocity_iterations);
        mPhysics.SetTimestep(1.0f / conf.updates_per_second);

        mFlags.set(Flags::EditingMode,     init.editing_mode);
        mFlags.set(Flags::Running,         true);
        mFlags.set(Flags::Fullscreen,      false);
        mFlags.set(Flags::BlockKeyboard,   false);
        mFlags.set(Flags::BlockMouse,      false);
        mFlags.set(Flags::ShowMouseCursor, true);
        mFlags.set(Flags::ShowDebugs,      true);
        mFlags.set(Flags::EnableBloom,     true);
        mFlags.set(Flags::EnablePhysics,   true);
        mFlags.set(Flags::ShowMouseCursor, conf.mouse_cursor.show);
        mFlags.set(Flags::EnablePhysics, conf.physics.enabled);

        auto mouse_drawable = mClasslib->FindDrawableClassById(conf.mouse_cursor.drawable);
        DEBUG("Mouse material='%1',drawable='%2'", conf.mouse_cursor.material, conf.mouse_cursor.drawable);
        if (!mouse_drawable)
        {
            WARN("No such mouse cursor drawable found. [drawable='%1']", conf.mouse_cursor.drawable);
            mouse_drawable = std::make_shared<gfx::ArrowCursorClass>();
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
            mouse_material = material;
        }
        mMouseDrawable = gfx::CreateDrawableInstance(mouse_drawable);
        mMouseMaterial = gfx::CreateMaterialInstance(mouse_material);

        mClearColor = conf.clear_color;
        mGameTimeStep = 1.0f / conf.updates_per_second;
        mGameTickStep = 1.0f / conf.ticks_per_second;
        mSurfaceWidth  = init.surface_width;
        mSurfaceHeight = init.surface_height;
        mCursorUnits   = conf.mouse_cursor.units;
        DEBUG("Graphics surface %1x%2]", init.surface_width, init.surface_height);
    }

    struct LoadingScreen : public Engine::LoadingScreen {
        std::unique_ptr<uik::Window> splash;
        uik::TransientState state;
        engine::UIStyle style;
        engine::UIPainter painter;
        std::vector<uik::Animation> animations;
        std::string font;
        std::unique_ptr<gfx::Material> logo;
    };
    virtual std::unique_ptr<Engine::LoadingScreen> CreateLoadingScreen(const LoadingScreenSettings& settings) override
    {
        auto state = std::make_unique<LoadingScreen>();

        gfx::Image png((const void*)DetonatorLOGO, sizeof(DetonatorLOGO));
        if (png.IsValid())
        {
            auto texture_source = std::make_unique<gfx::TextureBitmapBufferSource>();
            texture_source->SetBitmap(png.AsBitmap<gfx::Pixel_RGBA>());
            texture_source->SetColorSpace(gfx::TextureSource::ColorSpace::sRGB);
            texture_source->SetName("Detonator Logo");

            gfx::MaterialClass logo(gfx::MaterialClass::Type::Texture);
            logo.SetTexture(std::move(texture_source));

            state->logo = gfx::CreateMaterialInstance(logo);
            LogoWidth  = png.GetWidth();
            LogoHeight = png.GetHeight();
        }
        state->font = settings.font_uri;
        return state;
    }

    virtual void PreloadClass(const Engine::ContentClass& klass, size_t index, size_t last, Engine::LoadingScreen* screen) override
    {
        gfx::Painter dummy;
        dummy.SetEditingMode(mFlags.test(Flags::EditingMode));
        dummy.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
        dummy.SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
        dummy.SetPixelRatio(glm::vec2{1.0f, 1.0});
        dummy.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, mSurfaceWidth, mSurfaceHeight));
        dummy.SetScissor(0, 0, 1, 1);
        dummy.SetDevice(mDevice);

        // todo: We currently don't have a good mechanism to understand and track
        // all the possible content (shaders, programs, textures and geometries)
        // that the game needs. The packaging process should be enhanced so that
        // the loading process can be improved.

        // The implementation here is a kludge hack basically doing a dry-run
        // rendering without anything visible getting rendered. This will hopefully
        // precipitate data generation on the GPU.

        if (klass.type == engine::ClassLibrary::ClassType::Entity)
        {
            const auto& entity = mClasslib->FindEntityClassById(klass.id);
            if (!entity)
                return;

            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                const auto& node = entity->GetNode(i);
                if (!node.HasDrawable())
                    continue;

                const auto* item = node.GetDrawable();
                const auto& materialId = item->GetMaterialId();
                const auto& drawableId = item->GetDrawableId();

                const auto& materialClass = mClasslib->FindMaterialClassById(materialId);
                const auto& drawableClass = mClasslib->FindDrawableClassById(drawableId);
                if (!materialClass || !drawableClass)
                    continue;

                const auto& material = gfx::CreateMaterialInstance(materialClass);
                const auto& drawable = gfx::CreateDrawableInstance(drawableClass);

                glm::mat4 model(1.0f);
                gfx::Drawable::Environment  env;
                env.editing_mode = mFlags.test(Flags::EditingMode);
                env.model_matrix = &model;

                float time = 0.0f;
                float step = 1.0f/60.0f;
                while (time < 5.0f)
                {
                    glm::mat4 model(1.0f);
                    dummy.Draw(*drawable, model, *material);
                    material->Update(step);
                    drawable->Update(env, step);
                    time += step;
                }
            }
        }

        if (klass.type == engine::ClassLibrary::ClassType::AudioGraph)
        {
            const auto& graph = mClasslib->FindAudioGraphClassById(klass.id);
            if (!graph)
                return;
            audio::GraphClass::PreloadParams params;
            params.enable_pcm_caching = mAudio->IsCachingEnabled();

            graph->Preload(*mAudioLoader, params);
        }

        if (klass.type == engine::ClassLibrary::ClassType::UI)
        {
            const auto& window_template = mClasslib->FindUIById(klass.id);
            if (!window_template)
                return;

            auto ui = std::make_shared<uik::Window>(*window_template);
            mUIEngine.OpenWindowStackState(ui);

            float time = 0.0f;
            float step = 1.0f / 60.0f;
            while (time < 5.0f)
            {
                std::vector<engine::UIEngine::WidgetAction> widget_actions;
                std::vector<engine::UIEngine::WindowAction> window_actions;
                mUIEngine.UpdateWindow(time, step, &widget_actions);
                mUIEngine.UpdateState(time, step, &window_actions);
                mUIEngine.Draw(*mDevice);
                time += step;
            }
            mUIEngine.CloseWindowStackState();
        }


        auto* my_screen = static_cast<LoadingScreen*>(screen);
        const float surf_width = mSurfaceWidth;
        const float surf_height = mSurfaceHeight;

        mDevice->BeginFrame();
        mDevice->ClearColor(gfx::Color::Black);
        mDevice->ClearDepth(1.0f);

        if (my_screen->splash)
        {
            auto& ui_splash  = *my_screen->splash;
            auto& ui_painter = my_screen->painter;
            auto& ui_state   = my_screen->state;
            auto& ui_style   = my_screen->style;

            // the viewport retains the UI's aspect ratio and is centered in the middle
            // of the rendering surface.
            const auto& window_rect = ui_splash.GetBoundingRect();
            const float width = window_rect.GetWidth();
            const float height = window_rect.GetHeight();
            const float scale = std::min(surf_width / width, surf_height / height);
            const float device_viewport_width = width * scale;
            const float device_viewport_height = height * scale;

            gfx::IRect device_viewport;
            device_viewport.Move((surf_width - device_viewport_width) * 0.5,
                                 (surf_height - device_viewport_height) * 0.5);
            device_viewport.Resize(device_viewport_width, device_viewport_height);

            gfx::Painter painter(mDevice);
            painter.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
            painter.SetPixelRatio(glm::vec2(1.0f, 1.0f));
            painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, width, height));
            painter.ResetViewMatrix();
            painter.SetViewport(device_viewport);
            painter.SetEditingMode(mFlags.test(Flags::EditingMode));

            ui_painter.SetPainter(&painter);
            ui_painter.SetStyle(&ui_style);
            ui_splash.Paint(ui_state, ui_painter, base::GetTime(), nullptr);
            ui_painter.SetPainter(nullptr);
        }
        else if (!my_screen->font.empty())
        {
            gfx::Painter painter;
            painter.SetDevice(mDevice);
            painter.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
            painter.SetPixelRatio(glm::vec2(1.0f, 1.0f));
            painter.SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
            painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, mSurfaceWidth, mSurfaceHeight));
            painter.SetEditingMode(mFlags.test(Flags::EditingMode));

            painter.ClearColor(gfx::Color::Black);
            const auto& window = gfx::FRect(0.0f, 0.0f, mSurfaceWidth, mSurfaceHeight);

            const auto have_logo = LogoWidth && LogoHeight;
            const auto& logo_rect = CenterRectOnRect(window, gfx::FRect(0.0f, 0.0f,
                                        have_logo ? LogoWidth / 2 : 500.0f,
                                        have_logo ? LogoHeight / 2 : 300.0f));
            if (my_screen->logo)
            {
                gfx::FillRect(painter, logo_rect, *my_screen->logo);
            }

            gfx::FRect text_rect;
            text_rect.Resize(logo_rect.GetWidth(), 50.0f);
            text_rect.Translate(logo_rect.GetPosition());
            text_rect.Translate(0.0f, logo_rect.GetHeight());

            const int done = ((float)index / (float)last) * 100;

            gfx::FillRect(painter, text_rect, gfx::Color::Black);
            gfx::DrawTextRect(painter,  base::FormatString("Loading ... %1%\n%2", done, klass.name),
                              my_screen->font, 22, text_rect,
                              gfx::Color::Silver,
                              gfx::TextAlign::AlignVCenter | gfx::TextAlign::AlignHCenter);
        }

        // this is for debugging so we can see what happens...
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));

        mDevice->EndFrame(true);
    }

    virtual void NotifyClassUpdate(const ContentClass& klass) override
    {
        DEBUG("Content class was updated. [type=%1, name='%2', id='%2]", klass.type, klass.name, klass.id);

        engine::GameRuntime::ContentClass k;
        k.type = klass.type;
        k.name = klass.name;
        k.id   = klass.id;
        mRuntime->OnContentClassUpdate(k);
    }
    virtual void SetRendererConfig(const RendererConfig& config) override
    {
        mRendererConfig = config;
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

        if (mDebug.debug_show_fps || mDebug.debug_show_msg)
        {
            if (mDebug.debug_font.empty())
            {
                WARN("Debug font is empty (no font selected).");
                WARN("Debug prints will not be available.");
                WARN("You can set the debug font in the project settings.");
            }
        }
    }
    virtual void DebugPrintString(const std::string& message) override
    {
        DebugPrint print;
        print.message = message;
        mDebugPrints.push_back(std::move(print));
    }
    virtual void SetTracer(base::Trace* tracer, base::TraceWriter* writer) override
    {
        if (mAudio)
            mAudio->SetAudioThreadTraceWriter(writer);
    }
    virtual void SetTracingOn(bool on_off) override
    {
        if (mAudio)
            mAudio->EnableAudioThreadTrace(on_off);
    }

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

    virtual void Draw(float dt) override
    {
        mDevice->BeginFrame();
        mDevice->ClearColor(mClearColor);
        mDevice->ClearDepth(1.0f);

        // Do the main drawing here based on previously generated
        // draw packets that are stored in the renderer.
        // this state is rebuilt in the the call to renderer's
        // CreateRenderPackets, which is an operation that cannot
        // run at the same time when we're drawing.
        TRACE_CALL("Renderer::DrawFrame", mRenderer.DrawFrame(*mDevice));

#if defined(ENGINE_USE_UPDATE_THREAD)
        base::TaskHandle next_frame_task;

        // if we had updates running in parallel then complete (wait)
        // the tasks here. This is unfortunately needed in order to
        // make sure that the update thread is no longer touching
        // the UI system or the scene.
        if (!mUpdateTasks.empty())
        {
            // Wait each update task
            for (auto& handle: mUpdateTasks)
            {
                TRACE_CALL("WaitSceneUpdate", handle.Wait(base::TaskHandle::WaitStrategy::BusyLoop));
            }

            // make sure that we first wait everything and only then throw
            // if anything failed.
            for (auto& handle : mUpdateTasks)
            {
                const auto* task = handle.GetTask();
                if (task->HasException())
                    task->RethrowException();
            }

            mUpdateTasks.clear();

            class CreateNextFrameTask : public base::ThreadTask {
            public:
                explicit CreateNextFrameTask(GameStudioEngine* engine)
                  : mEngine(engine)
                {}
            protected:
                void DoTask() override
                {
                    mEngine->CreateNextFrame();
                }

            private:
                GameStudioEngine* mEngine = nullptr;
            };

            // create task to create the next frame in the renderer
            // we can do this in parallel while this thread can proceed
            // to draw game UI, debug objects etc.
            auto* thread_pool = base::GetGlobalThreadPool();
            auto thread_task = std::make_unique<CreateNextFrameTask>(this);
            next_frame_task = thread_pool->SubmitTask(std::move(thread_task), base::ThreadPool::UpdateThreadID);

            // update the debug draws only after updating the game
            // if this is done per each frame they will not be seen
            // by the user if the rendering is running very fast.
            std::vector<engine::DebugDrawCmd> debug_draws;
            mRuntime->TransferDebugQueue(&debug_draws);
            std::swap(mDebugDraws, debug_draws);
        }
#else
        CreateNextFrame();
#endif
        // Continue drawing more stuff while the renderer update
        // task runs in parallel.
        TRACE_CALL("Engine::DrawGameUI",        DrawGameUI());
        TRACE_CALL("Engine::DrawDebugObjects",  DrawDebugObjects());
        TRACE_CALL("Engine::DrawDebugMessages", DrawDebugMessages());
        TRACE_CALL("Engine::DrawMousePointer",  DrawMousePointer(dt));

        TRACE_CALL("Device::EndFrame", mDevice->EndFrame(true));
        // Note that we *don't* call CleanGarbage here since currently there should
        // be nothing that is creating needless GPU resources.

#if defined(ENGINE_USE_UPDATE_THREAD)
        // complete the update/render loop, wait this task here
        // so that the update/rendering stay in sync.
        if (next_frame_task)
        {
            TRACE_CALL("WaitCreateFrame", next_frame_task.Wait(base::TaskHandle::WaitStrategy::BusyLoop));
        }
#endif

        if (mDebug.debug_pause && !mStepForward)
            return;

        TRACE_CALL("HandleGameActions", PerformGameActions(dt));
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

#if defined(ENGINE_USE_UPDATE_THREAD)
        auto* thread_pool = base::GetGlobalThreadPool();

        class UpdateTask : public base::ThreadTask {
        public:
            explicit UpdateTask(GameStudioEngine* engine, double total_time, float time_step) noexcept
              : mGameTimeTotal(total_time)
              , mGameTimeStep(time_step)
              , mEngine(engine)
            {}
        protected:
            void DoTask() override
            {
                TRACE_CALL("UpdateGame", mEngine->UpdateGame(mGameTimeTotal, mGameTimeStep));
                TRACE_CALL("UpdateGameUI", mEngine->UpdateGameUI(mGameTimeTotal, mGameTimeStep));
            }
        private:
            const double mGameTimeTotal = 0.0;
            const double mGameTimeStep  = 0.0;
            GameStudioEngine* mEngine = nullptr;
        };
#endif

        // do simulation/animation update steps.
        while (mTimeAccum >= mGameTimeStep)
        {
            // Call UpdateGame with the *current* time. I.e. the game
            // is advancing one time step from current mGameTimeTotal.
            // this is consistent with the tick time accumulation below.
#if defined(ENGINE_USE_UPDATE_THREAD)
            auto task = std::make_unique<UpdateTask>(this, mGameTimeTotal, mGameTimeStep);
            mUpdateTasks.push_back(thread_pool->SubmitTask(std::move(task), base::ThreadPool::UpdateThreadID));
#else
            TRACE_CALL("UpdateGame", UpdateGame(mGameTimeTotal, mGameTimeStep));
            TRACE_CALL("UpdateGameUI", UpdateGameUI(mGameTimeTotal, mGameTimeStep));
#endif
            mGameTimeTotal += mGameTimeStep;
            mTimeAccum -= mGameTimeStep;

            // if we're paused for debugging stop after one step forward.
            mStepForward = false;
        }
#if !defined(ENGINE_USE_UPDATE_THREAD)
        // update the debug draws only after updating the game
        // if this is done per each frame they will not be seen
        // by the user if the rendering is running very fast.
        std::vector<engine::DebugDrawCmd> debug_draws;
        mRuntime->TransferDebugQueue(&debug_draws);
        std::swap(mDebugDraws, debug_draws);
#endif

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
            DEBUG("FPS: %1, wall_time: %2, frames: %3",
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
        gfx::Bitmap<gfx::Pixel_RGB> rgb;
        rgb.Resize(rgba.GetWidth(), rgba.GetHeight());
        for (unsigned y=0; y<rgba.GetHeight(); ++y) {
            for (unsigned x=0; x<rgba.GetWidth(); ++x) {
                const auto src = rgba.GetPixel(y, x);
                const auto alpha = src.a / 255.0;
                const auto dst = gfx::Pixel_RGB(src.r * alpha, src.g * alpha, src.b * alpha);
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

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnKeyDown(key, &actions);

        mRuntime->OnUIAction(mUIEngine.GetUI(), actions);

        mRuntime->OnKeyDown(key);
    }
    virtual void OnKeyUp(const wdk::WindowEventKeyUp& key) override
    {       
        if (mFlags.test(Flags::BlockKeyboard))
            return;

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnKeyUp(key, &actions);

        mRuntime->OnUIAction(mUIEngine.GetUI(), actions);

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

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnMouseMove(mouse, &actions);

        mRuntime->OnUIAction(mUIEngine.GetUI(), actions);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMouseMove);
    }
    virtual void OnMousePress(const wdk::WindowEventMousePress& mouse) override
    {
        if (mFlags.test(Flags::BlockMouse))
            return;

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnMousePress(mouse, &actions);

        mRuntime->OnUIAction(mUIEngine.GetUI(), actions);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMousePress);
    }
    virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse) override
    {
        if (mFlags.test(Flags::BlockMouse))
            return;

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnMouseRelease(mouse, &actions);

        mRuntime->OnUIAction(mUIEngine.GetUI(), actions);

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMouseRelease);
    }
private:
    engine::IRect GetViewport() const
    {
        // get the game's logical viewport into the game world.
        const auto& view = mRuntime->GetCamera().viewport;
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
        const auto& camera = mRuntime->GetCamera();
        const auto& device_viewport  = GetViewport();

        engine::MouseEvent event;
        event.window_coord = glm::vec2(mickey.window_x, mickey.window_y);
        event.btn          = mickey.btn;
        event.mods         = mickey.modifiers;
        if (device_viewport.TestPoint(mickey.window_x, mickey.window_y))
        {
            const auto& point  = device_viewport.MapToLocal(mickey.window_x, mickey.window_y);
            const auto& view_to_clip  = engine::CreateProjectionMatrix(engine::Projection::Orthographic, camera.viewport);
            const auto& world_to_view = engine::CreateModelViewMatrix(engine::GameView::AxisAligned,
                                                                      camera.position,
                                                                      camera.scale,
                                                                      0.0f); // camera rotation
            event.over_scene  = true;
            event.scene_coord = engine::MapFromWindowToWorldPlane(view_to_clip,
                                                                  world_to_view,
                                                                  glm::vec2{point.GetX(), point.GetY()},
                                                                  glm::vec2{device_viewport.GetWidth(),
                                                                            device_viewport.GetHeight()});
            event.map_coord = event.scene_coord;
            if (mTilemap)
            {
                const auto perspective = mTilemap->GetPerspective();
                if (perspective != engine::GameView::AxisAligned)
                    event.map_coord = engine::MapFromScenePlaneToTilePlane(
                            glm::vec4{event.scene_coord.x, event.scene_coord.y, 0.0f, 1.0}, perspective);
            }
        }

        return event;
    }
    void OnAction(const engine::OpenUIAction& action)
    {
        TRACE_CALL("UI::Open", mUIEngine.OpenUI(action.ui));
    }
    void OnAction(const engine::CloseUIAction& action)
    {
        TRACE_CALL("Ui::Close", mUIEngine.CloseUI(action.window_name, action.action_id, action.result));
    }
    void OnAction(engine::PlayAction& action)
    {
        mScene = std::move(action.scene);
        if (mFlags.test(Flags::EnablePhysics))
        {
            mPhysics.DeleteAll();
            mPhysics.CreateWorld(*mScene);
        }

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
                TRACE_CALL("Tilemap::Create", mTilemap = game::CreateTilemap(map));
                TRACE_CALL("Tilemap::Load", mTilemap->Load(*mGameLoader));
                DEBUG("Created tilemap instance");
            }
        } else mTilemap.reset();

        mScene->SetMap(mTilemap.get());

        TRACE_CALL("Renderer::CreateState", mRenderer.CreateRendererState(*mScene, mTilemap.get()));
        ConfigureRendererForScene();

        TRACE_CALL("Runtime::BeginPlay", mRuntime->BeginPlay(mScene.get(), mTilemap.get()));
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
        mRuntime->EndPlay(mScene.get(), mTilemap.get());
        mScene.reset();
        mTilemap.reset();
        mPhysics.DeleteAll();
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
    void OnAction(const engine::EnableTracing& action)
    {
        DEBUG("Enable function tracing. [value=%1]", action.enabled ? "enable" : "disable");
        mRequests.EnableTracing(action.enabled);
    }
    void OnAction(const engine::EnableDebugDraw& action)
    {
        DEBUG("Enable debug draw. [value=%1]", action.enabled ? "enable" : "disable");
        mRequests.EnableDebugDraw(action.enabled);
    }

    void UpdateGame(double game_time, float dt)
    {
        if (mScene)
        {
            TRACE_CALL("Scene::BeginLoop", mScene->BeginLoop());
            TRACE_CALL("Runtime:BeginLoop", mRuntime->BeginLoop());

            std::vector<game::Scene::Event> events;
            TRACE_CALL("Scene::Update", mScene->Update(dt, &events));
            TRACE_CALL("Runtime:OnSceneEvent", mRuntime->OnSceneEvent(events));

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
                TRACE_CALL("Runtime::OnContactEvents", mRuntime->OnContactEvent(contacts));
            }
        }

        TRACE_CALL("Runtime::Update", mRuntime->Update(game_time, dt));

        // Tick game
        {
            mTickAccum += dt;
            // current game time from which we step forward
            // in ticks later on.
            auto tick_time = game_time;

            while (mTickAccum >= mGameTickStep)
            {
                TRACE_CALL("Runtime::Tick", mRuntime->Tick(tick_time, mGameTickStep));
                mTickAccum -= mGameTickStep;
                tick_time += mGameTickStep;
            }
        }

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
            // Update renderers data structures from the scene.
            // This involves creating new render nodes for new entities
            // that have been spawned etc. This needs to be done inside
            // the begin/end loop in order to have the correct signalling
            // i.e. entity control flags.
            TRACE_CALL("Renderer::UpdateState", mRenderer.UpdateRendererState(*mScene, mTilemap.get()));

            // make sure to do this first in order to allow the scene to rebuild
            // the spatial indices etc. before the game's PostUpdate runs.
            TRACE_CALL("Scene::Rebuild", mScene->Rebuild());
            // using the time we've arrived to now after having taken the previous
            // delta step forward in game time.
            TRACE_CALL("Runtime::PostUpdate", mRuntime->PostUpdate(game_time + dt));

            TRACE_CALL("Runtime::EndLoop", mRuntime->EndLoop());
            TRACE_CALL("Scene::EndLoop", mScene->EndLoop());
        }
    }

    void CreateNextFrame()
    {
        const auto now = mGameTimeTotal;
        if (mRenderTimeStamp == 0.0)
            mRenderTimeStamp = now;

        const auto dt  = now - mRenderTimeStamp;

        if (mScene)
        {
            if (SetRendererState())
            {
                TRACE_CALL("Renderer::Update", mRenderer.Update(*mScene, mTilemap.get(), mRenderTimeTotal, dt));
                TRACE_CALL("Renderer::CreateFrame", mRenderer.CreateFrame(*mScene, mTilemap.get()));
                if (mFlags.test(GameStudioEngine::Flags::EditingMode))
                {
                    ConfigureRendererForScene();
                }
            }
        }
        mRenderTimeTotal += dt;
        mRenderTimeStamp = now;
    }

    bool SetRendererState()
    {
        // configure renderer
        const auto surf_width  = (float)mSurfaceWidth;
        const auto surf_height = (float)mSurfaceHeight;
        const auto& game_camera = mRuntime->GetCamera();
        // get the game's logical viewport into the game world.
        const auto& game_view = game_camera.viewport;
        // map the logical viewport to some area in the rendering surface
        // so that the rendering area (the device viewport) has the same
        // aspect ratio as the logical viewport.
        const float game_view_width  = game_view.GetWidth();
        const float game_view_height = game_view.GetHeight();
        // the scaling factor for mapping game units to rendering surface (pixel) units.
        const float game_scale  = std::min(surf_width / game_view_width, surf_height / game_view_height);

        // if the game hasn't set the viewport... then don't draw!
        if (game_view_width <= 0.0f || game_view_height <= 0.0f)
        {
            WARN("Game viewport is invalid. [width=%1, height=%2]", game_view_width, game_view_height);
            return false;
        }

        engine::Renderer::Surface surface;
        surface.viewport = GetViewport();
        surface.size     = base::USize(mSurfaceWidth, mSurfaceHeight);
        mRenderer.SetSurface(surface);

        engine::Renderer::Camera camera;
        camera.clear_color = mClearColor;
        camera.viewport    = game_view;
        camera.scale       = game_camera.scale;
        camera.position    = game_camera.position;
        camera.rotation    = 0.0f;
        camera.ppa         = engine::ComputePerspectiveProjection(game_view);
        mRenderer.SetCamera(camera);
        return true;
    }

    void UpdateGameUI(double game_time, float dt)
    {
        std::vector<engine::UIEngine::WidgetAction> widget_actions;
        std::vector<engine::UIEngine::WindowAction> window_actions;
        TRACE_CALL("UIEngine::UpdateWindow", mUIEngine.UpdateWindow(game_time, dt, &widget_actions));
        TRACE_CALL("UIEngine::UpdateState", mUIEngine.UpdateState(game_time, dt, &window_actions));
        if (auto* ui = mUIEngine.GetUI())
        {
            TRACE_CALL("Runtime::OnUIAction", mRuntime->OnUIAction(ui, widget_actions));
            TRACE_CALL("Runtime::UpdateUI", mRuntime->UpdateUI(ui, game_time, dt));
        }

        for (const auto& w : window_actions)
        {
            if (const auto* ptr = std::get_if<engine::UIEngine::WindowOpen>(&w))
                mRuntime->OnUIOpen(ptr->window);
            else if (const auto* ptr = std::get_if<engine::UIEngine::WindowUpdate>(&w))
                mRuntime->SetCurrentUI(ptr->window);
            else if(const auto* ptr = std::get_if<engine::UIEngine::WindowClose>(&w))
                mRuntime->OnUIClose(ptr->window.get(), ptr->result);
            else BUG("Missing UIEngine window event handling.");
        }
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
        const auto shading = (*mScene)->GetShadingMode();
        if (shading == game::SceneClass::RenderingArgs::ShadingMode::Flat)
            mRenderer.SetStyle(engine::Renderer::RenderingStyle::FlatColor);
        else if (shading == game::SceneClass::RenderingArgs::ShadingMode::BasicLight)
            mRenderer.SetStyle(engine::Renderer::RenderingStyle::BasicShading);
        else BUG("Bug on renderer shading mode.");

        if (mRendererConfig.has_value())
        {
            const auto& value = mRendererConfig.value();
            mRenderer.SetStyle(value.style);
        }
    }

    void DrawMousePointer(float dt)
    {
        if (!mFlags.test(Flags::ShowMouseCursor))
            return;

        gfx::Drawable::Environment env; // todo:
        mMouseMaterial->Update(dt);
        mMouseDrawable->Update(env, dt);

        const auto surf_width  = (float)mSurfaceWidth;
        const auto surf_height = (float)mSurfaceHeight;
        const auto& game_camera = mRuntime->GetCamera();
        // get the game's logical viewport into the game world.
        const auto& game_view = game_camera.viewport;
        // map the logical viewport to some area in the rendering surface
        // so that the rendering area (the device viewport) has the same
        // aspect ratio as the logical viewport.
        const float game_view_width  = game_view.GetWidth();
        const float game_view_height = game_view.GetHeight();

        // the scaling factor for mapping game units to rendering surface (pixel) units.
        const float game_scale  = std::min(surf_width / game_view_width, surf_height / game_view_height);

        // scale the cursor size based on the requested units of the cursor size.
        const auto& size   = mCursorUnits == EngineConfig::MouseCursorUnits::Units
                             ? mCursorSize * game_scale
                             : mCursorSize;
        const auto& offset = -mCursorHotspot*size;

        // this painter will be configured to draw directly in the window
        // coordinates. Used to draw debug text and the mouse cursor.
        gfx::Painter painter;
        painter.SetDevice(mDevice);
        painter.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
        painter.SetPixelRatio(glm::vec2(1.0f, 1.0f));
        painter.SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
        painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, surf_width, surf_height));
        painter.SetEditingMode(mFlags.test(Flags::EditingMode));

        gfx::FRect rect;
        rect.Resize(size.x, size.y);
        rect.Move(mCursorPos.x, mCursorPos.y);
        rect.Translate(offset.x, offset.y);
        gfx::FillShape(painter, rect, *mMouseDrawable, *mMouseMaterial);
    }

    void DrawGameUI()
    {
        mUIEngine.SetSurfaceSize((float)mSurfaceWidth, (float)mSurfaceHeight);
        mUIEngine.Draw(*mDevice);
    }

    void DrawDebugMessages()
    {
        const auto draw_any_debug = mDebug.debug_show_fps ||
                                    mDebug.debug_show_msg ||
                                    mDebug.debug_draw;
        if  (!draw_any_debug)
            return;

        const auto surf_width  = (float)mSurfaceWidth;
        const auto surf_height = (float)mSurfaceHeight;

        // this painter will be configured to draw directly in the window
        // coordinates. Used to draw debug text and the mouse cursor.
        gfx::Painter painter;
        painter.SetDevice(mDevice);
        painter.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
        painter.SetPixelRatio(glm::vec2(1.0f, 1.0f));
        painter.SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
        painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, surf_width, surf_height));
        painter.SetEditingMode(mFlags.test(Flags::EditingMode));

        if (mDebug.debug_show_fps && !mDebug.debug_font.empty())
        {
            char hallelujah[512] = {0};
            std::snprintf(hallelujah, sizeof(hallelujah) - 1,
                          "FPS: %.2f wall time: %.2f frames: %u",
                          mLastStats.current_fps, mLastStats.total_wall_time, mLastStats.num_frames_rendered);

            const gfx::FRect rect(10, 10, 500, 20);
            gfx::FillRect(painter, rect, gfx::Color4f(gfx::Color::Black, 0.6f));
            gfx::DrawTextRect(painter, hallelujah,
                              mDebug.debug_font, 14, rect, gfx::Color::HotPink,
                              gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
        }
        if (mDebug.debug_show_msg && mFlags.test(Flags::ShowDebugs) && !mDebug.debug_font.empty())
        {
            gfx::FRect rect(10, 30, 500, 20);
            for (const auto& print : mDebugPrints)
            {
                gfx::FillRect(painter, rect, gfx::Color4f(gfx::Color::Black, 0.6f));
                gfx::DrawTextRect(painter, print.message,
                                  mDebug.debug_font, 14, rect, gfx::Color::HotPink,
                                  gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
                rect.Translate(0, 20);
            }
        }
        if (mDebug.debug_draw)
        {
            // visualize the game's logical viewport in the window.
            gfx::DrawRectOutline(painter, gfx::FRect(GetViewport()), gfx::Color::Green, 1.0f);
        }
    }


    void DrawDebugObjects()
    {
        if (!mDebug.debug_draw)
        {
            return;
        }

        const auto& camera = mRuntime->GetCamera();
        const auto device_viewport  = GetViewport();
        const auto surface_width  = (float)mSurfaceWidth;
        const auto surface_height = (float)mSurfaceHeight;

        gfx::Painter painter(mDevice);
        painter.SetProjectionMatrix(engine::CreateProjectionMatrix(engine::Projection::Orthographic, camera.viewport));
        painter.SetViewMatrix(engine::CreateModelViewMatrix(engine::GameView::AxisAligned,
                                                            camera.position,
                                                            camera.scale,
                                                            0.0f));
        painter.SetViewport(device_viewport);
        painter.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
        painter.SetEditingMode(mFlags.test(Flags::EditingMode));
        painter.SetPixelRatio(glm::vec2 { 1.0f, 1.0f });

        TRACE_BLOCK("DebugDrawLines",
            if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::GameDebugDraw))
            {
                for (const auto& draw: mDebugDraws)
                {
                    if (const auto* ptr = std::get_if<engine::DebugDrawLine>(&draw))
                        gfx::DebugDrawLine(painter, ptr->a, ptr->b, ptr->color, ptr->width);
                    else if (const auto* ptr = std::get_if<engine::DebugDrawRect>(&draw))
                        gfx::DebugDrawRect(painter, gfx::FRect(ptr->top_left, ptr->bottom_right), ptr->color, ptr->width);
                    else if (const auto* ptr = std::get_if<engine::DebugDrawCircle>(&draw))
                        gfx::DebugDrawCircle(painter, gfx::FCircle(ptr->center, ptr->radius), ptr->color, ptr->width);
                    else BUG("Missing debug draw implementation");
                }
            }
        );

        TRACE_BLOCK("DebugDrawPhysics",
            if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::PhysicsBody))
            {
                if (mPhysics.HaveWorld())
                {
                    mPhysics.DebugDrawObjects(painter);
                }
            }
        );

        // this debug drawing is provided for the game developer to help them
        // see where the spatial nodes are, not for the engine developer to
        // debug the engine code. So this means that we assume that the engine
        // code is correct and the spatial index correctly reflects the nodes
        // and their positions. Thus the debug drawing can be based on the
        // entity/node iteration instead of iterating over the items in the
        // spatial index. (Which is a function that doesn't event exist yet).
        TRACE_BLOCK("DebugDrawScene",
            if (mScene)
            {
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
                            gfx::DebugDrawRect(painter, aabb, gfx::Color::Gray, 1.0f);
                        }
                    }
                    if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::BoundingRect))
                    {
                        const auto rect = mScene->FindEntityBoundingRect(&entity);
                        gfx::DrawRectOutline(painter, rect, gfx::Color::Yellow, 1.0f);
                    }
                    if (mDebug.debug_draw_flags.test(DebugOptions::DebugDraw::BoundingBox))
                    {
                        // todo: need to implement the minimum bounding box computation first
                    }
                }
            }
        );

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
    // The graphics device.
    std::shared_ptr<gfx::Device> mDevice;
    // The rendering subsystem.
    engine::Renderer mRenderer;
    // The physics subsystem.
    engine::PhysicsEngine mPhysics;
    // The UI subsystem
    engine::UIEngine mUIEngine;
    // The audio engine.
    std::unique_ptr<engine::AudioEngine> mAudio;
    // The game runtime that runs the actual game logic.
    std::unique_ptr<engine::GameRuntime> mRuntime;
    // Current game scene or nullptr if no scene.
    std::unique_ptr<game::Scene> mScene;
    // Current tilemap or nullptr if no map.
    std::unique_ptr<game::Tilemap> mTilemap;
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
    std::vector<engine::DebugDrawCmd> mDebugDraws;
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
    double mRenderTimeTotal = 0.0;
    double mRenderTimeStamp = 0.0;

    std::vector<base::TaskHandle> mUpdateTasks;

    // The bitbag for storing game state.
    engine::KeyValueStore mStateStore;
    // debug stepping flag to control taking a single update step.
    bool mStepForward = false;
    // renderer config set explicitly. Used to override
    // the normal renderer config when doing previews etc.
    std::optional<engine::Engine::RendererConfig> mRendererConfig;
};

} //namespace

extern "C" {
ENGINE_DLL_EXPORT engine::Engine* Gamestudio_CreateEngine()
{
#if defined(NDEBUG)
    DEBUG("DETONATOR 2D Engine in release build. *Kapow!*");
#else
    DEBUG("DETONATOR 2D Engine in DEBUG build. *pof*");
#endif
    return new GameStudioEngine;
}
} // extern C
