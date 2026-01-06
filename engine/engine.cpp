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
#include "audio/elements/graph_class.h"
#include "game/entity.h"
#include "game/entity_class.h"
#include "game/entity_node_drawable_item.h"
#include "game/treeop.h"
#include "game/tilemap.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/painter.h"
#include "graphics/paint_context.h"
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
#include "engine/lua_game_runtime.h"
#include "engine/cpp_game_runtime.h"
#include "engine/format.h"
#include "engine/library/library.h"
#include "engine/ui.h"
#include "engine/state.h"
#include "engine/frametimer.h"
#include "uikit/window.h"
#include "uikit/painter.h"
#include "uikit/state.h"

const unsigned char* GetEngineLogoData();
size_t GetEngineLogoDataSize();

namespace
{
// we'll do this dynamically in loading screen.
unsigned LogoWidth  = 0;
unsigned LogoHeight = 0;

struct UpdateState {
    double game_time  = 0.0;
    double game_step  = 0.0;
    double tick_accum = 0.0f;
    double tick_step  = 0.0f;
    engine::GameRuntime::Camera camera;
};

struct RenderState {
    double game_time = 0.0;
    double render_time_stamp = 0.0;
    double render_time_total = 0.0;
    unsigned surface_width  = 0.0f;
    unsigned surface_height = 0.0f;
    engine::GameRuntime::Camera camera;
    base::Color4f clear_color;
    bool enable_bloom = false;
    std::optional<engine::Engine::RendererConfig> render_config;
};

struct EngineRuntime {
    game::Scene* scene     = nullptr;
    game::Tilemap* tilemap = nullptr;

    engine::PhysicsEngine* physics = nullptr;
    engine::GameRuntime* lua_rt = nullptr;
    engine::GameRuntime* cpp_rt = nullptr;
    engine::Renderer* renderer = nullptr;
    engine::UIEngine* ui = nullptr;
};

// Default game engine implementation. Implements the main App interface
// which is the interface that enables the game host to communicate
// with the application/game implementation in order to update/tick/etc.
// the game and also to handle input from keyboard and mouse.
class DetonatorEngine final : public engine::Engine,
                              public engine::EventListener
{
public:
    DetonatorEngine() : mDebugPrints(10)
    {}

    ~DetonatorEngine() override
    {
        DEBUG("Engine deleted");
    }

    bool GetNextRequest(Request* out) override
    {
        return mRequests.GetNext(out);
    }

    void Init(const InitParams& init, const EngineConfig& conf) override
    {
        DEBUG("Engine initializing.");
        // LOCK ORDER MUST BE CONSISTENT. See Draw method.
        std::unique_lock<std::mutex> runtime_lock(mRuntimeLock);
        std::unique_lock<std::mutex> engine_lock(mEngineLock);

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

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime = std::make_unique<engine::LuaRuntime>("lua", init.game_script, mGameHome, init.application_name);
        mLuaRuntime->SetClassLibrary(mClasslib);
        mLuaRuntime->SetPhysicsEngine(&mPhysics);
        mLuaRuntime->SetStateStore(&mStateStore);
        mLuaRuntime->SetAudioEngine(mAudio.get());
        mLuaRuntime->SetDataLoader(mEngineDataLoader);
        mLuaRuntime->SetEditingMode(init.editing_mode);
        mLuaRuntime->SetPreviewMode(init.preview_mode);
        mLuaRuntime->Init();
        mLuaRuntime->SetSurfaceSize(init.surface_width, init.surface_height);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime = std::make_unique<engine::CppRuntime>();
        mCppRuntime->SetClassLibrary(mClasslib);
        mCppRuntime->SetPhysicsEngine(&mPhysics);
        mCppRuntime->SetStateStore(&mStateStore);
        mCppRuntime->SetAudioEngine(mAudio.get());
        mCppRuntime->SetDataLoader(mEngineDataLoader);
        mCppRuntime->SetEditingMode(init.editing_mode);
        mCppRuntime->SetPreviewMode(init.preview_mode);
        mCppRuntime->Init();
        mCppRuntime->SetSurfaceSize(init.surface_width, init.surface_height);
#endif

        mUIEngine.SetClassLibrary(mClasslib);
        mUIEngine.SetLoader(mEngineDataLoader);
        mUIEngine.SetSurfaceSize(float(init.surface_width), float(init.surface_height));
        mUIEngine.SetEditingMode(init.editing_mode);

        mRenderer.SetClassLibrary(mClasslib);
        mRenderer.SetEditingMode(init.editing_mode);
        mRenderer.SetName("Engine");

        mPhysics.SetClassLibrary(mClasslib);
        mPhysics.SetScale(conf.physics.scale);
        mPhysics.SetGravity(conf.physics.gravity);
        mPhysics.SetNumPositionIterations(conf.physics.num_position_iterations);
        mPhysics.SetNumVelocityIterations(conf.physics.num_velocity_iterations);
        mPhysics.SetTimestep(1.0f / float(conf.updates_per_second));

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
        bool preload_errors = false;
        bool preload_warnings = false;
    };
    std::unique_ptr<Engine::LoadingScreen> CreateLoadingScreen(const LoadingScreenSettings& settings) override
    {
        auto state = std::make_unique<LoadingScreen>();

        gfx::Image png((const void*)GetEngineLogoData(), GetEngineLogoDataSize());
        if (png.IsValid())
        {
            auto texture_source = std::make_unique<gfx::TextureBitmapBufferSource>();
            texture_source->SetBitmap(png.AsBitmap<gfx::Pixel_RGBA>());
            texture_source->SetColorSpace(gfx::TextureSource::ColorSpace::sRGB);
            texture_source->SetName("Detonator Logo");

            gfx::MaterialClass logo(gfx::MaterialClass::Type::Texture);
            logo.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            logo.SetTexture(std::move(texture_source));

            state->logo = gfx::CreateMaterialInstance(logo);
            LogoWidth  = png.GetWidth();
            LogoHeight = png.GetHeight();
        }
        state->font = settings.font_uri;
        return state;
    }

    void PreloadClass(const Engine::ContentClass& klass, size_t index, size_t last, Engine::LoadingScreen* screen) override
    {
        gfx::PaintContext pc;

        gfx::Painter dummy;
        dummy.SetEditingMode(mFlags.test(Flags::EditingMode));
        dummy.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
        dummy.SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
        dummy.SetPixelRatio(glm::vec2{1.0f, 1.0});
        dummy.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, float(mSurfaceWidth), float(mSurfaceHeight)));
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

            entity->InitClassGameRuntime();

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
        const auto surf_width  = float(mSurfaceWidth);
        const auto surf_height = float(mSurfaceHeight);

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
            device_viewport.Move((surf_width - device_viewport_width) * 0.5f,
                                 (surf_height - device_viewport_height) * 0.5f);
            device_viewport.Resize(int(device_viewport_width), int(device_viewport_height));

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
            painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, float(mSurfaceWidth), float(mSurfaceHeight)));
            painter.SetEditingMode(mFlags.test(Flags::EditingMode));

            painter.ClearColor(gfx::Color::Black);
            const auto& window = gfx::FRect(0.0f, 0.0f, float(mSurfaceWidth), float(mSurfaceHeight));

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

            const float done = float(index) / float(last);

            gfx::FillRect(painter, text_rect, gfx::Color::Black);
            gfx::DrawTextRect(painter,  base::FormatString("Loading ... %1%\n%2", int(done * 100.0f), klass.name),
                              my_screen->font, 22, text_rect,
                              gfx::Color::Silver,
                              gfx::TextAlign::AlignVCenter | gfx::TextAlign::AlignHCenter);

            gfx::FRect loader_rect_outline;
            loader_rect_outline.Resize(logo_rect.GetWidth(), 20.0f);
            loader_rect_outline.Translate(logo_rect.GetPosition());
            loader_rect_outline.Translate(0.0f, logo_rect.GetHeight());
            loader_rect_outline.Translate(0.0f, text_rect.GetHeight());
            loader_rect_outline.Translate(0.0f, 10.0f);
            gfx::DrawRectOutline(painter, loader_rect_outline, gfx::Color::Silver);

            gfx::FRect loader_rect_fill;
            loader_rect_fill.Resize((logo_rect.GetWidth()-4.0f) * done, 20.0f-4.0f);
            loader_rect_fill.Translate(logo_rect.GetPosition());
            loader_rect_fill.Translate(0.0f, logo_rect.GetHeight());
            loader_rect_fill.Translate(0.0f, text_rect.GetHeight());
            loader_rect_fill.Translate(0.0f, 10.0f);
            loader_rect_fill.Translate(2.0f, 2.0f);
            gfx::FillRect(painter, loader_rect_fill, gfx::Color::Silver);
        }
        // todo: use this informatino somewhere
        if (pc.HasErrors())
            my_screen->preload_errors = true;
        if (pc.HasWarnings())
            my_screen->preload_warnings = true;

        // this is for debugging so we can see what happens...
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));

        mDevice->EndFrame(true);
    }

    void NotifyClassUpdate(const ContentClass& klass) override
    {
        DEBUG("Content class was updated. [type=%1, name='%2', id='%2]", klass.type, klass.name, klass.id);

        std::lock_guard<std::mutex> lock(mRuntimeLock);

        engine::GameRuntime::ContentClass k;
        k.type = klass.type;
        k.name = klass.name;
        k.id   = klass.id;
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->OnContentClassUpdate(k);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->OnContentClassUpdate(k);
#endif
    }
    void SetRendererConfig(const RendererConfig& config) override
    {
        std::lock_guard<std::mutex> lock(mEngineLock);
        mRendererConfig = config;
    }

    bool Load() override
    {
        DEBUG("Loading game state.");
        ASSERT(mUpdateTasks.empty());

        std::lock_guard<std::mutex> lock(mRuntimeLock);
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->LoadGame();
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->LoadGame();
#endif
        return true;
    }

    void Start() override
    {
        DEBUG("Starting game play.");
        ASSERT(mUpdateTasks.empty());

        std::lock_guard<std::mutex> lock(mRuntimeLock);
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->StartGame();
#endif
        
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->StartGame();
#endif
    }
    void SetDebugOptions(const DebugOptions& debug) override
    {
        {
            std::unique_lock<std::mutex> lock(mEngineLock);

            mDebug = debug;

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

        if (mAudio)
        {
            std::lock_guard<std::mutex> lock(mRuntimeLock);
            mAudio->SetDebugPause(debug.debug_pause);
        }
    }
    void DebugPrintString(const std::string& message) override
    {
        std::unique_lock<std::mutex> lock(mEngineLock);

        DebugPrint print;
        print.message = message;
        mDebugPrints.push_back(std::move(print));
    }
    void SetTracer(base::Trace* tracer, base::TraceWriter* writer) override
    {
        if (mAudio)
        {
            std::lock_guard<std::mutex> lock(mRuntimeLock);
            mAudio->SetAudioThreadTraceWriter(writer);
        }
    }
    void SetTracingOn(bool on_off) override
    {
        if (mAudio)
        {
            std::lock_guard<std::mutex> lock(mRuntimeLock);
            mAudio->EnableAudioThreadTrace(on_off);
        }
    }

    void SetEnvironment(const Environment& env) override
    {
        std::lock_guard<std::mutex> lock(mEngineLock);

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

    void Draw() override
    {
        const auto dt = mFrameTimer.GetAverage();

        mDevice->BeginFrame();
        mDevice->ClearColor(mClearColor);
        mDevice->ClearDepth(1.0f);

        gfx::PaintContext pc;

        // for the time being, if we have no debug font set then turn off the
        // paint context, which causes paint errors to go to the normal log.
        // todo: should probably embed a small font in the engine itself.
        if (mDebug.debug_font.empty())
            pc.EndScope();

        // Do the main drawing here based on previously generated
        // draw packets that are stored in the renderer.
        // This method can run in parallel with the calls to update
        // the renderer state. The thread safety is built into the
        // renderer class itself.
        TRACE_CALL("Renderer::DrawFrame", mRenderer.DrawFrame(*mDevice));

        // Wait the completion of update tasks that we might have
        // kicked off in the update step. The update accesses
        // the same data so we can't run parallel.
        const bool done_updates = WaitTasks();

        // take the locks to make Valgrind happy.
        // LOCK ORDER MUST BE CONSISTENT. See Init.
        std::lock_guard<std::mutex> runtime_lock(mRuntimeLock);
        std::lock_guard<std::mutex> engine_lock(mEngineLock);


#if defined(ENGINE_USE_UPDATE_THREAD)
        if (done_updates)
#endif
        {
            RenderState state;
            state.camera            = mCamera;
            state.game_time         = mGameTimeTotal;
            state.render_time_stamp = mRenderTimeStamp;
            state.render_time_total = mRenderTimeTotal;
            state.surface_width     = mSurfaceWidth;
            state.surface_height    = mSurfaceHeight;
            state.clear_color       = mClearColor;
            state.enable_bloom      = mFlags.test(DetonatorEngine::Flags::EnableBloom);
            state.render_config     = mRendererConfig;

            EngineRuntime runtime;
            runtime.scene    = mScene.get();
            runtime.tilemap  = mTilemap.get();
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
            runtime.lua_rt = mLuaRuntime.get();
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
            runtime.cpp_rt = mCppRuntime.get();
#endif
            runtime.physics  = &mPhysics;
            runtime.renderer = &mRenderer;
            runtime.ui       = &mUIEngine;
            TRACE_CALL("CreateNextFrame", CreateNextFrame(state, runtime));

            mRenderTimeStamp = state.render_time_stamp;
            mRenderTimeTotal = state.render_time_total;

        } // wait tasks

        TRACE_CALL("Engine::DrawGameUI",        DrawGameUI());
        TRACE_CALL("Engine::DrawDebugObjects",  DrawDebugObjects());
        TRACE_CALL("Engine::DrawDebugMessages", DrawDebugMessages());
        TRACE_CALL("Engine::DrawMousePointer",  DrawMousePointer(dt));
        TRACE_CALL("Engine::DrawPaintMessages", DrawPaintMessages(pc));
        TRACE_CALL("Device::EndFrame", mDevice->EndFrame(true));
    }

    void BeginMainLoop() override
    {
        std::lock_guard<std::mutex> lock(mRuntimeLock);

        ++mFrameCounter;

        engine::GameRuntime* cpp_runtime = nullptr;
        engine::GameRuntime* lua_runtime = nullptr;
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        lua_runtime = mLuaRuntime.get();
        lua_runtime->SetFrameNumber(mFrameCounter);
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        cpp_runtime = mCppRuntime.get();
        cpp_runtime->SetFrameNumber(mFrameCounter);
#endif

        // service the audio system once. We have to do this
        // in the main thread (same as drawing) because of
        // the limitations of the web build.
        TRACE_BLOCK("AudioEngineUpdate",
            std::vector<engine::AudioEvent> audio_events;
            mAudio->Update(&audio_events);
            for (const auto& event : audio_events)
            {
                if (lua_runtime)
                    lua_runtime->OnAudioEvent(event);
                if (cpp_runtime)
                    cpp_runtime->OnAudioEvent(event);
            }
        );
    }

    void Step() override
    {
        mStepForward = true;
    }

    void Update(float dt) override
    {
        // Game play update. NOT the place for any kind of
        // real time/wall time subsystem (such as audio) service
        if (mDebug.debug_pause && !mStepForward)
            return;

        mFrameTimer.AddSample(dt);

        dt = mFrameTimer.GetAverage();

        // there's plenty of information about different ways to write a basic
        // game rendering loop. here are some suggested references:
        // https://gafferongames.com/post/fix_your_timestep/
        // Game Engine Architecture by Jason Gregory
        mTimeAccum += dt;

#if defined(ENGINE_USE_UPDATE_THREAD)
        auto* thread_pool = base::GetGlobalThreadPool();

        class UpdateTask : public base::ThreadTask {
        public:
            explicit UpdateTask(DetonatorEngine* engine) noexcept
              : mEngine(engine)
            {}
        protected:
            void DoTask() override
            {
                UpdateState state;
                {
                    std::lock_guard<std::mutex> lock(mEngine->mEngineLock);
                    state.tick_accum = mEngine->mTickAccum;
                    state.tick_step  = mEngine->mGameTickStep;
                    state.game_time  = mEngine->mGameTimeTotal;
                    state.game_step  = mEngine->mGameTimeStep;
                }

                {
                    std::lock_guard<std::mutex> lock(mEngine->mRuntimeLock);
                    EngineRuntime runtime;
                    runtime.scene    = mEngine->mScene.get();
                    runtime.tilemap  = mEngine->mTilemap.get();
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
                    runtime.lua_rt   = mEngine->mLuaRuntime.get();
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
                    runtime.cpp_rt   = mEngine->mCppRuntime.get();
#endif
                    runtime.physics  = &mEngine->mPhysics;
                    runtime.renderer = &mEngine->mRenderer;
                    runtime.ui       = &mEngine->mUIEngine;
                    TRACE_CALL("UpdateGame", mEngine->UpdateGame(state, runtime));
                }

                {
                    std::lock_guard<std::mutex> lock(mEngine->mEngineLock);
                    mEngine->mTickAccum     = state.tick_accum;
                    mEngine->mGameTickStep  = state.tick_step;
                    mEngine->mGameTimeTotal = state.game_time;
                    mEngine->mGameTimeStep  = state.game_step;
                    mEngine->mCamera        = state.camera;
                }
            }
        private:
            DetonatorEngine* mEngine = nullptr;
        };

        class UpdateDebugDrawTask : public base::ThreadTask {
        public:
            explicit UpdateDebugDrawTask(DetonatorEngine* engine) noexcept
              : mEngine(engine)
            {}
        protected:
            void DoTask() override
            {
                // update the debug draws only after updating the game
                // if this is done per each frame they will not be seen
                // by the user if the rendering is running very fast.
                std::vector<engine::DebugDrawCmd> debug_draws;
                {
                    std::lock_guard<std::mutex> lock(mEngine->mRuntimeLock);
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
                    std::vector<engine::DebugDrawCmd> lua_debug_draws;
                    mEngine->mLuaRuntime->TransferDebugQueue(&lua_debug_draws);
                    base::AppendVector(debug_draws, lua_debug_draws);
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
                    std::vector<engine::DebugDrawCmd> cpp_debug_draws;
                    mEngine->mCppRuntime->TransferDebugQueue(&cpp_debug_draws);
                    base::AppendVector(debug_draws, cpp_debug_draws);
#endif

                }

                {
                    std::lock_guard<std::mutex> lock(mEngine->mEngineLock);
                    std::swap(mEngine->mDebugDraws, debug_draws);
                }
            }
        private:
            DetonatorEngine* mEngine = nullptr;
        };
#endif
        bool did_update = false;

        // do simulation/animation update steps.
        while (mTimeAccum >= mGameTimeStep)
        {
            // Call UpdateGame with the *current* time. I.e. the game
            // is advancing one time step from current mGameTimeTotal.
            // this is consistent with the tick time accumulation below.
#if defined(ENGINE_USE_UPDATE_THREAD)
            auto task = std::make_unique<UpdateTask>(this);
            task->SetTaskName("UpdateTask");
            mUpdateTasks.push_back(thread_pool->SubmitTask(std::move(task), base::ThreadPool::UpdateThreadID));
#else
            UpdateState state;
            state.tick_accum = mTickAccum;
            state.tick_step  = mGameTickStep;
            state.game_time  = mGameTimeTotal;
            state.game_step  = mGameTimeStep;

            EngineRuntime runtime;
            runtime.scene    = mScene.get();
            runtime.tilemap  = mTilemap.get();
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
            runtime.glua_rt  = mLuaRuntime.get();
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
            runtime.cpp_rt   = mCppRuntime.get();
#endif
            runtime.physics  = &mPhysics;
            runtime.renderer = &mRenderer;
            runtime.ui       = &mUIEngine;
            TRACE_CALL("UpdateGame", UpdateGame(state, runtime));

            mTickAccum     = state.tick_accum;
            mGameTickStep  = state.tick_step;
            mGameTimeTotal = state.game_time;
            mGameTimeStep  = state.game_step;
            mCamera        = state.camera;

#endif
            mGameTimeTotal += mGameTimeStep;
            mTimeAccum -= mGameTimeStep;

            // if we're paused for debugging stop after one step forward.
            mStepForward = false;

            did_update = true;
        }

        if (did_update)
        {
#if defined(ENGINE_USE_UPDATE_THREAD)
            auto task = std::make_unique<UpdateDebugDrawTask>(this);
            task->SetTaskName("UpdateDebugDraws");
            mUpdateTasks.push_back(thread_pool->SubmitTask(std::move(task), base::ThreadPool::UpdateThreadID));
#else
            // update the debug draws only after updating the game
            // if this is done per each frame they will not be seen
            // by the user if the rendering is running very fast.
            std::vector<engine::DebugDrawCmd> debug_draws;
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
            std::vector<engine::DebugDrawCmd> lua_debug_draws;
            mLuaRuntime->TransferDebugQueue(&lua_debug_draws);
            base::AppendVector(debug_draws, lua_debug_draws);
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
            std::vector<engine::DebugDrawCmd> cpp_debug_draws;
            mCppRuntime->TransferDebugQueue(&cpp_debug_draws);
            base::AppendVector(debug_draws, cpp_debug_draws);
#endif

            std::swap(mDebugDraws, debug_draws);
#endif
        }

    }
    void EndMainLoop() override
    {
        const auto dt = mFrameTimer.GetAverage();

        // take the locks to make Valgrind happy.
        // LOCK ORDER MUST BE CONSISTENT. See Init method.
        std::lock_guard<std::mutex> runtime_lock(mRuntimeLock);
        std::lock_guard<std::mutex> engine_lock(mEngineLock);

        // Note that we *don't* call CleanGarbage here since currently there should
        // be nothing that is creating needless GPU resources.
        if (mDebug.debug_pause && !mStepForward)
            return;

        // todo: the action processing probably needs to be split
        // into game-actions and non-game actions. For example the
        // game might insert an additional delay in order to transition
        // from one game state to another but likely want to transition
        // to full screen mode in real time. (non game action)
        mActionDelay = math::clamp(0.0f, mActionDelay, mActionDelay - dt);
        if (mActionDelay > 0.0f)
            return;

        engine::GameRuntime* lua_runtime = nullptr;
        engine::GameRuntime* cpp_runtime = nullptr;
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        lua_runtime = mLuaRuntime.get();
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        cpp_runtime = mCppRuntime.get();
#endif
        engine::Action action;
        while ((lua_runtime && lua_runtime->GetNextAction(&action)) ||
               (cpp_runtime && cpp_runtime->GetNextAction(&action)))
        {
            std::visit([this](auto& variant_value) {
                this->OnAction(variant_value);
            }, action);

            if (mActionDelay > 0.0f)
                break;
        }
    }

    void Stop() override
    {
        DEBUG("Stop game");
        WaitTasks();

        std::lock_guard<std::mutex> lock(mRuntimeLock);

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->StopGame();
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->StopGame();
#endif
    }

    void Save() override
    {
        DEBUG("Save game");
        WaitTasks();

        std::lock_guard<std::mutex> lock(mRuntimeLock);
        ASSERT(mUpdateTasks.empty());

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->SaveGame();
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->SaveGame();
#endif
    }

    void Shutdown() override
    {
        DEBUG("Engine shutting down.");
        std::unique_lock<std::mutex> runtime_lock(mRuntimeLock);
        std::unique_lock<std::mutex> engine_lock(mEngineLock);

        ASSERT(mUpdateTasks.empty());

        mPhysics.SetClassLibrary(nullptr);

        mRenderer.SetClassLibrary(nullptr);

        mUIEngine.SetClassLibrary(nullptr);
        mUIEngine.SetLoader(nullptr);

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->SetClassLibrary(nullptr);
        mLuaRuntime->SetPhysicsEngine(nullptr);
        mLuaRuntime->SetStateStore(nullptr);
        mLuaRuntime->SetAudioEngine(nullptr);
        mLuaRuntime->SetDataLoader(nullptr);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->SetClassLibrary(nullptr);
        mCppRuntime->SetPhysicsEngine(nullptr);
        mCppRuntime->SetStateStore(nullptr);
        mCppRuntime->SetAudioEngine(nullptr);
        mCppRuntime->SetDataLoader(nullptr);
#endif

        mAudio->SetClassLibrary(nullptr);
        mAudio.reset();

        gfx::SetResourceLoader(nullptr);
        mDevice.reset();

        audio::ClearCaches();
    }
    bool IsRunning() const override
    {
        return mFlags.test(Flags::Running);
    }
    wdk::WindowListener* GetWindowListener() override
    {
        return this;
    }

    void SetHostStats(const HostStats& stats) override
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
    bool GetStats(Stats* stats) const override
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
    void TakeScreenshot(const std::string& filename) const override
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
    void ReloadResources(unsigned bits) override
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

    void OnRenderingSurfaceResized(unsigned width, unsigned height) override
    {
        std::lock_guard<std::mutex> lock(mEngineLock);

        // ignore accidental superfluous notifications.
        if (width == mSurfaceWidth && height == mSurfaceHeight)
            return;

        DEBUG("Rendering surface resized. [width=%1, height=%2]", width, height);
        mSurfaceWidth = width;
        mSurfaceHeight = height;
    }
    void OnEnterFullScreen() override
    {
        DEBUG("Enter full screen mode.");
        mFlags.set(Flags::Fullscreen, true);
    }
    void OnLeaveFullScreen() override
    {
        DEBUG("Leave full screen mode.");
        mFlags.set(Flags::Fullscreen, false);
    }

    // WindowListener
    void OnWantClose(const wdk::WindowEventWantClose&) override
    {
        // todo: handle ending play, saving game etc.
        mFlags.set(Flags::Running, false);
    }
    void OnKeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (mFlags.test(Flags::BlockKeyboard))
            return;

        std::lock_guard<std::mutex> lock(mRuntimeLock);

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnKeyDown(key, &actions);

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->OnUIAction(mUIEngine.GetUI(), actions);
        mLuaRuntime->OnKeyDown(key);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->OnUIAction(mUIEngine.GetUI(), actions);
        mCppRuntime->OnKeyDown(key);
#endif
    }
    void OnKeyUp(const wdk::WindowEventKeyUp& key) override
    {       
        if (mFlags.test(Flags::BlockKeyboard))
            return;

        std::lock_guard<std::mutex> lock(mRuntimeLock);

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnKeyUp(key, &actions);

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->OnUIAction(mUIEngine.GetUI(), actions);
        mLuaRuntime->OnKeyUp(key);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->OnUIAction(mUIEngine.GetUI(), actions);
        mCppRuntime->OnKeyUp(key);
#endif
    }
    void OnChar(const wdk::WindowEventChar& text) override
    {
        if (mFlags.test(Flags::BlockKeyboard))
            return;

        std::lock_guard<std::mutex> lock(mRuntimeLock);

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->OnChar(text);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->OnChar(text);
#endif
    }
    void OnMouseMove(const wdk::WindowEventMouseMove& mouse) override
    {
        if (mFlags.test(Flags::BlockMouse))
            return;

        std::lock_guard<std::mutex> lock(mRuntimeLock);

        mCursorPos.x = float(mouse.window_x);
        mCursorPos.y = float(mouse.window_y);

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnMouseMove(mouse, &actions);

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->OnUIAction(mUIEngine.GetUI(), actions);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->OnUIAction(mUIEngine.GetUI(), actions);
#endif

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMouseMove);
    }
    void OnMousePress(const wdk::WindowEventMousePress& mouse) override
    {
        if (mFlags.test(Flags::BlockMouse))
            return;

        std::lock_guard<std::mutex> lock(mRuntimeLock);

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnMousePress(mouse, &actions);

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->OnUIAction(mUIEngine.GetUI(), actions);
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->OnUIAction(mUIEngine.GetUI(), actions);
#endif

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMousePress);
    }
    void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse) override
    {
        if (mFlags.test(Flags::BlockMouse))
            return;

        std::lock_guard<std::mutex> lock(mRuntimeLock);

        std::vector<engine::UIEngine::WidgetAction> actions;
        mUIEngine.OnMouseRelease(mouse, &actions);

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->OnUIAction(mUIEngine.GetUI(), actions);
#endif
#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->OnUIAction(mUIEngine.GetUI(), actions);
#endif

        const auto& mickey = MapGameMouseEvent(mouse);
        SendGameMouseEvent(mickey, &engine::GameRuntime::OnMouseRelease);
    }
private:
    engine::GameRuntime::Camera GetCamera() const
    {
        return mCamera;
    }

    engine::IRect GetViewport(const engine::GameRuntime::Camera& camera,
                              unsigned surface_width, unsigned surface_height) const

    {
        // get the game's logical viewport into the game world.
        const auto& view = camera.viewport;
        // map the logical viewport to some area in the rendering surface
        // so that the rendering area (the device viewport) has the same
        // aspect ratio as the logical viewport.
        const float width       = view.GetWidth();
        const float height      = view.GetHeight();
        const auto surf_width  = (float)surface_width;
        const auto surf_height = (float)surface_height;
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
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        auto* lua_rt = mLuaRuntime.get();
        (lua_rt->*which)(mickey);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        auto* cpp_rt = mCppRuntime.get();
        (cpp_rt->*which)(mickey);
#endif
    }

    template<typename WdkMouseEvent>
    engine::MouseEvent MapGameMouseEvent(const WdkMouseEvent& mickey) const
    {
        const auto& camera = GetCamera();
        const auto& device_viewport  = GetViewport(camera, mSurfaceWidth, mSurfaceHeight);

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
                    event.map_coord = engine::MapFromViewPlaneToGamePlane(
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

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        TRACE_CALL("LuaRuntime::BeginPlay", mLuaRuntime->BeginPlay(mScene.get(), mTilemap.get()));
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        TRACE_CALL("CppRuntime::BeginPlay", mCppRuntime->BeginPlay(mScene.get(), mTilemap.get()));
#endif
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

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->EndPlay(mScene.get(), mTilemap.get());
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->EndPlay(mScene.get(), mTilemap.get());
#endif
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
#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
        mLuaRuntime->OnGameEvent(action.event);
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
        mCppRuntime->OnGameEvent(action.event);
#endif
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

    void UpdateGame(UpdateState& state, EngineRuntime& runtime) const
    {
        const auto game_time = state.game_time;
        const auto dt = state.game_step;

        if (runtime.scene)
        {
            TRACE_CALL("Scene::BeginLoop", runtime.scene->BeginLoop());

            if (runtime.lua_rt)
                TRACE_CALL("LuaRuntime::BeginLoop", runtime.lua_rt->BeginLoop());
            if (runtime.cpp_rt)
                TRACE_CALL("CppRuntime::BeginLoop", runtime.cpp_rt->BeginLoop());

            // do a component wise runtime update.
            game::EntityClass::UpdateRuntimes(game_time, dt);

            std::vector<game::Scene::Event> events;
            TRACE_CALL("Scene::Update", runtime.scene->Update(dt, &events));

            TRACE_CALL("HandleSceneEvents",HandleSceneEvents(events));

            if (runtime.lua_rt)
                TRACE_CALL("LuaRuntime::OnSceneEvent", runtime.lua_rt->OnSceneEvent(events));
            if (runtime.cpp_rt)
                TRACE_CALL("CppRuntime::OnSceneEvent", runtime.cpp_rt->OnSceneEvent(events));

            if (runtime.physics->HaveWorld())
            {
                std::vector<engine::ContactEvent> contacts;
                // Apply any pending changes such as velocity updates,
                // rigid body flag state changes, new rigid bodies etc.
                // changes to the physics world.
                TRACE_CALL("Physics::UpdateWorld", runtime.physics->UpdateWorld(*mScene));
                // Step the simulation forward.
                TRACE_CALL("Physics::Step", runtime.physics->Step(&contacts));
                // Update the result of the physics simulation from the
                // physics world to the scene and its entities.
                TRACE_CALL("Physics::UpdateScene", runtime.physics->UpdateScene(*mScene));
                // dispatch the contact events (if any).
                if (runtime.lua_rt)
                    TRACE_CALL("LuaRuntime::OnContactEvents", runtime.lua_rt->OnContactEvent(contacts));
                if (runtime.cpp_rt)
                    TRACE_CALL("CppRuntime::OnContactEvents", runtime.cpp_rt->OnContactEvent(contacts));
            }
        }

        if (runtime.lua_rt)
            TRACE_CALL("LuaRuntime::Update", runtime.lua_rt->Update(game_time, dt));
        if (runtime.cpp_rt)
            TRACE_CALL("CppRuntime:Update", runtime.cpp_rt->Update(game_time, dt));

        // Tick game
        {
            state.tick_accum += dt;
            // current game time from which we step forward
            // in ticks later on.
            auto tick_time = game_time;

            while (state.tick_accum >= state.tick_step)
            {
                if (runtime.lua_rt)
                    TRACE_CALL("LuaRuntime::Tick", runtime.lua_rt->Tick(tick_time, mGameTickStep));
                if (runtime.cpp_rt)
                    TRACE_CALL("CppRuntime:Tick", runtime.cpp_rt->Tick(tick_time, mGameTickStep));
                state.tick_accum -= state.tick_step;
                tick_time += state.tick_step;
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
        if (runtime.scene)
        {
            // Update renderers data structures from the scene.
            // This involves creating new render nodes for new entities
            // that have been spawned etc. This needs to be done inside
            // the begin/end loop in order to have the correct signalling
            // i.e. entity control flags.
            TRACE_CALL("Renderer::UpdateState", runtime.renderer->UpdateRendererState(*mScene, mTilemap.get()));

            // make sure to do this first in order to allow the scene to rebuild
            // the spatial indices etc. before the game's PostUpdate runs.
            TRACE_CALL("Scene::Rebuild", runtime.scene->Rebuild());
            // using the time we've arrived to now after having taken the previous
            // delta step forward in game time.
            if (runtime.lua_rt)
                TRACE_CALL("LuaRuntime::PostUpdate", runtime.lua_rt->PostUpdate(game_time + dt));
            if (runtime.cpp_rt)
                TRACE_CALL("CppRuntime::PostUpdate", runtime.cpp_rt->PostUpdate(game_time + dt));

            if (runtime.lua_rt)
                TRACE_CALL("LuaRuntime::EndLoop", runtime.lua_rt->EndLoop());
            if (runtime.cpp_rt)
                TRACE_CALL("CppRuntime::EndLoop", runtime.cpp_rt->EndLoop());

            TRACE_CALL("Scene::EndLoop", runtime.scene->EndLoop());
        }

        // this looks like a competition, but basically it's up to the game
        // to make the right thing in terms of controlling the camera
        // properly.
        if (runtime.lua_rt)
            runtime.lua_rt->GetCamera(&state.camera);
        if (runtime.cpp_rt)
            runtime.cpp_rt->GetCamera(&state.camera);

        std::vector<engine::UIEngine::WidgetAction> widget_actions;
        std::vector<engine::UIEngine::WindowAction> window_actions;
        TRACE_CALL("UI::UpdateWindow", runtime.ui->UpdateWindow(game_time, dt, &widget_actions));
        TRACE_CALL("UI::UpdateState", runtime.ui->UpdateState(game_time, dt, &window_actions));

        TRACE_BLOCK("LuaRuntime::UpdateUI",
            if (auto* ui = runtime.ui->GetUI())
            {
                if (runtime.lua_rt)
                {
                    runtime.lua_rt->OnUIAction(ui, widget_actions);
                    runtime.lua_rt->UpdateUI(ui, game_time, dt);
                }
            }
        );

        TRACE_BLOCK("CppRuntime::UpdateUI",
            if (auto* ui = runtime.ui->GetUI())
            {
                if (runtime.cpp_rt)
                {
                    runtime.cpp_rt->OnUIAction(ui, widget_actions);
                    runtime.cpp_rt->UpdateUI(ui, game_time, dt);
                }
            }
        );

        TRACE_BLOCK("LuaRuntime::HandleUI",
            if (runtime.lua_rt)
            {
                for (const auto& w : window_actions)
                {
                    if (const auto* ptr = std::get_if<engine::UIEngine::WindowOpen>(&w))
                        runtime.lua_rt->OnUIOpen(ptr->window);
                    else if (const auto* ptr = std::get_if<engine::UIEngine::WindowUpdate>(&w))
                        runtime.lua_rt->SetCurrentUI(ptr->window);
                    else if(const auto* ptr = std::get_if<engine::UIEngine::WindowClose>(&w))
                        runtime.lua_rt->OnUIClose(ptr->window.get(), ptr->result);
                    else BUG("Missing UIEngine window event handling.");
                }
            }
        );

        TRACE_BLOCK("CppRuntime::HandleUI",
            if (runtime.cpp_rt)
            {
                for (const auto& w : window_actions)
                {
                    if (const auto* ptr = std::get_if<engine::UIEngine::WindowOpen>(&w))
                        runtime.cpp_rt->OnUIOpen(ptr->window);
                    else if (const auto* ptr = std::get_if<engine::UIEngine::WindowUpdate>(&w))
                        runtime.cpp_rt->SetCurrentUI(ptr->window);
                    else if(const auto* ptr = std::get_if<engine::UIEngine::WindowClose>(&w))
                        runtime.cpp_rt->OnUIClose(ptr->window.get(), ptr->result);
                    else BUG("Missing UIEngine window event handling.");
                }
            }
        );
    }

    bool HandleAnimationAudioTriggerEvent(const game::Scene::Event& event) const
    {
        // todo: the nesting of events is a bit complicated here.. maybe flatten
        // them into a simpler structure?

        const auto* ea_ptr = std::get_if<game::Scene::EntityAnimationEvent>(&event);
        if (!ea_ptr)
            return false;

        const auto* ptr = std::get_if<game::AnimationAudioTriggerEvent>(&ea_ptr->event.event);
        if (!ptr)
            return false;

        if (ptr->action == game::AnimationAudioTriggerEvent::StreamAction::Play)
        {
            const auto& audio_graph = mClasslib->FindAudioGraphClassById(ptr->audio_graph_id);
            if (!audio_graph)
            {
                WARN("Failed to trigger audio on animation event. No such audio graph was found."
                    "[entity='%1', animation='%2', trigger='%3']", ea_ptr->entity->GetName(),
                        ea_ptr->event.animation_name, ptr->trigger_name);
                return true;
            }
            if (ptr->stream == game::AnimationAudioTriggerEvent::AudioStream::Effect)
                mAudio->PlaySoundEffect(audio_graph);
            else if (ptr->stream == game::AnimationAudioTriggerEvent::AudioStream::Music)
                mAudio->PlayMusic(audio_graph);
        } else BUG("Unhandled audio action trigger.");

        return true;
    }

    bool HandleAnimationSpawnEntityTrigger(const game::Scene::Event& event) const
    {
        const auto* ea_ptr = std::get_if<game::Scene::EntityAnimationEvent>(&event);
        if (!ea_ptr)
            return false;

        const auto* ptr = std::get_if<game::AnimationSpawnEntityTriggerEvent>(&ea_ptr->event.event);
        if (!ptr)
            return false;

        const auto& entity_class = mClasslib->FindEntityClassById(ptr->entity_class_id);
        if (!entity_class)
        {
            WARN("Failed to trigger entity spawn on animation event. No such entity class was found. "
                "[entity='%1', animation='%2', trigger='%3']", ea_ptr->entity->GetName(),
                ea_ptr->event.animation_name, ptr->trigger_name);
            return true;
        }
        const auto* entity = ea_ptr->entity;
        const auto* entity_node = entity->FindNodeByInstanceId(ptr->source_node_id);
        const auto& spawn_world_pos = mScene->MapPointFromEntityNode(entity, entity_node, {0.0f, 0.0f});

        game::EntityArgs spawn_args;
        spawn_args.async_spawn = true;
        spawn_args.klass = entity_class;
        spawn_args.render_layer = ptr->render_layer;
        spawn_args.position = spawn_world_pos;
        mScene->SpawnEntity(spawn_args, true);
        return true;
    }

    void HandleSceneEvents(const std::vector<game::Scene::Event>& events) const
    {
        for (const auto& event : events)
        {
            if (HandleAnimationAudioTriggerEvent(event) ||
                HandleAnimationSpawnEntityTrigger(event))
                continue;
        }
    }

    void CreateNextFrame(RenderState& state, EngineRuntime& runtime) const
    {
        const auto now = state.game_time;
        if (state.render_time_stamp == 0.0)
            state.render_time_stamp = now;

        const auto current_render_delta = now - state.render_time_stamp;
        const auto current_render_time = state.render_time_total;

        state.render_time_total += current_render_delta;
        state.render_time_stamp = now;

        if (!runtime.scene)
            return;

        const auto surf_width   = float(state.surface_width);
        const auto surf_height  = float(state.surface_height);
        const auto& game_camera = state.camera;

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
            return;

        engine::Renderer::FrameSettings settings;
        settings.surface.viewport   = GetViewport(state.camera, state.surface_width, state.surface_height);
        settings.surface.size       = base::USize(state.surface_width, state.surface_height);
        settings.camera.clear_color = state.clear_color;
        settings.camera.viewport    = game_view;
        settings.camera.scale       = game_camera.scale;
        settings.camera.position    = game_camera.position;
        settings.camera.rotation    = 0.0f;
        settings.camera.ppa         = engine::ComputePerspectiveProjection(game_view);

        if (auto* bloom = runtime.scene->GetBloom(); bloom && state.enable_bloom)
        {
            settings.bloom.threshold = bloom->threshold;
            settings.bloom.red       = bloom->red;
            settings.bloom.green     = bloom->green;
            settings.bloom.blue      = bloom->blue;
            settings.effects.set(engine::Renderer::Effects::Bloom, true);
        }
        if (auto* fog = runtime.scene->GetFog())
        {
            settings.fog = *fog;
            settings.enable_fog = true;
        }

        const auto shading = (*runtime.scene)->GetShadingMode();
        if (shading == game::SceneClass::SceneShadingMode::Flat)
            settings.style = engine::Renderer::RenderingStyle::FlatColor;
        else if (shading == game::SceneClass::SceneShadingMode::BasicLight)
            settings.style = engine::Renderer::RenderingStyle::BasicShading;
        else BUG("Bug on renderer shading mode.");

        if (state.render_config.has_value())
        {
            const auto& value = state.render_config.value();
            settings.style = value.style;
        }

        TRACE_CALL("Renderer::Update", runtime.renderer->Update(*runtime.scene, runtime.tilemap,
            current_render_time, current_render_delta));

        TRACE_CALL("Renderer::CreateFrame", runtime.renderer->CreateFrame(*runtime.scene, runtime.tilemap, settings));
    }

    bool WaitTasks()
    {
        if (mUpdateTasks.empty())
            return false;
        // if we had updates running in parallel then complete (wait)
        // the tasks here. This is unfortunately needed in order to
        // make sure that the update thread is no longer touching
        // the UI system or the scene.
        // Wait each update task
        for (auto& handle: mUpdateTasks)
        {
            TRACE_CALL("WaitSceneUpdate", handle.Wait(base::TaskHandle::WaitStrategy::BusyLoop));
            const auto* task = handle.GetTask();
            if (task->HasException())
            {
                ERROR("Task has encountered an exception. [task='%1']", task->GetTaskName());
                // should we rethrow this? Yes the answer is yes.
                // Exceptions such as Lua LuaErrors mean the game code is all bonkers trying to
                // access a nil variable for example.
                task->RethrowException();

            }
        }
        mUpdateTasks.clear();
        return true;
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
        const auto& game_camera = GetCamera();
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

    void DrawPaintMessages(gfx::PaintContext& pc) const
    {
        if (mDebug.debug_font.empty())
            return;

        gfx::PaintContext::MessageList msgs;
        pc.TransferMessages(&msgs);
        if (msgs.empty())
            return;

        const auto surf_width  = static_cast<float>(mSurfaceWidth);
        const auto surf_height = static_cast<float>(mSurfaceHeight);

        // this painter will be configured to draw directly in the window
        // coordinates. Used to draw debug text and the mouse cursor.
        gfx::Painter painter;
        painter.SetDevice(mDevice);
        painter.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
        painter.SetPixelRatio(glm::vec2(1.0f, 1.0f));
        painter.SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
        painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, surf_width, surf_height));
        painter.SetEditingMode(mFlags.test(Flags::EditingMode));

        gfx::FRect rect(10, 30, 500, 20);
        for (const auto& msg : msgs)
        {
            gfx::Color color = gfx::Color::HotPink;
            if (msg.type == gfx::PaintContext::LogEvent::Error)
                color = gfx::Color::Red;
            else if (msg.type == gfx::PaintContext::LogEvent::Warning)
                color = gfx::Color::Yellow;
            else continue;

            gfx::FillRect(painter, rect, gfx::Color4f(gfx::Color::Black, 0.6f));
            gfx::DrawTextRect(painter, msg.message, mDebug.debug_font, 18, rect,color,
                              gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
            rect.Translate(0, 20.0f);
        }
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
            const auto& camera = GetCamera();
            const auto& viewport = GetViewport(camera, mSurfaceWidth, mSurfaceHeight);
            gfx::DrawRectOutline(painter, gfx::FRect(viewport), gfx::Color::Green, 1.0f);
        }
    }


    void DrawDebugObjects()
    {
        if (!mDebug.debug_draw)
        {
            return;
        }

        const auto& camera = GetCamera();
        const auto device_viewport  = GetViewport(camera, mSurfaceWidth, mSurfaceHeight);
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

        if (!mScene)
            return;

        // this debug drawing is provided for the game developer to help them
        // see where the spatial nodes are, not for the engine developer to
        // debug the engine code. So this means that we assume that the engine
        // code is correct and the spatial index correctly reflects the nodes
        // and their positions. Thus the debug drawing can be based on the
        // entity/node iteration instead of iterating over the items in the
        // spatial index. (Which is a function that doesn't event exist yet).
        TRACE_BLOCK("DebugDrawScene",
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
    // list of current debug print messages that
    // get printed to the display.
    struct DebugPrint {
        std::string message;
        short lifetime = 3;
    };

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

    std::mutex mRuntimeLock;
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

#if defined(ENGINE_ENABLE_LUA_SCRIPTING)
    std::unique_ptr<engine::LuaRuntime> mLuaRuntime;
#endif

#if defined(ENGINE_ENABLE_CPP_SCRIPTING)
    std::unique_ptr<engine::CppRuntime> mCppRuntime;
#endif
    // Current game scene or nullptr if no scene.
    std::unique_ptr<game::Scene> mScene;
    // Current tilemap or nullptr if no map.
    std::unique_ptr<game::Tilemap> mTilemap;

    std::mutex mEngineLock;

    // current engine flags to control execution etc.
    base::bitflag<Flags> mFlags;
    unsigned mFrameCounter = 0;
    // current FBO 0 surface sizes
    unsigned mSurfaceWidth  = 0;
    unsigned mSurfaceHeight = 0;
    // current mouse cursor details
    EngineConfig::MouseCursorUnits mCursorUnits = EngineConfig::MouseCursorUnits::Pixels;
    glm::vec2 mCursorPos     = {0.0f, 0.0f};
    glm::vec2 mCursorHotspot = {0.0f, 0.0f};
    glm::vec2 mCursorSize    = {1.0f, 1.0f};
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

    engine::GameRuntime::Camera mCamera;
    // current debug options.
    engine::Engine::DebugOptions mDebug;
    // last statistics about the rendering rate etc.
    engine::Engine::HostStats mLastStats;

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

    engine::FrameTimer mFrameTimer;

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
    return new DetonatorEngine;
}
} // extern C
