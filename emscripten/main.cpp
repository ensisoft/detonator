
// For clang in Clion
#if !defined(__EMSCRIPTEN__)
#  define __EMSCRIPTEN__
#endif

#include "config.h"
#include "warnpush.h"
#  include <emscripten/emscripten.h>
#  include <emscripten/html5.h>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <iostream>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <variant>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "base/json.h"
#include "wdk/events.h"
#include "engine/main/interface.h"
#include "engine/loader.h"

// mkdir build
// cd build
// emcmake cmake ..
// make
// python -m http.server

namespace {
struct WebGuiToggleDbgSwitchCmd {
    std::string name;
    bool enabled = false;
};
using WebGuiCmd = std::variant<WebGuiToggleDbgSwitchCmd>;

std::queue<WebGuiCmd> gui_commands;

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

std::string GenerateGameHome(const std::string& user_home, const std::string& title)
{
    std::string dir = "/ensisoft/";
    dir += ".GameStudio";
    if (mkdir(dir.c_str(), 0777) == -1)
        WARN("Failed to create game home directory. [dir='%1', error='%2']", dir, strerror(errno));
    dir += "/";
    dir += title;
    if (mkdir(dir.c_str(), 0777) == -1)
        WARN("Failed to create game home directory. [dir='%1', error='%2']", dir, strerror(errno));
    return dir;
#if 0
    std::error_code error;
    std::filesystem::path p(user_home);
    p /= ".GameStudio";
    if (!std::filesystem::create_directory(p, error))
    p /= title;
    if (!std::filesystem::create_directory(p, error))
        ERROR("Failed to create game home directory. [dir='%1', error='%2']", p.generic_u8string(), error.message());
    return p.generic_u8string(); // std::string until c++20 (u8string)
#endif
}

class WebGLContext : public gfx::Device::Context
{
public:
    // WebGL power preference.
    enum class PowerPreference {
        // Request a default power preference setting
        Default,
        // Request a low power mode that prioritizes power saving and battery over render perf
        LowPower,
        // Request a high performance mode that prioritizes rendering perf
        // over battery life/power consumption
        HighPerf
    };
    WebGLContext(PowerPreference powerpref, bool antialias)
    {
        EmscriptenWebGLContextAttributes attrs;
        emscripten_webgl_init_context_attributes(&attrs);
        attrs.alpha                        = false;
        attrs.depth                        = true;
        attrs.stencil                      = true;
        attrs.antialias                    = antialias;
        attrs.majorVersion                 = 1; // WebGL 1.0 is based on OpenGL ES 2.0
        attrs.minorVersion                 = 0;
        attrs.preserveDrawingBuffer        = false;
        attrs.failIfMajorPerformanceCaveat = true;
        attrs.powerPreference              = EM_WEBGL_POWER_PREFERENCE_DEFAULT;
        if (powerpref == PowerPreference::HighPerf)
            attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
        else if (powerpref == PowerPreference::LowPower)
            attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_LOW_POWER;

        /*
        attrs.premultipliedAlpha           = false;
        attrs.preserveDrawingBuffer        = false;
        attrs.failIfMajorPerformanceCaveat = false;
        attrs.enableExtensionsByDefault    = false;
        attrs.explicitSwapControl          = true; // does swap/display when OnAnimationFrame returns
        attrs.renderViaOffscreenBackBuffer = false;
        attrs.powerPreference              = EM_WEBGL_POWER_PREFERENCE_DEFAULT;
        attrs.proxyContextToMainThread     = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW;
         */

        // todo: deal with failure?
        mContext = emscripten_webgl_create_context("canvas", &attrs);
        DEBUG("Create WebGL context. [context=%1]", mContext);
        emscripten_webgl_make_context_current(mContext);
    }
    ~WebGLContext()
    {
        DEBUG("Destroy WebGL context. [context=%1]", mContext);
        emscripten_webgl_destroy_context(mContext);
    }
    virtual void Display() override
    {
        // automatic on return from frame render
    }
    virtual void* Resolve(const char* name) override
    {
        void* ret = emscripten_webgl_get_proc_address(name);
        DEBUG("Resolving GL entry point. [name=%1, ret=%2]", name, ret);
        return ret;
    }
    virtual void MakeCurrent() override
    {
        emscripten_webgl_make_context_current(mContext);
    }
private:
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE mContext = 0;
};

class Application
{
public:
    Application()
    {}
   ~Application()
    {
        DEBUG("Destroy application.");
        base::SetThreadLog(nullptr);
    }

    bool Init()
    {
        // read config JSON
        const char* config_file = "config.json";
        const auto [json_ok, json, json_error] = base::JsonParseFile(config_file);
        if (!json_ok)
        {
            std::fprintf(stderr, "Failed to parse config file. [file='%s']", config_file);
            std::fprintf(stderr, "Json parse error. [error='%s']", json_error.c_str());
            return false;
        }

        bool global_log_debug = true;
        bool global_log_warn  = true;
        bool global_log_info  = true;
        bool global_log_error = true;
        base::JsonReadSafe(json["logging"], "debug", &global_log_debug);
        base::JsonReadSafe(json["logging"], "warn",  &global_log_warn);
        base::JsonReadSafe(json["logging"], "info",  &global_log_info);
        base::JsonReadSafe(json["logging"], "error", &global_log_error);
        base::SetGlobalLog(&mLogger);
        base::EnableLogEvent(base::LogEvent::Debug,   global_log_debug);
        base::EnableLogEvent(base::LogEvent::Info,    global_log_info);
        base::EnableLogEvent(base::LogEvent::Warning, global_log_warn);
        base::EnableLogEvent(base::LogEvent::Error,   global_log_error);

        DEBUG("It's alive!");
        INFO("Ensisoft Gamestudio 2D");
        INFO("Gamestudio Copyright (c) 2010-2021 Sami Vaisanen");
        INFO("http://www.ensisoft.com");
        INFO("http://github.com/ensisoft/gamestudio");

        std::string content;
        std::string title;
        std::string identifier;
        base::JsonReadSafe(json["application"], "title",   &title);
        base::JsonReadSafe(json["application"], "content", &content);
        base::JsonReadSafe(json["application"], "identifier", &identifier);
        emscripten_set_window_title(title.c_str());

        // How the HTML5 canvas is sized on the page.
        enum class CanvasMode {
            // Canvas has a fixed size
            Default,
            // Canvas is resized to match the available client area in the page.
            SoftFullScreen
        };
        CanvasMode canvas_mode = CanvasMode::Default;
        WebGLContext::PowerPreference power_pref = WebGLContext::PowerPreference::Default;
        unsigned canvas_width  = 0;
        unsigned canvas_height = 0;
        bool antialias = true;
        bool developer_ui = false;
        base::JsonReadSafe(json["html5"], "canvas_mode", &canvas_mode);
        base::JsonReadSafe(json["html5"], "canvas_width", &canvas_width);
        base::JsonReadSafe(json["html5"], "canvas_height", &canvas_height);
        base::JsonReadSafe(json["html5"], "webgl_power_pref", &power_pref);
        base::JsonReadSafe(json["html5"], "webgl_antialias", &antialias);
        base::JsonReadSafe(json["html5"], "developer_ui", &developer_ui);
        mDevicePixelRatio = emscripten_get_device_pixel_ratio();
        DEBUG("Device pixel ratio = %1.", mDevicePixelRatio);

        int canvas_render_width   = 0;
        int canvas_render_height  = 0;
        double canvas_display_width  = 0;
        double canvas_display_height = 0;
        // try to set the size of the canvas element's drawing buffer.
        // this is *not* the same as the final display size which is determined
        // by any CSS based size and browser's hDPI scale factor.
        emscripten_set_canvas_element_size("canvas", canvas_width, canvas_height);
        emscripten_get_canvas_element_size("canvas", &canvas_render_width, &canvas_render_height);
        emscripten_get_element_css_size("canvas", &canvas_display_width, &canvas_display_height);
        DEBUG("Initial canvas render target size. [width=%1, height=%2]", canvas_render_width, canvas_render_height);
        DEBUG("Initial canvas display (CSS logical) size. [width=%1, height=%2]", canvas_display_width, canvas_display_height);

        if (canvas_mode == CanvasMode::SoftFullScreen)
        {
            DEBUG("Enter soft fullscreen canvas mode.");
            SetFullScreen(true);
        }

        mContentLoader  = engine::JsonFileClassLoader::Create();
        mResourceLoader = engine::FileResourceLoader::Create();
        mContentLoader->LoadFromFile("/" + content + "/content.json");
        mResourceLoader->SetApplicationPath("/");
        mResourceLoader->SetContentPath("/" + content);

        // todo: context attributes.
        mContext.reset(new WebGLContext(power_pref, antialias));

        mEngine.reset(Gamestudio_CreateEngine());

        // IMPORTANT: make sure that the order in which the engine is setup
        // is the same between all the launcher applications.
        // that is engine/main/main.cpp and editor/gui/playwindow.cpp

        // The initial state needs to be kept in sync with the HTML5 UI somehow!
        // easiest thing is just to start with a known default state (all off)
        // and then let the UI set the state.
        engine::Engine::DebugOptions debug;
        debug.debug_pause     = false;
        debug.debug_draw      = false;
        debug.debug_show_fps  = false;
        debug.debug_show_msg  = false;
        debug.debug_print_fps = false;
        if (json.contains("debug"))
        {
            const auto& debug_settings = json["debug"];
            base::JsonReadSafe(debug_settings, "font", &debug.debug_font);
            base::JsonReadSafe(debug_settings, "show_fps", &debug.debug_show_fps);
            base::JsonReadSafe(debug_settings, "show_msg", &debug.debug_show_msg);
            base::JsonReadSafe(debug_settings, "draw", &debug.debug_draw);
            mDebugFont = debug.debug_font;
        }
        mEngine->SetDebugOptions(debug);

        engine::Engine::Environment env;
        env.classlib = mContentLoader.get();
        env.graphics_loader  = mResourceLoader.get();
        env.game_data_loader = mResourceLoader.get();
        env.audio_loader     = mResourceLoader.get();
        env.directory        = "/";
        env.user_home        = ""; // todo:
        env.game_home        = GenerateGameHome("/ensisoft", identifier);
        mEngine->SetEnvironment(env);

        engine::Engine::InitParams init;
        init.editing_mode     = false;
        init.application_name = title;
        init.context          = mContext.get();
        init.surface_width    = canvas_render_width;
        init.surface_height   = canvas_render_height;
        base::JsonReadSafe(json["application"], "game_script", &init.game_script);
        mEngine->Init(init);

        engine::Engine::EngineConfig config;
        config.updates_per_second = 60.0f;
        config.ticks_per_second   = 1.0f;

        if (json.contains("physics"))
        {
            const auto& physics_settings = json["physics"];
            base::JsonReadSafe(physics_settings, "num_velocity_iterations", &config.physics.num_velocity_iterations);
            base::JsonReadSafe(physics_settings, "num_position_iterations", &config.physics.num_position_iterations);
            base::JsonReadSafe(physics_settings, "gravity", &config.physics.gravity);
            base::JsonReadSafe(physics_settings, "scale",   &config.physics.scale);
        }
        if (json.contains("engine"))
        {
            const auto& engine_settings = json["engine"];
            base::JsonReadSafe(engine_settings, "clear_color", &config.clear_color);
            base::JsonReadSafe(engine_settings, "default_min_filter", &config.default_min_filter);
            base::JsonReadSafe(engine_settings, "default_mag_filter", &config.default_mag_filter);
            base::JsonReadSafe(engine_settings, "updates_per_second", &config.updates_per_second);
            base::JsonReadSafe(engine_settings, "ticks_per_second", &config.ticks_per_second);
            DEBUG("time_step = 1.0/%1, tick_step = 1.0/%2", config.updates_per_second, config.ticks_per_second);
        }
        if (json.contains("mouse_cursor"))
        {
            const auto& mouse_cursor = json["mouse_cursor"];
            base::JsonReadSafe(mouse_cursor, "show", &config.mouse_cursor.show);
            base::JsonReadSafe(mouse_cursor, "drawable", &config.mouse_cursor.drawable);
            base::JsonReadSafe(mouse_cursor, "material", &config.mouse_cursor.material);
            base::JsonReadSafe(mouse_cursor, "hotspot", &config.mouse_cursor.hotspot);
            base::JsonReadSafe(mouse_cursor, "size", &config.mouse_cursor.size);
            base::JsonReadSafe(mouse_cursor, "units", &config.mouse_cursor.units);
        }
        if (json.contains("audio"))
        {
            const auto& audio = json["audio"];
            base::JsonReadSafe(audio, "channels", &config.audio.channels);
            base::JsonReadSafe(audio, "sample_rate", &config.audio.sample_rate);
            base::JsonReadSafe(audio, "sample_type", &config.audio.sample_type);
            base::JsonReadSafe(audio, "buffer_size", &config.audio.buffer_size);
        }
        mEngine->SetEngineConfig(config);
        // doesn't exist here.
        mEngine->SetTracer(nullptr);
        mEngine->Load();
        mEngine->Start();

        mRenderTargetWidth   = canvas_render_width;
        mRenderTargetHeight  = canvas_render_height;
        mCanvasDisplayWidth  = canvas_display_width;
        mCanvasDisplayHeight = canvas_display_height;
        mListener = mEngine->GetWindowListener();

        // sync the HTML5 gui. (quite easy to do from here with all the data)
        // from JS would need to marshall the call with ccall
        struct UIFlag {
            const char* name;
            bool value;
        };
        UIFlag flags[] = {
            {"chk-log-debug", base::IsLogEventEnabled(base::LogEvent::Debug)},
            {"chk-log-warn", base::IsLogEventEnabled(base::LogEvent::Warning)},
            {"chk-log-info", base::IsLogEventEnabled(base::LogEvent::Info)},
            {"chk-log-error", base::IsLogEventEnabled(base::LogEvent::Error)},
            {"chk-show-fps", debug.debug_show_fps},
            {"chk-print-fps", debug.debug_print_fps},
            {"chk-dbg-draw", debug.debug_draw},
            {"chk-dbg-msg", debug.debug_show_msg}
        };
        for (int i=0; i<8; ++i)
        {
            const auto& flag = flags[i];
            const auto& script = base::FormatString("var chk = document.getElementById('%1');"
                "chk.checked = %2;", flag.name, flag.value);
            emscripten_run_script(script.c_str());
        }

        if (developer_ui)
        {
            emscripten_run_script("var ui = document.getElementById('developer-control-panel');"
                                  "ui.style.display = 'block'; ");
        }
        return true;
    }


    EM_BOOL OnCanvasResize(int event_type)
    {
        // should be called when entering/exiting soft full screen mode.
        // not used currently since the callback seems asynchronous
        // so what's the point. the contents are therefore in SetFullScreen.
        return EM_TRUE;
    }

    EM_BOOL OnWindowResize(int emsc_type, const EmscriptenUiEvent* emsc_event)
    {
        if (emsc_type == EMSCRIPTEN_EVENT_RESIZE)
        {
            int canvas_render_with   = 0;
            int canvas_render_height = 0;
            emscripten_get_canvas_element_size("canvas", &canvas_render_with, &canvas_render_height);

            wdk::WindowEventResize resize;
            resize.width  = canvas_render_with;
            resize.height = canvas_render_height;
            mEventQueue.push(resize);
        }
        return EM_TRUE;
    }

    EM_BOOL OnAnimationFrame()
    {
        if (!gui_commands.empty())
        {
            engine::Engine::DebugOptions debug;
            debug.debug_font = mDebugFont;
            while (!gui_commands.empty())
            {
                const auto& cmd = gui_commands.front();
                if (const auto* ptr = std::get_if<WebGuiToggleDbgSwitchCmd>(&cmd))
                {
                    if (ptr->name == "chk-pause")
                        debug.debug_pause = ptr->enabled;
                    if (ptr->name == "chk-show-fps")
                        debug.debug_show_fps = ptr->enabled;
                    else if (ptr->name == "chk-print-fps")
                        debug.debug_print_fps = ptr->enabled;
                    else if (ptr->name == "chk-dbg-draw")
                        debug.debug_draw = ptr->enabled;
                    else if (ptr->name == "chk-dbg-msg")
                        debug.debug_show_msg = ptr->enabled;
                    else if (ptr->name == "chk-log-debug")
                        base::EnableLogEvent(base::LogEvent::Debug, ptr->enabled);
                    else if (ptr->name == "chk-log-info")
                        base::EnableLogEvent(base::LogEvent::Info, ptr->enabled);
                    else if (ptr->name == "chk-log-warn")
                        base::EnableLogEvent(base::LogEvent::Warning, ptr->enabled);
                    else if (ptr->name == "chk-log-error")
                        base::EnableLogEvent(base::LogEvent::Error, ptr->enabled);
                    else WARN("Unknown debug flag. [flag='%1']", ptr->name);
                }
                gui_commands.pop();
            }
            mEngine->SetDebugOptions(debug);
        }

        // important: make sure that the order in which stuff is done
        // is the same across all "main application" instances.
        // i.e., native main and editor's playwindow main.

        mEngine->BeginMainLoop();

        // dispatch the pending user input events.
        while (!mEventQueue.empty())
        {
            const auto& event = mEventQueue.front();
            if (const auto* ptr = std::get_if<wdk::WindowEventMousePress>(&event))
                mListener->OnMousePress(*ptr);
            else if (const auto* ptr = std::get_if<wdk::WindowEventMouseRelease>(&event))
                mListener->OnMouseRelease(*ptr);
            else if (const auto* ptr = std::get_if<wdk::WindowEventMouseMove>(&event))
                mListener->OnMouseMove(*ptr);
            else if (const auto* ptr = std::get_if<wdk::WindowEventKeyDown>(&event))
                mListener->OnKeyDown(*ptr);
            else if (const auto* ptr = std::get_if<wdk::WindowEventKeyUp>(&event))
                mListener->OnKeyUp(*ptr);
            else if (const auto* ptr = std::get_if<wdk::WindowEventChar>(&event))
                mListener->OnChar(*ptr);
            else if (const auto* ptr = std::get_if<wdk::WindowEventResize>(&event))
            {
                // filter out superfluous event notifications when the render target
                // hasn't actually changed.
                if ((mRenderTargetHeight != ptr->height) || (mRenderTargetWidth != ptr->width))
                {
                    // for consistency's sake call the window resize event handler.
                    mListener->OnResize(*ptr);
                    // this is the main engine rendering surface callback which is important.
                    mEngine->OnRenderingSurfaceResized(ptr->width, ptr->height);

                    mRenderTargetWidth  = ptr->width;
                    mRenderTargetHeight = ptr->height;
                    DEBUG("Canvas render target size changed. [width=%1, height=%2]", ptr->width, ptr->height);
                }
                // obtain the new (if changed) canvas display width and height.
                // we need these for mapping the mouse coordinates from CSS display
                // units to render target units.
                emscripten_get_element_css_size("canvas", &mCanvasDisplayWidth, &mCanvasDisplayHeight);
                DEBUG("Canvas display (CSS logical pixel) size changed. [width=%1, height=%2]",
                      mCanvasDisplayWidth, mCanvasDisplayHeight);
            }
            else BUG("Unhandled window event.");
            mEventQueue.pop();
        }

        bool quit = false;

        // Process pending application requests if any.
        engine::Engine::Request request;
        while (mEngine->GetNextRequest(&request))
        {
            if (auto* ptr = std::get_if<engine::Engine::ResizeSurface>(&request))
                HandleEngineRequest(*ptr);
            else if (auto* ptr = std::get_if<engine::Engine::SetFullScreen>(&request))
                HandleEngineRequest(*ptr);
            else if (auto* ptr = std::get_if<engine::Engine::ToggleFullScreen>(&request))
                HandleEngineRequest(*ptr);
            else if (auto* ptr = std::get_if<engine::Engine::ShowMouseCursor>(&request))
                HandleEngineRequest(*ptr);
            else if (auto* ptr = std::get_if<engine::Engine::GrabMouse>(&request))
                HandleEngineRequest(*ptr);
            else if (auto* ptr = std::get_if<engine::Engine::ShowDeveloperUI>(&request))
                HandleEngineRequest(*ptr);
            else if (auto* ptr = std::get_if<engine::Engine::QuitApp>(&request))
                quit = true;
            else BUG("Unhandled engine request type.");
        }

        // this is the real wall time elapsed rendering the previous
        // for each iteration of the loop we measure the time
        // spent producing a frame. the time is then used to take
        // some number of simulation steps in order for the simulations
        // to catch up for the *next* frame.
        const auto time_step = ElapsedSeconds();
        const auto wall_time = CurrentRuntime();

        // ask the application to take its simulation steps.
        mEngine->Update(time_step);

        // ask the application to draw the current frame.
        mEngine->Draw();

        ++mFrames;
        ++mCounter;
        mSeconds += time_step;
        if (mSeconds > 1.0)
        {
            engine::Engine::HostStats stats;
            stats.current_fps         = mCounter / mSeconds;
            stats.num_frames_rendered = mFrames;
            stats.total_wall_time     = CurrentRuntime();
            mEngine->SetHostStats(stats);
            mCounter = 0;
            mSeconds = 0;
        }

        mEngine->EndMainLoop();

        if (!quit)
            return EM_TRUE;

        if (mSoftFullScreen)
            SetFullScreen(false);

        DEBUG("Starting shutdown sequence.");
        mEngine->SetTracer(nullptr);
        mEngine->Stop();
        mEngine->Save();
        mEngine->Shutdown();

        mEngine.reset();
        mContext.reset();
        return EM_FALSE;
    }
    void SetFullScreen(bool fullscreen)
    {
        if (fullscreen == mSoftFullScreen)
            return;

        // The soft full screen is a mode where the canvas element is
        // resized to cover the whole client area of the page.
        // It's not a "true" full screen mode that some browsers support.
        // The benefit of this soft full screen approach is that it can
        // easily be done by the game itself and doesn't suffer from web
        // security issues which prevent some operations unless done as
        // a response to user input and inside an event handler.
        if (fullscreen)
        {
            EmscriptenFullscreenStrategy fss;
            // so this shoddily named parameter controls how the
            // content of the canvas is scaled for displaying.
            fss.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_ASPECT; // scale and retain aspect ratio.

            // this shoddily named parameter controls how the
            // render target (draw buffer) of the canvas object is
            // resized/scaled when going into soft full screen mode.
            // currently, the native main applications don't do any
            // scaled display but derive the render target size directly
            // from the native window's (renderable) client surface area.
            // we should keep the same semantics here for now.
            fss.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF; // scale render target
            //EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE; // keep the render target size as-is

            // how to filter when scaling the content from render size to display size.
            fss.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;

            // callback data. dangerous but it seems that the
            // callback is executed immediately from the enter_soft_fullscreen.
            // better not to use it.
            fss.canvasResizedCallback = nullptr; //&Application::OnCanvasResize;
            fss.canvasResizedCallbackUserData = nullptr; //this;
            fss.canvasResizedCallbackTargetThread = 0;

            // looks like this will invoke the callback immediately.
            if (emscripten_enter_soft_fullscreen("canvas", &fss) != EMSCRIPTEN_RESULT_SUCCESS)
            {
                ERROR("Failed to enter soft full screen.");
                return;
            }
        }
        else
        {
            emscripten_exit_soft_fullscreen();
        }

        // handle canvas resize.

        mSoftFullScreen = fullscreen;

        // the canvas render size may or may not change depending
        // on how the full screen change happens. if we're just
        // scaling the rendered content for display then there's no
        // actual change of the render target size.
        int canvas_render_with   = 0;
        int canvas_render_height = 0;
        emscripten_get_canvas_element_size("canvas", &canvas_render_with, &canvas_render_height);

        // enqueue a notification.
        wdk::WindowEventResize resize;
        resize.width  = canvas_render_with;
        resize.height = canvas_render_height;
        mEventQueue.push(resize);
    }
    void HandleEngineRequest(const engine::Engine::ShowDeveloperUI& devui)
    {
        if (devui.show)
            emscripten_run_script("var ui = document.getElementById('developer-control-panel'); ui.style.display = 'block';");
        else emscripten_run_script("var ui = document.getElementById('developer-control-panel'); ui.style.display = 'none'; ");
        DEBUG("Request to show/hide developer UI. [show=%1]", devui.show);
    }

    void HandleEngineRequest(const engine::Engine::ResizeSurface& resize)
    {
        // note that this means the *rendering* surface size which is not
        // the same as the display size. in web the canvas object has
        // width and height attributes which define the size of the drawing buffer.
        // the same canvas also can be affected by the width and height
        // attributes of the CSS style that is applied on the canvas and
        // these define the *display* size.

        // todo: can this really fail?
        // todo: will this result in some event? (assuming no)
        if (emscripten_set_canvas_element_size("canvas", resize.width, resize.height) != EMSCRIPTEN_RESULT_SUCCESS)
        {
            ERROR("Failed to set canvas element (render target) size.[width=%1, height=%2]", resize.width, resize.height);
            return;
        }
        mRenderTargetWidth  = resize.width;
        mRenderTargetHeight = resize.height;
        DEBUG("Request resize canvas render target. [width=%d, height=%d]", resize.width, resize.height);
    }
    void HandleEngineRequest(const engine::Engine::SetFullScreen& fs)
    {
        SetFullScreen(fs.fullscreen);
        DEBUG("Request to change soft full screen mode. [fs=%1]", fs.fullscreen);
    }
    void HandleEngineRequest(const engine::Engine::ToggleFullScreen& fs)
    {
        SetFullScreen(!mSoftFullScreen);
        DEBUG("Request to toggle full screen mode. [current=%1]", mSoftFullScreen);
    }
    void HandleEngineRequest(const engine::Engine::ShowMouseCursor& mickey)
    {
        WARN("Show/hide mouse cursor is not supported. [show=%1]", mickey.show);
    }
    void HandleEngineRequest(const engine::Engine::GrabMouse& mickey)
    {
        // todo: pointer lock ?
        WARN("Mouse grab is not supported.");
    }
    void HandleEngineRequest(const engine::Engine::QuitApp& quit)
    { }

    EM_BOOL OnMouseEvent(int emsc_type, const EmscriptenMouseEvent* emsc_event)
    {
        wdk::bitflag<wdk::Keymod> mods;
        if (emsc_event->shiftKey)
            mods.set(wdk::Keymod::Shift, true);
        if (emsc_event->altKey)
            mods.set(wdk::Keymod::Alt, true);
        if (emsc_event->ctrlKey)
            mods.set(wdk::Keymod::Control, true);

        wdk::MouseButton btn = wdk::MouseButton::None;
        if (emsc_event->button == 0)
            btn = wdk::MouseButton::Left;
        else if (emsc_event->button == 1)
            btn = wdk::MouseButton::Wheel;
        else if (emsc_event->button == 2)
            btn = wdk::MouseButton::Right;
        else WARN("Unmapped mouse button. [value=%1]", emsc_event->button);

        const auto canvas_display_width  = (int)mCanvasDisplayWidth;
        const auto canvas_display_height = (int)mCanvasDisplayHeight;

        // the mouse x,y coordinates are in CSS logical pixel units.
        // if the display size of the canvas is not the same as the
        // render target size the mouse coordinates must be mapped.
        auto render_target_x = emsc_event->targetX;
        auto render_target_y = emsc_event->targetY;
        if ((canvas_display_width != mRenderTargetWidth) ||
            (canvas_display_height != mRenderTargetHeight))
        {
            const float scale = std::min(mCanvasDisplayWidth / mRenderTargetWidth,
                                         mCanvasDisplayHeight / mRenderTargetHeight);
            const auto scaled_render_width = mRenderTargetWidth * scale;
            const auto scaled_render_height = mRenderTargetHeight * scale;

            const auto css_xpos = emsc_event->targetX;
            const auto css_ypos = emsc_event->targetY;
            const auto css_offset_x = (mCanvasDisplayWidth - scaled_render_width) * 0.5;
            const auto css_offset_y = (mCanvasDisplayHeight - scaled_render_height) * 0.5;

            const auto css_normalized_xpos = (css_xpos - css_offset_x) / scaled_render_width;
            const auto css_normalized_ypos = (css_ypos - css_offset_y) / scaled_render_height;
            if ((css_normalized_xpos < 0.0 || css_normalized_xpos > 1.0f) ||
                (css_normalized_ypos < 0.0 || css_normalized_ypos > 1.0f))
                return EM_TRUE;

            render_target_x = css_normalized_xpos * mRenderTargetWidth;
            render_target_y = css_normalized_ypos * mRenderTargetHeight;
        }

        if (emsc_type == EMSCRIPTEN_EVENT_MOUSEDOWN)
        {
            wdk::WindowEventMousePress event;
            event.window_x  = render_target_x;
            event.window_y  = render_target_y;
            event.global_y  = emsc_event->screenX;
            event.global_y  = emsc_event->screenY;
            event.modifiers = mods;
            event.btn       = btn;
            mEventQueue.push(event);
            DEBUG("Mouse down event. [x=%1, y=%2]", event.window_x, event.window_y);
        }
        else if (emsc_type == EMSCRIPTEN_EVENT_MOUSEUP)
        {
            wdk::WindowEventMouseRelease event;
            event.window_x  = render_target_x;
            event.window_y  = render_target_y;
            event.global_y  = emsc_event->screenX;
            event.global_y  = emsc_event->screenY;
            event.modifiers = mods;
            event.btn       = btn;
            mEventQueue.push(event);
        }
        else if (emsc_type == EMSCRIPTEN_EVENT_MOUSEMOVE)
        {
            wdk::WindowEventMouseMove event;
            event.window_x  = render_target_x;
            event.window_y  = render_target_y;
            event.global_y  = emsc_event->screenX;
            event.global_y  = emsc_event->screenY;
            event.modifiers = mods;
            event.btn       = btn;
            mEventQueue.push(event);
        } else WARN("Unhandled mouse event.[emsc_type=%1]", emsc_type);
        return EM_TRUE;
    }
    EM_BOOL OnKeyboardEvent(int emsc_type, const EmscriptenKeyboardEvent* emsc_event)
    {
        wdk::bitflag<wdk::Keymod> mods;
        if (emsc_event->ctrlKey)
            mods.set(wdk::Keymod::Control, true);
        if (emsc_event->shiftKey)
            mods.set(wdk::Keymod::Shift, true);
        if (emsc_event->altKey)
            mods.set(wdk::Keymod::Alt, true);

        static std::unordered_map<std::string, wdk::Keysym> keymap;
        if (keymap.empty())
        {
            // unmapped. (no key in wdk)
            // "Pause", "PrintScreen", "AltRight", "MetaLeft", "MetaRight"
            // NumPad keys (multiple)
            keymap["Backspace"]    = wdk::Keysym::Backspace;
            keymap["Tab"]          = wdk::Keysym::Tab;
            keymap["Enter"]        = wdk::Keysym::Enter;
            keymap["ShiftLeft"]    = wdk::Keysym::ShiftL;
            keymap["ShiftRight"]   = wdk::Keysym::ShiftR;
            keymap["ControlLeft"]  = wdk::Keysym::ControlL;
            keymap["ControlRight"] = wdk::Keysym::ControlR;
            keymap["AltLeft"]      = wdk::Keysym::AltL;
            keymap["CapsLock"]     = wdk::Keysym::CapsLock;
            keymap["Escape"]       = wdk::Keysym::Escape;
            keymap["Space"]        = wdk::Keysym::Space;
            keymap["PageUp"]       = wdk::Keysym::PageUp;
            keymap["PageDown"]     = wdk::Keysym::PageDown;
            keymap["End"]          = wdk::Keysym::End;
            keymap["Home"]         = wdk::Keysym::Home;
            keymap["ArrowLeft"]    = wdk::Keysym::ArrowLeft;
            keymap["ArrowUp"]      = wdk::Keysym::ArrowUp;
            keymap["ArrowRight"]   = wdk::Keysym::ArrowRight;
            keymap["ArrowDown"]    = wdk::Keysym::ArrowDown;
            keymap["Insert"]       = wdk::Keysym::Insert;
            keymap["Delete"]       = wdk::Keysym::Del;
            keymap["Digit0"]       = wdk::Keysym::Key0;
            keymap["Digit1"]       = wdk::Keysym::Key1;
            keymap["Digit2"]       = wdk::Keysym::Key2;
            keymap["Digit3"]       = wdk::Keysym::Key3;
            keymap["Digit4"]       = wdk::Keysym::Key4;
            keymap["Digit5"]       = wdk::Keysym::Key5;
            keymap["Digit6"]       = wdk::Keysym::Key6;
            keymap["Digit7"]       = wdk::Keysym::Key7;
            keymap["Digit8"]       = wdk::Keysym::Key8;
            keymap["Digit9"]       = wdk::Keysym::Key9;
            keymap["KeyA"]         = wdk::Keysym::KeyA;
            keymap["KeyB"]         = wdk::Keysym::KeyB;
            keymap["KeyC"]         = wdk::Keysym::KeyC;
            keymap["KeyD"]         = wdk::Keysym::KeyD;
            keymap["KeyE"]         = wdk::Keysym::KeyE;
            keymap["KeyF"]         = wdk::Keysym::KeyF;
            keymap["KeyG"]         = wdk::Keysym::KeyG;
            keymap["KeyH"]         = wdk::Keysym::KeyH;
            keymap["KeyI"]         = wdk::Keysym::KeyI;
            keymap["KeyJ"]         = wdk::Keysym::KeyJ;
            keymap["KeyK"]         = wdk::Keysym::KeyK;
            keymap["KeyL"]         = wdk::Keysym::KeyL;
            keymap["KeyM"]         = wdk::Keysym::KeyM;
            keymap["KeyN"]         = wdk::Keysym::KeyN;
            keymap["KeyO"]         = wdk::Keysym::KeyO;
            keymap["KeyP"]         = wdk::Keysym::KeyP;
            keymap["KeyQ"]         = wdk::Keysym::KeyQ;
            keymap["KeyR"]         = wdk::Keysym::KeyR;
            keymap["KeyS"]         = wdk::Keysym::KeyS;
            keymap["KeyT"]         = wdk::Keysym::KeyT;
            keymap["KeyU"]         = wdk::Keysym::KeyU;
            keymap["KeyV"]         = wdk::Keysym::KeyV;
            keymap["KeyW"]         = wdk::Keysym::KeyW;
            keymap["KeyX"]         = wdk::Keysym::KeyX;
            keymap["KeyY"]         = wdk::Keysym::KeyY;
            keymap["KeyZ"]         = wdk::Keysym::KeyZ;
            keymap["F1"]           = wdk::Keysym::F1;
            keymap["F2"]           = wdk::Keysym::F2;
            keymap["F3"]           = wdk::Keysym::F3;
            keymap["F4"]           = wdk::Keysym::F4;
            keymap["F5"]           = wdk::Keysym::F5;
            keymap["F6"]           = wdk::Keysym::F6;
            keymap["F7"]           = wdk::Keysym::F7;
            keymap["F8"]           = wdk::Keysym::F8;
            keymap["F9"]           = wdk::Keysym::F9;
            keymap["F10"]          = wdk::Keysym::F10;
            keymap["F11"]          = wdk::Keysym::F11;
            keymap["F12"]          = wdk::Keysym::F12;
            keymap["Minus"]        = wdk::Keysym::Minus;
            keymap["Plus"]         = wdk::Keysym::Plus;
        }
        wdk::Keysym symbol = wdk::Keysym::None;
        auto it = keymap.find(emsc_event->code);
        if (it != keymap.end())
            symbol = it->second;

        if (emsc_type == EMSCRIPTEN_EVENT_KEYPRESS)
        {
            wdk::WindowEventChar character;
            std::memcpy(character.utf8, emsc_event->key, sizeof(character.utf8));
            mEventQueue.push(character);
        }
        else if (emsc_type == EMSCRIPTEN_EVENT_KEYDOWN)
        {
            wdk::WindowEventKeyDown key;
            key.modifiers = mods;
            key.symbol    = symbol;
            mEventQueue.push(key);
        }
        else if (emsc_type == EMSCRIPTEN_EVENT_KEYUP)
        {
            wdk::WindowEventKeyUp key;
            key.modifiers = mods;
            key.symbol    = symbol;
            mEventQueue.push(key);
        }
        return EM_TRUE;
    }
private:
    static EM_BOOL OnCanvasResize(int event_type, const void* reserved, void* user_data)
    { return static_cast<Application*>(user_data)->OnCanvasResize(event_type); }
private:
    base::EmscriptenLogger mLogger;
    std::unique_ptr<WebGLContext> mContext;
    std::unique_ptr<engine::Engine> mEngine;
    wdk::WindowListener* mListener = nullptr;
    std::unique_ptr<engine::JsonFileClassLoader> mContentLoader;
    std::unique_ptr<engine::FileResourceLoader>  mResourceLoader;

    using WindowEvent = std::variant<
        wdk::WindowEventResize,
        wdk::WindowEventKeyUp,
        wdk::WindowEventKeyDown,
        wdk::WindowEventChar,
        wdk::WindowEventMouseMove,
        wdk::WindowEventMousePress,
        wdk::WindowEventMouseRelease>;
    std::queue<WindowEvent> mEventQueue;
    // debug font URI (if any)
    std::string mDebugFont;
    // flag to indicate whether currently in soft fullscreen or not
    bool mSoftFullScreen = false;

    double mSeconds = 0;
    unsigned mCounter = 0;
    unsigned mFrames  = 0;
    // for High DPI display devices
    double mDevicePixelRatio = 1.0;
    // the underlying canvas render target size.
    int mRenderTargetWidth  = 0;
    int mRenderTargetHeight = 0;
    // The display size of the canvas
    // Not necessarily the same as the render target size.
    double mCanvasDisplayWidth  = 0.0f;
    double mCanvasDisplayHeight = 0.0f;
};

EM_BOOL OnWindowSizeChanged(int emsc_type, const EmscriptenUiEvent* emsc_event, void* user_data)
{
    return static_cast<Application*>(user_data)->OnWindowResize(emsc_type, emsc_event);
}
EM_BOOL OnMouseEvent(int emsc_type, const EmscriptenMouseEvent* emsc_event, void* user_data)
{
    return static_cast<Application*>(user_data)->OnMouseEvent(emsc_type, emsc_event);
}

EM_BOOL OnWheelEvent(int emsc_type, const EmscriptenWheelEvent* emsc_event, void* user_data)
{
    return EM_TRUE;
}

EM_BOOL OnKeyboardEvent(int emsc_type, const EmscriptenKeyboardEvent* emsc_event, void* user_data)
{
    return static_cast<Application*>(user_data)->OnKeyboardEvent(emsc_type, emsc_event);
}

EM_BOOL OnTouchEvent(int emsc_type, const EmscriptenTouchEvent* emsc_event, void* user_data)
{
    return EM_TRUE;
}

EM_BOOL OnFocusEvent(int emsc_type, const EmscriptenFocusEvent* emsc_event, void* user_data)
{
    return EM_TRUE;
}
EM_BOOL OnBlurEvent(int emsc_type, const EmscriptenFocusEvent* emsc_event, void* user_data)
{
    return EM_TRUE;
}
EM_BOOL OnContextEvent(int emsc_type, const void* reserved, void* user_data)
{
    return EM_TRUE;
}
EM_BOOL OnAnimationFrame(double time, void* user_data)
{
    // the time value is the time from performance.now()
    // which has 1ms resolution on ff.
    // https://developer.mozilla.org/en-US/docs/Web/API/Performance/now
    auto* app = static_cast<Application*>(user_data);

    static bool init_done = false;
    if (!init_done)
    {
        if (emscripten_run_script_int("Module.syncdone") == 1)
        {
            // todo: error checking and propagation to the HTML UI
            app->Init();
            init_done = true;
        }
    }
    if (!init_done)
        return EM_TRUE;

    const auto ret = app->OnAnimationFrame();
    // EM_TRUE means that another frame is wanted, so the app
    // is still running.
    if (ret == EM_TRUE)
        return EM_TRUE;

    // prepare to "exit" cleanly.
    DEBUG("Unregister emscripten callbacks.");
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,   nullptr, EM_FALSE /* capture*/, nullptr);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,  nullptr, true, nullptr);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,    nullptr, true, nullptr);
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_mousedown_callback("canvas",  nullptr, true, nullptr);
    emscripten_set_mouseup_callback("canvas",    nullptr, true, nullptr);
    emscripten_set_mousemove_callback("canvas",  nullptr, true, nullptr);
    emscripten_set_mouseenter_callback("canvas", nullptr, true, nullptr);
    emscripten_set_mouseleave_callback("canvas", nullptr, true, nullptr);

    DEBUG("Delete canvas element.");
    emscripten_run_script("var el = document.getElementById('canvas'); el.remove();");
    // try to sync changes to IDB filesystem back to a persistent storage.
    // todo: this is async, we'd probably need to wait until callback executes?
    emscripten_run_script("FS.syncfs(false, function(err) {});");
    INFO("Thank you for playing, G'bye!");

    emscripten_run_script("var goodbye = document.getElementById('goodbye');"
                          "goodbye.hidden = false;");
    emscripten_run_script("var ui = document.getElementById('developer-control-panel');"
                          "ui.style.display = 'none'; ");
    delete app;
    return EM_FALSE;
}
} // namespace

int main()
{
    // in emscripten the default file system is in-memory
    // only filesystem and isn't persisted anywhere.
    // IDBFS offers persistent storage but is unfortunately asynchronous.
    // try to mount it here and then synchronize from browser's
    // persistent storage into memory.
    EM_ASM(
        Module.syncdone = 0;
        FS.mkdir('/ensisoft');
        FS.mount(IDBFS, {}, '/ensisoft');
        FS.syncfs(true, function(err) {
            console.log('Filesystem sync. Error=', err);
            // there's also emscripten_push_main_loop_blocker
            Module.syncdone = 1;
        });
    );

    Application* app = new Application;

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,   app, EM_FALSE /* capture*/, OnWindowSizeChanged);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,  app, true, OnKeyboardEvent);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,    app, true, OnKeyboardEvent);
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, app, true, OnKeyboardEvent);

    emscripten_set_mousedown_callback("canvas",  app, true, OnMouseEvent);
    emscripten_set_mouseup_callback("canvas",    app, true, OnMouseEvent);
    emscripten_set_mousemove_callback("canvas",  app, true, OnMouseEvent);
    emscripten_set_mouseenter_callback("canvas", app, true, OnMouseEvent);
    emscripten_set_mouseleave_callback("canvas", app, true, OnMouseEvent);
    // note that this thread will return after calling this.
    // and after exiting the main function will go call OnAnimationFrame
    // when the browser sees fit.
    emscripten_request_animation_frame_loop(OnAnimationFrame, app);
    return 0;
}

extern "C" {
void gui_set_flag(const char* name, bool enabled)
{
    WebGuiToggleDbgSwitchCmd cmd;
    cmd.name = name;
    cmd.enabled = enabled;
    gui_commands.push(std::move(cmd));
}
} // extern "C"
