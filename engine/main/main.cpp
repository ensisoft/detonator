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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <cstring>
#include <chrono>
#include <cmath>
#include <iostream>

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
MakeAppFunc                   GameLibMakeApp;
SetResourceLoaderFunc         GameLibSetResourceLoader;
SetGlobalLoggerFunc           GameLibSetGlobalLogger;
CreateDefaultEnvironmentFunc  GameLibCreateEnvironment;
DestroyDefaultEnvironmentFunc GameLibDestroyEnvironment;

#if defined(POSIX_OS)
void* application_library;
void LoadAppLibrary(const std::string& lib)
{
    // todo: check for path in the filename
#if defined(NDEBUG)
    const std::string name = "./lib" + lib + ".so";
#else
    const std::string name = "./lib" + lib + "d.so";
#endif
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
HMODULE application_library;
void LoadAppLibrary(const std::string& lib)
{
    std::string name = lib + ".dll";
    application_library = ::LoadLibraryA(name.c_str());
    if (application_library == NULL)
        throw std::runtime_error("Load library failed."); // todo: error string message
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
        mConfig.release();
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

        // open config file.
        #if defined(WINDOWS_OS)
            std::fstream in(base::FromUtf8(config_file)); // msvs extension
        #elif defined(POSIX_OS)
            std::fstream in(config_file);
        #endif
        if (!in.is_open())
            throw std::runtime_error("Failed to open: " + config_file);

        // read config JSON
        const auto& json = nlohmann::json::parse(in);

        std::string library;
        std::string content;
        std::string title = "MainWindow";
        base::JsonReadSafe(json["application"], "title", &title);
        base::JsonReadSafe(json["application"], "library", &library);
        base::JsonReadSafe(json["application"], "content", &content);
        LoadAppLibrary(library);
        DEBUG("Loaded library: '%1'", library);

        GameLibMakeApp            = (MakeAppFunc)LoadFunction("MakeApp");
        GameLibSetResourceLoader  = (SetResourceLoaderFunc)LoadFunction("SetResourceLoader");
        GameLibSetGlobalLogger    = (SetGlobalLoggerFunc)LoadFunction("SetGlobalLogger");
        GameLibCreateEnvironment  = (CreateDefaultEnvironmentFunc)LoadFunction("CreateDefaultEnvironment");
        GameLibDestroyEnvironment = (DestroyDefaultEnvironmentFunc)LoadFunction("DestroyDefaultEnvironment");

        // we've created the logger object, so pass it to the engine.
        GameLibSetGlobalLogger(&logger);

        // The implementations of these types are built into the engine
        // so the engine needs to give this application a pointer back.
        game::ClassLibrary* classlib = nullptr;
        GameLibCreateEnvironment(&classlib);
        if (!content.empty())
        {
            classlib->LoadFromFile(".", content);
        }

        // Create the app instance.
        std::unique_ptr<game::App> app(GameLibMakeApp());
        if (!app->ParseArgs(argc, (const char**)argv))
            return 0;

        app->SetDebugOptions(debug);

        game::App::Environment env;
        env.classlib  = classlib;
        env.directory = GetPath();
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
        base::JsonReadSafe(json["window"], "width", &window_width);
        base::JsonReadSafe(json["window"], "height", &window_height);
        base::JsonReadSafe(json["window"], "can_resize", &window_can_resize);
        base::JsonReadSafe(json["window"], "has_border", &window_has_border);
        base::JsonReadSafe(json["window"], "set_fullscreen", &window_set_fullscreen);
        base::JsonReadSafe(json["window"], "vsync", &window_vsync);

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
        window.ShowCursor(false);

        // Setup context to render in the window.
        context->SetWindowSurface(window);
        context->SetSwapInterval(window_vsync ? 1 : 0);
        DEBUG("Swap interval: %1", window_vsync ? 1 : 0);

        // setup application
        app->Init(context.get(), window.GetSurfaceWidth(), window.GetSurfaceHeight());

        // the times here are in the application timeline which
        // is not the same as the real wall time but can drift
        float updates_per_second = 60.0f;
        float ticks_per_second = 1.0f;
        base::JsonReadSafe(json["application"], "updates_per_second", &updates_per_second);
        base::JsonReadSafe(json["application"], "ticks_per_second", &ticks_per_second);

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

        // there's plenty of information about different ways ot write a basic
        // game rendering loop. here are some suggested references:
        // https://gafferongames.com/post/fix_your_timestep/
        // Game Engine Architecture by Jason Gregory
        double game_time_total = 0.0; // total time so far.
        double game_time_step  = 1.0f / updates_per_second; // the simulation time step
        double game_tick_step  = 1.0f / ticks_per_second;
        double tick_accum = 0.0; // the time available for taking tick steps.
        double time_accum = 0.0; // the time available for taking update steps.
        DEBUG("time_step = 1.0/%1, tick_step = 1.0/%2", updates_per_second, ticks_per_second);

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
                else if (auto* ptr = std::get_if<game::App::QuitApp>(&request))
                    quit = true;
            }

            // this is the real wall time elapsed rendering the previous
            // for each iteration of the loop we measure the time
            // spent producing a frame. the time is then used to take
            // some number of simulation steps in order for the simulations
            // to catch up for the *next* frame.
            const auto previous_frame_time = ElapsedSeconds();

            time_accum += previous_frame_time;

            // do simulation/animation update steps.
            while (time_accum >= game_time_step)
            {
                const auto wall_time = CurrentRuntime();
                // if the simulation step takes more real time than
                // what the time step is worth we're going to start falling
                // behind, i.e. the frame times will grow and and for the
                // bigger time values more simulation steps need to be taken
                // which will slow things down even more.
                app->Update(wall_time, game_time_total, game_time_step);
                game_time_total += game_time_step;
                time_accum -= game_time_step;

                //put some accumulated time towards ticking game
                tick_accum += game_time_step;
            }

            // do game tick steps
            auto game_tick_time = game_time_total;
            while (tick_accum >= game_tick_step)
            {
                const auto wall_time = CurrentRuntime();
                app->Tick(wall_time, game_tick_time, game_tick_step);
                game_tick_time += game_tick_step;
                tick_accum -= game_tick_step;
            }

            // ask the application to draw the current frame.
            app->Draw();

            // do some simple statistics bookkeeping.
            static auto frames_total = 0;
            static auto frames       = 0;
            static auto seconds      = 0.0;

            frames_total += 1;
            frames  += 1;
            seconds += previous_frame_time;
            if (seconds > 1.0)
            {
                const auto fps = frames / seconds;

                game::App::Stats stats;
                stats.current_fps = fps;
                stats.num_frames_rendered = frames_total;
                stats.total_game_time = game_time_total;
                stats.total_wall_time = CurrentRuntime();
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

        GameLibDestroyEnvironment(classlib);
        GameLibSetResourceLoader(nullptr);
        GameLibSetGlobalLogger(nullptr);
        DEBUG("Exiting...");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Oops there was a problem:\n";
        std::cerr << e.what() << std::endl;
        return 1;
    }
    std::cout << "Have a good day.\n";
    std::cout << std::endl;
    return 0;
}
