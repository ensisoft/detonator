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
#include <filesystem>
#include <optional>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#if defined(LINUX_OS)
#  include <fenv.h>
#  include <dlfcn.h>
#  include <unistd.h>
#  include <sys/types.h>
#  include <pwd.h>
#elif defined(WINDOWS_OS)
#  include <Windows.h>
#  include <Shlobj.h>
#endif

#include "base/platform.h"
#include "base/logging.h"
#include "base/format.h"
#include "base/cmdline.h"
#include "base/utility.h"
#include "base/json.h"
#include "base/trace.h"
#include "engine/main/interface.h"
#include "engine/classlib.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"
#include "interface.h"

// this application will read the given JSON file and
// create window and open gl rendering context based on
// the parameters in the config file.
// then it will load the game module (in a shared object)
// and start invoking callbacks on the implementation
// provided by the game module.

// entry point functions that are resolved when the game library is loaded.
Gamestudio_CreateEngineFunc      GameLibCreateEngine;
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

std::string DiscoverUserHome()
{
    struct passwd* pw = ::getpwuid(::getuid());
    if (pw == nullptr || pw->pw_dir == nullptr)
        throw std::runtime_error("user's home directory location not found");
    return pw->pw_dir;
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

std::string DiscoverUserHome()
{
    WCHAR path[MAX_PATH] = {};
    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path)))
        throw std::runtime_error("user's home directory location not found");
    return base::ToUtf8(std::wstring(path));
}

#endif

std::string GenerateGameHome(const std::string& user_home, const std::string& title)
{
    std::filesystem::path p(user_home);
    p /= ".GameStudio";
    std::filesystem::create_directory(p);
    p /= title;
    std::filesystem::create_directory(p);
    return p.generic_u8string(); // std::string until c++20 (u8string)
}

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

void SaveState(const std::string& file, const wdk::Window& window)
{
    nlohmann::json json;
    base::JsonWrite(json["window"], "width",  window.GetSurfaceWidth());
    base::JsonWrite(json["window"], "height", window.GetSurfaceHeight());
    base::JsonWrite(json["window"], "xpos", window.GetPosX());
    base::JsonWrite(json["window"], "ypos", window.GetPosY());
    base::JsonWrite(json["window"], "fullscreen", window.IsFullscreen());
    const auto [success, error] = base::JsonWriteFile(json, file);
    if (success) return;

    ERROR("Failed to save main app state.", error);
}

void LoadState(const std::string& file, wdk::Window& window)
{
    if (!base::FileExists(file))
        return;

    const auto [json_ok, json, json_error] = base::JsonParseFile(file);
    if (!json_ok)
    {
        ERROR("Failed to read _app_state.json ('%1')", json_error);
        return;
    }

    int window_width = 0;
    int window_height = 0;
    int window_xpos = 0;
    int window_ypos = 0;
    bool window_set_fullscreen = false;
    base::JsonReadSafe(json["window"], "width", &window_width);
    base::JsonReadSafe(json["window"], "height", &window_height);
    base::JsonReadSafe(json["window"], "xpos", &window_xpos);
    base::JsonReadSafe(json["window"], "ypos", &window_ypos);
    base::JsonReadSafe(json["window"], "fullscreen", &window_set_fullscreen);
    window.Move(window_xpos, window_ypos);
    window.SetSize(window_width, window_height);
    window.SetFullscreen(window_set_fullscreen);
    DEBUG("Previous window state %1x%2 @ %3%,%4 full screen = %5.",
          window_width, window_height,
          window_xpos, window_ypos,
          window_set_fullscreen ? "True" : "False");
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
        engine::Engine::DebugOptions debug;
        std::optional<bool> debug_log_override;
        std::optional<bool> vsync_override;

        std::string trace_file;
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
        opt.Add("--debug-font", "Set debug font for debug messages.", std::string(""));
        opt.Add("--debug-show-fps", "Show FPS counter and stats. You'll need to use --debug-font.");
        opt.Add("--debug-show-msg", "Show debug messages. You'll need to use --debug-font.");
        opt.Add("--debug-print-fps", "Print FPS counter and stats to log.");
        opt.Add("--trace", "Record engine function call trace and timing info into a file.", std::string("trace.txt"));
        opt.Add("--vsync", "Force vsync on or off.", false);
        if (!opt.Parse(args, &cmdline_error, true))
        {
            std::fprintf(stdout, "Error parsing args. [err='%s']\n", cmdline_error.c_str());
            return 0;
        }
        else if (opt.WasGiven("--help"))
        {
            opt.Print(std::cout);
            return 0;
        }
        if (opt.WasGiven("--trace"))
        {
            trace_file = opt.GetValue<std::string>("--trace");
        }
        if (opt.WasGiven("--vsync"))
        {
            vsync_override = opt.GetValue<bool>("--vsync");
        }

        if (opt.WasGiven("--debug"))
        {
            debug_log_override = true;
            debug.debug_draw      = true;
            debug.debug_show_fps  = true;
            debug.debug_show_msg  = true;
            debug.debug_print_fps = true;
        }
        else
        {
            if (opt.WasGiven("--debug-log"))
                debug_log_override = true;

            debug.debug_print_fps = opt.WasGiven("--debug-print-fps");
            debug.debug_show_fps  = opt.WasGiven("--debug-show-fps");
            debug.debug_draw      = opt.WasGiven("--debug-draw");
            debug.debug_show_msg  = opt.WasGiven("--debug-show-msg");
        }

        debug.debug_font = opt.GetValue<std::string>("--debug-font");
        if ((debug.debug_show_msg || debug.debug_show_fps) && debug.debug_font.empty())
        {
            std::fprintf(stdout, "No debug font was given. Use --debug-font.\n");
            return 0;
        }

        config_file = opt.GetValue<std::string>("--config");

        const auto [json_ok, json, json_error] = base::JsonParseFile(config_file);
        if (!json_ok)
        {
            std::fprintf(stderr, "Failed to parse config file. [file='%s']", config_file.c_str());
            std::fprintf(stderr, "Json parse error. [error='%s']", json_error.c_str());
            return EXIT_FAILURE;
        }
        bool global_log_debug = true;
        bool global_log_warn  = true;
        bool global_log_info  = true;
        bool global_log_error = true;
        base::JsonReadSafe(json["logging"], "debug", &global_log_debug);
        base::JsonReadSafe(json["logging"], "warn",  &global_log_warn);
        base::JsonReadSafe(json["logging"], "info",  &global_log_info);
        base::JsonReadSafe(json["logging"], "error", &global_log_error);

        if (debug_log_override.has_value())
            global_log_debug = debug_log_override.value();

        // setting the logger is a bit dangerous here since the current
        // build configuration builds logging.cpp into this executable
        // and possibly the library we're going to load has also built logging.cpp
        // into it. That means (with proper linker flags) the logger variables
        // are actually distinct two sets of variables. So this means that
        // if we set the global logger here then the mutex that is used to protect
        // it isn't actually no longer global (in the current logger design).
        // Two(?) solutions for this:
        //  - move the shared common code into a shared library
        //  - change the locking mechanism and put it into the logger.
        base::LockedLogger<base::OStreamLogger> logger((base::OStreamLogger(std::cout)));
        base::SetGlobalLog(&logger);
        base::EnableLogEvent(base::LogEvent::Debug,   global_log_debug);
        base::EnableLogEvent(base::LogEvent::Info,    global_log_info);
        base::EnableLogEvent(base::LogEvent::Warning, global_log_warn);
        base::EnableLogEvent(base::LogEvent::Error,   global_log_error);

        DEBUG("It's alive!");
        INFO("Ensisoft Gamestudio 2D");
        INFO("Copyright (c) 2010-2020 Sami Vaisanen");
        INFO("http://www.ensisoft.com");
        INFO("http://github.com/ensisoft/gamestudio");

        std::unique_ptr<base::TraceWriter> trace_writer;
        std::unique_ptr<base::TraceLog> trace_logger;
        if (opt.WasGiven("--trace"))
        {
            if (base::EndsWith(trace_file, ".json"))
                trace_writer.reset(new base::ChromiumTraceJsonWriter(trace_file));
            else trace_writer.reset(new base::TextFileTraceWriter(trace_file));
            trace_logger.reset(new base::TraceLog(1000));
            base::SetThreadTrace(trace_logger.get());
        }


        std::string library;
        std::string content;
        std::string title;
        std::string identifier;
        base::JsonReadSafe(json["application"], "title",   &title);
        base::JsonReadSafe(json["application"], "library", &library);
        base::JsonReadSafe(json["application"], "content", &content);
        base::JsonReadSafe(json["application"], "identifier", &identifier);
        LoadAppLibrary(library);
        DEBUG("Loaded library: '%1'", library);

        GameLibCreateEngine    = (Gamestudio_CreateEngineFunc)LoadFunction("Gamestudio_CreateEngine");
        GameLibCreateLoaders   = (Gamestudio_CreateFileLoadersFunc)LoadFunction("Gamestudio_CreateFileLoaders");
        GameLibSetGlobalLogger = (Gamestudio_SetGlobalLoggerFunc)LoadFunction("Gamestudio_SetGlobalLogger");

        // we've created the logger object, so pass it to the engine library
        // which has its own copies of the global state.
        GameLibSetGlobalLogger(&logger,
            global_log_debug, global_log_warn, global_log_info, global_log_error);

        // The implementations of these types are built into the engine
        // so the engine needs to give this application a pointer back.
        Gamestudio_Loaders loaders;
        GameLibCreateLoaders(&loaders);

        if (!content.empty())
        {
            const auto& application_path = GetPath();
            const auto& content_path = base::JoinPath(application_path, content);
            const auto& content_file = base::JoinPath(content_path, "content.json");
            DEBUG("Content package. [package='%1']", content);
            DEBUG("Content path. [path='%1']", content_path);
            DEBUG("Content file. [file='%1']", content_file);
            if (!loaders.ContentLoader->LoadFromFile(content_file))
                return EXIT_FAILURE;
            loaders.ResourceLoader->SetApplicationPath(application_path);
            loaders.ResourceLoader->SetContentPath(content_path);
        }

        // Create the app instance.
        std::unique_ptr<engine::Engine> engine(GameLibCreateEngine());
        if (!engine->ParseArgs(argc, (const char**)argv))
            return 0;

        engine->SetDebugOptions(debug);

        engine::Engine::Environment env;
        env.classlib         = loaders.ContentLoader.get();
        env.graphics_loader  = loaders.ResourceLoader.get();
        env.game_data_loader = loaders.ResourceLoader.get();
        env.audio_loader     = loaders.ResourceLoader.get();
        env.directory        = GetPath();
        env.user_home        = DiscoverUserHome();
        env.game_home        = GenerateGameHome(env.user_home, identifier);
        engine->SetEnvironment(env);

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
        bool window_grab_mouse  = false;
        base::JsonReadSafe(json["window"], "width", &window_width);
        base::JsonReadSafe(json["window"], "height", &window_height);
        base::JsonReadSafe(json["window"], "can_resize", &window_can_resize);
        base::JsonReadSafe(json["window"], "has_border", &window_has_border);
        base::JsonReadSafe(json["window"], "set_fullscreen", &window_set_fullscreen);
        base::JsonReadSafe(json["window"], "vsync", &window_vsync);
        base::JsonReadSafe(json["window"], "cursor", &window_show_cursor);
        base::JsonReadSafe(json["window"], "grab_mouse", &window_grab_mouse);

        if (vsync_override.has_value())
            window_vsync = vsync_override.value();

        wdk::Window window;
        // makes sure to connect the listener before creating the window
        // so that the listener can get the initial events, (resize, etc)
        if (auto* listener = engine->GetWindowListener())
        {
            wdk::Connect(window, *listener);
        }

        // Create application window
        window.Create(title, window_width, window_height, context->GetVisualID(),
            window_can_resize, window_has_border, true);
        window.SetFullscreen(window_set_fullscreen);
        window.ShowCursor(window_show_cursor);
        window.GrabMouse(window_grab_mouse);

        // Setup context to render in the window.
        context->SetWindowSurface(window);
        context->SetSwapInterval(window_vsync ? 1 : 0);
        DEBUG("Swap interval: %1", window_vsync ? 1 : 0);

        // setup application
        engine::Engine::InitParams params;
        params.editing_mode     = false; // no editing, static means optimal static, no checking for changes.
        params.application_name = title;
        params.context          = context.get();
        params.surface_width    = window.GetSurfaceWidth();
        params.surface_height   = window.GetSurfaceHeight();
        base::JsonReadSafe(json["application"], "game_script", &params.game_script);
        engine->Init(params);

        engine::Engine::EngineConfig config;
        config.ticks_per_second   = 1.0f;
        config.updates_per_second = 60.0f;

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

        // check whether there's a state file with previous window geometry
        const auto& state_file = base::JoinPath(env.game_home, "_app_state.json");

        bool save_window_geometry = false;
        base::JsonReadSafe(json["application"], "save_window_geometry", &save_window_geometry);
        if (save_window_geometry)
            LoadState(state_file, window);

        engine->SetTracer(trace_logger.get());
        engine->SetEngineConfig(config);
        engine->Load();
        engine->Start();

        bool quit = false;
        // initialize to false, so that if the window was requested to go into full screen
        // after being created we'll already do the state transition properly and
        // invoke the application handlers.
        bool fullscreen = false;

        while (engine->IsRunning() && !quit)
        {
            TRACE_START();
            TRACE_ENTER(MainLoop);

            // indicate beginning of the main loop iteration.
            TRACE_CALL("Engine::BeginMainLoop", engine->BeginMainLoop());

            TRACE_ENTER(EventDispatch);
            // process pending window events if any.
            wdk::native_event_t event;
            while(wdk::PeekEvent(event))
            {
                window.ProcessEvent(event);
                // if the window was resized notify the app that the rendering
                // surface has been resized.
                if (event.identity() == wdk::native_event_t::type::window_resize)
                    engine->OnRenderingSurfaceResized(window.GetSurfaceWidth(), window.GetSurfaceHeight());

                if (fullscreen != window.IsFullscreen())
                {
                    if (window.IsFullscreen())
                        engine->OnEnterFullScreen();
                    else engine->OnLeaveFullScreen();
                    fullscreen = window.IsFullscreen();
                }
            }
            TRACE_LEAVE(EventDispatch);

            TRACE_ENTER(EngineRequest);
            // Process pending application requests if any.
            engine::Engine::Request request;
            while (engine->GetNextRequest(&request))
            {
                if (auto* ptr = std::get_if<engine::Engine::ResizeSurface>(&request))
                    window.SetSize(ptr->width, ptr->height);
                else if (auto* ptr = std::get_if<engine::Engine::SetFullScreen>(&request))
                    window.SetFullscreen(ptr->fullscreen);
                else if (auto* ptr = std::get_if<engine::Engine::ToggleFullScreen>(&request))
                    window.SetFullscreen(!window.IsFullscreen());
                else if (auto* ptr = std::get_if<engine::Engine::ShowMouseCursor>(&request))
                    window.ShowCursor(ptr->show);
                else if (auto* ptr = std::get_if<engine::Engine::GrabMouse>(&request))
                    window.GrabMouse(ptr->grab);
                else if (auto* ptr = std::get_if<engine::Engine::QuitApp>(&request)) {
                    quit = true;
                    exit_code = ptr->exit_code;
                    INFO("Quit with exit code %1", exit_code);
                }
            }
            TRACE_LEAVE(EngineRequest);
            // this is the real wall time elapsed rendering the previous
            // for each iteration of the loop we measure the time
            // spent producing a frame. the time is then used to take
            // some number of simulation steps in order for the simulations
            // to catch up for the *next* frame.
            const auto time_step = ElapsedSeconds();
            const auto wall_time = CurrentRuntime();

            // ask the application to take its simulation steps.
            TRACE_CALL("Engine::Update", engine->Update(time_step));

            // ask the application to draw the current frame.
            TRACE_CALL("Engine::Draw", engine->Draw());

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
                engine::Engine::HostStats stats;
                stats.current_fps         = fps;
                stats.num_frames_rendered = frames_total;
                stats.total_wall_time     = wall_time;
                engine->SetHostStats(stats);

                frames  = 0;
                seconds = 0.0;
            }
            // indicate end of iteration.
            TRACE_CALL("Engine::EndMainLoop", engine->EndMainLoop());
            TRACE_LEAVE(MainLoop);

            if (trace_logger)
            {
                trace_logger->Write(*trace_writer);
            }
        } // main loop

        engine->SetTracer(nullptr);

        engine->Stop();
        engine->Save();
        engine->Shutdown();
        engine.reset();

        context->Dispose();

        if (save_window_geometry)
            SaveState(state_file, window);

        window.Destroy();

        GameLibSetGlobalLogger(nullptr, false, false, false, false);
        DEBUG("Exiting...");
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Oops there was a problem. [error='%s']", e.what());
        return EXIT_FAILURE;
    }
    std::printf("Exiting. Have a good day. [code=%d]\n", exit_code);
    return exit_code;
}
