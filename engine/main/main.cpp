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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <chrono>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstring>

#if defined(LINUX_OS)
#  include <fenv.h>
#  include <dlfcn.h>
#  include <unistd.h>
#elif defined(WINDOWS_OS)
#  include <Windows.h>
#endif

#include "base/platform.h"
#include "base/logging.h"
#include "base/format.h"
#include "base/cmdline.h"
#include "base/utility.h"
#include "base/json.h"
#include "engine/main/interface.h"
#include "engine/classlib.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"

// this application will read the given JSON file and
// create window and open gl rendering context based on
// the parameters in the config file.
// then it will load the game module (in a shared object)
// and start invoking callbacks on the implementation
// provided by the game module.

// entry point functions that are resolved when the game library is loaded.
Gamestudio_CreateAppFunc         GameLibCreateApp;
Gamestudio_CreateFileLoadersFunc GameLibCreateLoaders;
Gamestudio_SetGlobalLoggerFunc   GameLibSetGlobalLogger;

#if defined(POSIX_OS)
void* application_library;
void LoadAppLibrary(const std::string& lib)
{
    // todo: check for path in the filename

    const std::string name = "./lib" + lib + ".so";
    application_library = ::dlopen(name.c_str(), RTLD_NOW);
    if (application_library == NULL)
        throw std::runtime_error(dlerror());
}
void* LoadFunction(const char* name)
{
    void* ret = dlsym(application_library, name);
    if (ret == nullptr)
        throw std::runtime_error(std::string("No such entry point: ") + name);
    DEBUG("Resolved '%1' (%2)", name, ret);
    return ret;
}

// try to figure out the location of the currently running
// executable.
std::string GetPath()
{
    std::string buffer;
    buffer.resize(2048);
    // todo: this might not be utf-8 if the system encoding is not utf-8
    const int bytes = readlink("/proc/self/exe", &buffer[0], buffer.size());
    if (bytes == -1)
        throw std::runtime_error("readlink failed. cannot discover executable location.");

    buffer.resize(bytes);
    DEBUG("Executable path: '%1'", buffer);
    auto last = buffer.find_last_of('/');
    if (last == std::string::npos)
        return buffer;
    return buffer.substr(0, last);
}

#elif defined(WINDOWS_OS)
std::string FormatError(DWORD error)
{
    LPSTR buffer = nullptr;
    const auto flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS;
    const auto lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    size_t size = FormatMessageA(flags, NULL, error, lang, (LPSTR)&buffer, 0, NULL);
    std::string ret(buffer, size);
    LocalFree(buffer);
    if (ret.back() == '\n')
        ret.pop_back();
    if (ret.back() == '\r')
        ret.pop_back();
    return ret;
}

HMODULE application_library;
void LoadAppLibrary(const std::string& lib)
{
    std::string name = lib + ".dll";
    application_library = ::LoadLibraryA(name.c_str());
    if (application_library == NULL)
        throw std::runtime_error(base::FormatString("Load library ('%1') failed with error '%2'.", name,
                                                    FormatError(GetLastError())));
}
void* LoadFunction(const char* name)
{
    void* ret = GetProcAddress(application_library, name);
    if (ret == nullptr)
        throw std::runtime_error(std::string("No such entry point: ") + name);
    DEBUG("Resolved '%1' (%2)", name, ret);
    return ret;
}

std::string GetPath()
{
    std::wstring buffer;
    buffer.resize(2048);
    const auto ret = ::GetModuleFileNameW(NULL, &buffer[0], buffer.size());
    buffer.resize(ret);
    DEBUG("Executable path: '%1'", buffer);
    auto last = buffer.find_last_of(L'\\');
    if (last == std::string::npos)
        return base::ToUtf8(buffer);
    buffer = buffer.substr(0, last);
    return base::ToUtf8(buffer);
}
#endif

// Glue class to connect the window and device
class WindowContext : public gfx::Device::Context
{
public:
    WindowContext(const wdk::Config::Attributes& attrs)
    {
        mConfig  = std::make_unique<wdk::Config>(attrs);
        mContext = std::make_unique<wdk::Context>(*mConfig, 2, 0, false, // debug
            wdk::Context::Type::OpenGL_ES);
        mVisualID = mConfig->GetVisualID();
    }
    virtual void Display() override
    {
        mContext->SwapBuffers();
    }
    virtual void* Resolve(const char* name) override
    {
        return mContext->Resolve(name);
    }
    virtual void MakeCurrent() override
    {
        mContext->MakeCurrent(mSurface.get());
    }
    wdk::uint_t GetVisualID() const
    { return mVisualID; }

    void SetWindowSurface(wdk::Window& window)
    {
        mSurface = std::make_unique<wdk::Surface>(*mConfig, window);
        mContext->MakeCurrent(mSurface.get());
        mConfig.reset();
    }
    void Dispose()
    {
        mContext->MakeCurrent(nullptr);
        mSurface->Dispose();
        mSurface.reset();
        mConfig.reset();
    }
    void SetSwapInterval(int swap_interval)
    {
        mContext->SetSwapInterval(swap_interval);
    }
private:
    std::unique_ptr<wdk::Context> mContext;
    std::unique_ptr<wdk::Surface> mSurface;
    std::unique_ptr<wdk::Config>  mConfig;
    wdk::uint_t mVisualID = 0;
};

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

int main(int argc, char* argv[])
{
#if defined(LINUX_OS)
    // SIGFPE on floating point exception
    // todo: investigate the floating point exceptions that
    // started happening after integrating box2D. Is bo2D causing
    // them or just exposing some bugs in the other parts of the code?
    /*
    feenableexcept(FE_INVALID  |
                   FE_DIVBYZERO |
                   FE_OVERFLOW|
                   FE_UNDERFLOW
                   );
    DEBUG("Enabled floating point exceptions");
     */
#endif
    int exit_code = EXIT_SUCCESS;
    try
    {
        game::App::DebugOptions debug;

        std::string config_file;
        std::string cmdline_error;
        // skip arg 0 since that's the executable name
        base::CommandLineArgumentStack args(argc-1, (const char**)&argv[1]);
        base::CommandLineOptions opt;
        opt.Add("--config", "Application configuration JSON file.", std::string("config.json"));
        opt.Add("--help", "Print this help and exit.");
        opt.Add("--debug", "Enable all debug features.");
        opt.Add("--debug-log", "Enable debug logging.");
        opt.Add("--debug-draw", "Enable debug drawing.");
        opt.Add("--debug-font", "Debug font for debug messages.", std::string(""));
        opt.Add("--debug-show-fps", "Show FPS counter and stats. You'll need to use --debug-font.");
        opt.Add("--debug-show-msg", "Show debug messages. You'll need to use --debug-font.");
        opt.Add("--debug-print-fps", "Print FPS counter and stats to log.");
        if (!opt.Parse(args, &cmdline_error, true))
        {
            std::cerr << "Error parsing args: " << cmdline_error;
            std::cerr << std::endl;
            return 0;
        }
        else if (opt.WasGiven("--help"))
        {
            opt.Print(std::cout);
            return 0;
        }

        if (opt.WasGiven("--debug"))
        {
            debug.debug_log  = true;
            debug.debug_draw = true;
            debug.debug_show_fps = true;
            debug.debug_show_msg = true;
            debug.debug_print_fps = true;
        }
        else
        {
            debug.debug_print_fps = opt.WasGiven("--debug-print-fps");
            debug.debug_show_fps  = opt.WasGiven("--debug-show-fps");
            debug.debug_log       = opt.WasGiven("--debug-log");
            debug.debug_draw      = opt.WasGiven("--debug-draw");
            debug.debug_show_msg  = opt.WasGiven("--debug-show-msg");
        }

        debug.debug_font = opt.GetValue<std::string>("--debug-font");
        if ((debug.debug_show_msg || debug.debug_show_fps) && debug.debug_font.empty())
        {
            std::cerr << "No debug font was given. Use --debug-font.";
            std::cerr << std::endl;
            return 0;
        }

        config_file = opt.GetValue<std::string>("--config");

        // setting the logger is a bit dangerous here since the current
        // build configuration builds logging.cpp into this executable
        // and possibly the library we're going to load has also built logging.cpp
        // into it. That means (with proper linker flags) the logger variables
        // are actually distinct two sets of variables. So this means that
        // if we set the global logger here then the mutex that is used to protect
        // it isn't actually no longer global (in the current logger design).
        // Two(?) solutions for this:
        //  - move the shared common code into a shared shared library
        //  - change the locking mechanism and put it into the logger.
        base::LockedLogger<base::OStreamLogger> logger((base::OStreamLogger(std::cout)));
        base::SetGlobalLog(&logger);
        base::EnableDebugLog(debug.debug_log);
        DEBUG("It's alive!");
        INFO("Copyright (c) 2010-2020 Sami Vaisanen");
        INFO("http://www.ensisoft.com");
        INFO("http://github.com/ensisoft/gamestudio");

        // read config JSON
        const auto [json_ok, json, json_error] = base::JsonParseFile(config_file);
        if (!json_ok)
        {
            ERROR("Failed to parse '%1'", config_file);
            ERROR(json_error);
            return EXIT_FAILURE;
        }

        std::string library;
        std::string content;
        std::string title = "MainWindow";
        base::JsonReadSafe(json["application"], "title", &title);
        base::JsonReadSafe(json["application"], "library", &library);
        base::JsonReadSafe(json["application"], "content", &content);
        LoadAppLibrary(library);
        DEBUG("Loaded library: '%1'", library);

        GameLibCreateApp       = (Gamestudio_CreateAppFunc)LoadFunction("Gamestudio_CreateApp");
        GameLibCreateLoaders   = (Gamestudio_CreateFileLoadersFunc)LoadFunction("Gamestudio_CreateFileLoaders");
        GameLibSetGlobalLogger = (Gamestudio_SetGlobalLoggerFunc)LoadFunction("Gamestudio_SetGlobalLogger");

        // we've created the logger object, so pass it to the engine.
        GameLibSetGlobalLogger(&logger, debug.debug_log);

        // The implementations of these types are built into the engine
        // so the engine needs to give this application a pointer back.
        Gamestudio_Loaders loaders;
        GameLibCreateLoaders(&loaders);

        if (!content.empty())
        {
            const auto& path = GetPath();
            loaders.ContentLoader->LoadFromFile(content);
            loaders.ResourceLoader->SetApplicationPath(path);
            loaders.ResourceLoader->SetContentPath(path);
        }

        // Create the app instance.
        std::unique_ptr<game::App> app(GameLibCreateApp());
        if (!app->ParseArgs(argc, (const char**)argv))
            return 0;

        app->SetDebugOptions(debug);

        game::App::Environment env;
        env.classlib         = loaders.ContentLoader.get();
        env.graphics_loader  = loaders.ResourceLoader.get();
        env.game_data_loader = loaders.ResourceLoader.get();
        env.audio_loader     = loaders.ResourceLoader.get();
        env.directory        = GetPath();
        app->SetEnvironment(env);

        wdk::Config::Attributes attrs;
        attrs.surfaces.window = true;
        attrs.double_buffer   = true;
        attrs.srgb_buffer     = true;
        base::JsonReadSafe(json["config"], "red_size", &attrs.red_size);
        base::JsonReadSafe(json["config"], "green_size", &attrs.green_size);
        base::JsonReadSafe(json["config"], "blue_size", &attrs.blue_size);
        base::JsonReadSafe(json["config"], "alpha_size", &attrs.alpha_size);
        base::JsonReadSafe(json["config"], "stencil_size", &attrs.stencil_size);
        base::JsonReadSafe(json["config"], "depth_size", &attrs.depth_size);
        base::JsonReadSafe(json["config"], "sampling", &attrs.sampling);
        base::JsonReadSafe(json["config"], "srgb", &attrs.srgb_buffer);

        DEBUG("OpenGL Config:");
        DEBUG("Red: %1, Green: %2, Blue: %3, Alpha: %4, Stencil: %5, Depth: %6",
            attrs.red_size, attrs.green_size, attrs.blue_size, attrs.alpha_size,
            attrs.stencil_size, attrs.depth_size);
        DEBUG("Sampling: %1", attrs.sampling);


        auto context = std::make_shared<WindowContext>(attrs);

        unsigned window_width  = 0;
        unsigned window_height = 0;
        bool window_can_resize = true;
        bool window_has_border = true;
        bool window_set_fullscreen = false;
        bool window_vsync = false;
        bool window_show_cursor = false;
        base::JsonReadSafe(json["window"], "width", &window_width);
        base::JsonReadSafe(json["window"], "height", &window_height);
        base::JsonReadSafe(json["window"], "can_resize", &window_can_resize);
        base::JsonReadSafe(json["window"], "has_border", &window_has_border);
        base::JsonReadSafe(json["window"], "set_fullscreen", &window_set_fullscreen);
        base::JsonReadSafe(json["window"], "vsync", &window_vsync);
        base::JsonReadSafe(json["window"], "cursor", &window_show_cursor);

        wdk::Window window;
        // makes sure to connect the listener before creating the window
        // so that the listener can get the initial events, (resize, etc)
        if (auto* listener = app->GetWindowListener())
        {
            wdk::Connect(window, *listener);
        }

        // Create application window
        window.Create(title, window_width, window_height, context->GetVisualID(),
            window_can_resize, window_has_border, true);
        window.SetFullscreen(window_set_fullscreen);
        window.ShowCursor(window_show_cursor);

        // Setup context to render in the window.
        context->SetWindowSurface(window);
        context->SetSwapInterval(window_vsync ? 1 : 0);
        DEBUG("Swap interval: %1", window_vsync ? 1 : 0);

        // setup application
        game::App::InitParams params;
        params.application_name = title;
        params.context          = context.get();
        params.surface_width    = window.GetSurfaceWidth();
        params.surface_height   = window.GetSurfaceHeight();
        app->Init(params);

        // the times here are in the application timeline which
        // is not the same as the real wall time but can drift
        float updates_per_second = 60.0f;
        float ticks_per_second = 1.0f;
        base::JsonReadSafe(json["application"], "updates_per_second", &updates_per_second);
        base::JsonReadSafe(json["application"], "ticks_per_second", &ticks_per_second);
        DEBUG("time_step = 1.0/%1, tick_step = 1.0/%2", updates_per_second, ticks_per_second);

        game::App::EngineConfig config;
        base::JsonReadSafe(json["application"], "default_min_filter", &config.default_min_filter);
        base::JsonReadSafe(json["application"], "default_mag_filter", &config.default_mag_filter);
        config.updates_per_second = updates_per_second;
        config.ticks_per_second   = ticks_per_second;
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
        }
        app->SetEngineConfig(config);
        app->Load();
        app->Start();

        bool quit = false;
        // initialize to false, so that if the window was requested to go into full screen
        // after being created we'll already do the state transition properly and
        // invoke the application handlers.
        bool fullscreen = false;

        while (app->IsRunning() && !quit)
        {
            // indicate beginning of the main loop iteration.
            app->BeginMainLoop();

            // process pending window events if any.
            wdk::native_event_t event;
            while(wdk::PeekEvent(event))
            {
                window.ProcessEvent(event);
                // if the window was resized notify the app that the rendering
                // surface has been resized.
                if (event.identity() == wdk::native_event_t::type::window_resize)
                    app->OnRenderingSurfaceResized(window.GetSurfaceWidth(), window.GetSurfaceHeight());

                if (fullscreen != window.IsFullscreen())
                {
                    if (window.IsFullscreen())
                        app->OnEnterFullScreen();
                    else app->OnLeaveFullScreen();
                    fullscreen = window.IsFullscreen();
                }
            }

            // Process pending application requests if any.
            game::App::Request request;
            while (app->GetNextRequest(&request))
            {
                if (auto* ptr = std::get_if<game::App::ResizeWindow>(&request))
                    window.SetSize(ptr->width, ptr->height);
                else if (auto* ptr = std::get_if<game::App::MoveWindow>(&request))
                    window.Move(ptr->xpos, ptr->ypos);
                else if (auto* ptr = std::get_if<game::App::SetFullScreen>(&request))
                    window.SetFullscreen(ptr->fullscreen);
                else if (auto* ptr = std::get_if<game::App::ToggleFullScreen>(&request))
                    window.SetFullscreen(!window.IsFullscreen());
                else if (auto* ptr = std::get_if<game::App::ShowMouseCursor>(&request))
                    window.ShowCursor(ptr->show);
                else if (auto* ptr = std::get_if<game::App::QuitApp>(&request)) {
                    quit = true;
                    exit_code = ptr->exit_code;
                    INFO("Quit with exit code %1", exit_code);
                }
            }
            // this is the real wall time elapsed rendering the previous
            // for each iteration of the loop we measure the time
            // spent producing a frame. the time is then used to take
            // some number of simulation steps in order for the simulations
            // to catch up for the *next* frame.
            const auto time_step = ElapsedSeconds();
            const auto wall_time = CurrentRuntime();

            // ask the application to take its simulation steps.
            app->Update(time_step);

            // ask the application to draw the current frame.
            app->Draw();

            // do some simple statistics bookkeeping.
            static auto frames_total = 0;
            static auto frames       = 0;
            static auto seconds      = 0.0;

            frames_total += 1;
            frames  += 1;
            seconds += time_step;
            if (seconds > 1.0)
            {
                const auto fps = frames / seconds;
                game::App::Stats stats;
                stats.current_fps         = fps;
                stats.num_frames_rendered = frames_total;
                stats.total_wall_time     = wall_time;
                app->UpdateStats(stats);

                frames  = 0;
                seconds = 0.0;
            }
            // indicate end of iteration.
            app->EndMainLoop();
        } // main loop

        app->Save();
        app->Shutdown();
        app.reset();

        context->Dispose();

        GameLibSetGlobalLogger(nullptr, false);
        DEBUG("Exiting...");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Oops there was a problem:\n";
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Have a good day.\n";
    std::cout << std::endl;
    return exit_code;
}
