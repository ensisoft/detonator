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

#include <algorithm>
#include <thread>
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
#include "data/json.h"
#include "device/device.h"
#include "engine/main/interface.h"
#include "engine/classlib.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"
#include "wdk/videomode.h"
#include "interface.h"

#include "git.h"

#if defined(WINDOWS_OS)
extern "C" _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
#endif

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
class WindowContext : public dev::Context
{
public:
    WindowContext(const wdk::Config::Attributes& attrs, bool debug)
    {
        mConfig  = std::make_unique<wdk::Config>(attrs);
        mContext = std::make_unique<wdk::Context>(*mConfig, 3, 0, debug, wdk::Context::Type::OpenGL_ES);
        mVisualID = mConfig->GetVisualID();
        mDebug = debug;
    }
    virtual void Display() override
    {
        TRACE_CALL("Context::SwapBuffers", mContext->SwapBuffers());
    }
    virtual void* Resolve(const char* name) override
    {
        return mContext->Resolve(name);
    }
    virtual void MakeCurrent() override
    {
        mContext->MakeCurrent(mSurface.get());
    }
    virtual Version GetVersion() const override
    {
        return Version::OpenGL_ES3;
    }
    virtual bool IsDebug() const override
    {
        return mDebug;
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
    bool mDebug = false;
};

enum class TimeId {
    GameTime,
    LoopTime
};

// returns number of seconds elapsed since the last call
// of this function.
template<TimeId id>
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

void SaveWindowState(const std::string& file, const wdk::Window& window)
{
    nlohmann::json json;
    base::JsonWrite(json["window"], "width",  window.GetSurfaceWidth());
    base::JsonWrite(json["window"], "height", window.GetSurfaceHeight());
    base::JsonWrite(json["window"], "xpos", window.GetPosX());
    base::JsonWrite(json["window"], "ypos", window.GetPosY());
    const auto [success, error] = base::JsonWriteFile(json, file);
    if (success) return;

    ERROR("Failed to save window state. [file='%1', error='%2']", file, error);
}

void ProcessEvents(wdk::Window& window)
{
    for (unsigned i=0; i<10; ++i)
    {
        wdk::native_event_t event;
        while (wdk::PeekEvent(event))
        {
            window.ProcessEvent(event);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void LoadWindowState(const std::string& file, wdk::Window& window)
{
    const auto [json_ok, json, json_error] = base::JsonParseFile(file);
    if (!json_ok)
    {
        ERROR("Failed to read window state file. [file='%1', error='%2']", file, json_error);
        return;
    }

    unsigned surface_width = 0;
    unsigned surface_height = 0;
    int window_xpos = 0;
    int window_ypos = 0;
    base::JsonReadSafe(json["window"], "width",  &surface_width);
    base::JsonReadSafe(json["window"], "height", &surface_height);
    base::JsonReadSafe(json["window"], "xpos",   &window_xpos);
    base::JsonReadSafe(json["window"], "ypos",   &window_ypos);
    DEBUG("Previous window state %1x%2 @ %3%,%4.", surface_width, surface_height, window_xpos, window_ypos);

    // try to relocate the window in case the current coordinates
    // would place it offscreen.
    const auto& mode = wdk::GetCurrentVideoMode();
    if (window_xpos >= mode.xres)
        window_xpos = 0;
    if (window_ypos >= mode.yres)
        window_ypos = 0;

    window.Move(window_xpos, window_ypos);

    surface_width  = std::min(mode.xres, surface_width);
    surface_height = std::min(mode.yres, surface_height);
    ASSERT(surface_width && surface_height);
    if ((surface_width != window.GetSurfaceWidth()) ||
        (surface_height != window.GetSurfaceHeight()))
        window.SetSize(surface_width, surface_height);

    ProcessEvents(window);
}

void CenterWindowOnScreen(wdk::Window& window)
{
    // todo: this probably won't work with multiple displays...

    // this is actually a bit off when the window has a border around it...
    // because then the actual window size is bigger than the surface size.
    const auto width = window.GetSurfaceWidth();
    const auto height = window.GetSurfaceHeight();
    const auto& mode = wdk::GetCurrentVideoMode();
    DEBUG("Current window surface %1x%2. ", width, height);
    DEBUG("Current video mode %1x%2", mode.xres, mode.yres);

    const auto surface_width = std::min(mode.xres, width);
    const auto surface_height = std::min(mode.yres, height);

    const auto xpos = (mode.xres - surface_width) / 2;
    const auto ypos = (mode.yres - surface_height) / 2;
    window.Move(xpos, ypos);

    ASSERT(surface_width && surface_height);
    if ((surface_width != window.GetSurfaceWidth()) ||
        (surface_height != window.GetSurfaceHeight()))
        window.SetSize(surface_width, surface_height);

    ProcessEvents(window);
    DEBUG("Reformat the window. %1x%2 @ %3,%4", surface_width, surface_height, xpos, ypos);
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
        bool debug_context = false;
        bool trace_jank = true;
        bool report_jank = false;
        float jank_factor = 1.25f;

        std::string trace_file;
        std::string config_file;
        std::string cmdline_error;
        // skip arg 0 since that's the executable name
        base::CommandLineArgumentStack args(argc-1, (const char**)&argv[1]);
        base::CommandLineOptions opt;
        opt.Add("--config", "Application configuration JSON file.", std::string("config.json"));
        opt.Add("--help", "Print this help and exit.");
        opt.Add("--debug", "Enable all debug features.");
        opt.Add("--debug-ctx", "Enable debug rendering context and debug output.");
        opt.Add("--debug-log", "Enable debug logging.");
        opt.Add("--debug-draw", "Enable debug drawing.");
        opt.Add("--debug-font", "Set debug font for debug messages.", std::string(""));
        opt.Add("--debug-show-fps", "Show FPS counter and stats. You'll need to use --debug-font.");
        opt.Add("--debug-show-msg", "Show debug messages. You'll need to use --debug-font.");
        opt.Add("--debug-print-fps", "Print FPS counter and stats to log.");
        opt.Add("--trace-file", "Record engine function call trace and timing info into a file.", std::string("trace.txt"));
        opt.Add("--trace-start", "Start tracing immediately on application start. (requires --trace-file).");
        opt.Add("--trace-jank", "Try to detect and trace jank frames only.");
        opt.Add("--jank-factor", "The 'jank frame' time scaling factor. (time > avg * factor => 'jank')", jank_factor);
        opt.Add("--report-jank", "Report janky frames to log.");
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
        if (opt.WasGiven("--trace-file"))
        {
            trace_file = opt.GetValue<std::string>("--trace-file");
        }
        if (opt.WasGiven("--vsync"))
        {
            vsync_override = opt.GetValue<bool>("--vsync");
        }

        jank_factor = opt.GetValue<float>("--jank-factor");
        trace_jank  = opt.WasGiven("--trace-jank");
        report_jank = opt.WasGiven("--report-jank");

        if (opt.WasGiven("--debug"))
        {
            debug_log_override    = true;
            debug_context         = true;
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
            debug_context         = opt.WasGiven("--debug-ctx");
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
        const char* mandatory_blocks[] = {
            "application", "window", "config"
        };
        for (const auto* block : mandatory_blocks)
        {
            if (!json.contains(block))
            {
                std::fprintf(stderr, "Config file is missing mandatory object. [object='%s']", block);
                return EXIT_FAILURE;
            }
        }

        bool global_log_debug = true;
        bool global_log_warn  = true;
        bool global_log_info  = true;
        bool global_log_error = true;
        if (json.contains("logging"))
        {
            base::JsonReadSafe(json["logging"], "debug", &global_log_debug);
            base::JsonReadSafe(json["logging"], "warn", &global_log_warn);
            base::JsonReadSafe(json["logging"], "info", &global_log_info);
            base::JsonReadSafe(json["logging"], "error", &global_log_error);
        }

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
        INFO("Ensisoft DETONATOR 2D");
        INFO("Copyright (c) 2010-2023 Sami Vaisanen");
        INFO("http://www.ensisoft.com");
        INFO("http://github.com/ensisoft/detonator");
        INFO("Built on branch '%1' with commit %2", git_Branch(), git_CommitSHA1());

        // in order to have nesting and multiple things controlling the tracing
        // we have a tracing counter. Any time the counter has a value > 0 tracing
        // is enabled. Every request to disable the tracing decrements the counter
        // and when counter drops to 0 the tracing is disabled.
        unsigned trace_enabled_counter = 0;

        std::unique_ptr<base::TraceWriter> trace_writer;
        std::unique_ptr<base::TraceLog> trace_logger;
        if (opt.WasGiven("--trace-file"))
        {
            if (base::EndsWith(trace_file, ".json"))
                trace_writer.reset(new base::ChromiumTraceJsonWriter(trace_file));
            else trace_writer.reset(new base::TextFileTraceWriter(trace_file));
            trace_logger.reset(new base::TraceLog(1000));

            if (opt.WasGiven("--trace-start"))
            {
                trace_enabled_counter = 1;
            }
            else
            {
                WARN("Tracing is enabled but not started.");
                WARN("Use --trace-start to start immediately.");
                WARN("Or start tracing in the game with Game:EnableTracing(true).");
            }
            base::SetThreadTrace(trace_logger.get());
            base::EnableTracing(trace_enabled_counter == 1);
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
            DEBUG("Content package: '%1'", content);
            DEBUG("Content path: '%1']", content_path);
            DEBUG("Content file: '%1']", content_file);

            data::JsonFile content_json_file;
            const auto [success, error_string] = content_json_file.Load(content_file);
            if (!success)
            {
                ERROR("Failed to load game content from file. [file='%1', error='%2']", content_file, error_string);
                return EXIT_FAILURE;
            }
            const auto& content_json = content_json_file.GetRootObject();
            if (!loaders.ContentLoader->LoadClasses(content_json))
                return EXIT_FAILURE;

            engine::FileResourceLoader::DefaultAudioIOStrategy audio_io_strategy;
            if (base::JsonReadSafe(json["desktop"], "audio_io_strategy", &audio_io_strategy))
                loaders.ResourceLoader->SetDefaultAudioIOStrategy(audio_io_strategy);

            loaders.ResourceLoader->LoadResourceLoadingInfo(content_json);
            loaders.ResourceLoader->SetApplicationPath(application_path);
            loaders.ResourceLoader->SetContentPath(content_path);
            loaders.ResourceLoader->PreloadFiles();
        }

        // Create the app instance.
        std::unique_ptr<engine::Engine> engine(GameLibCreateEngine());
        if (!engine->ParseArgs(argc, (const char**)argv))
            return 0;

        engine->SetDebugOptions(debug);

        engine::Engine::Environment env;
        env.classlib         = loaders.ContentLoader.get();
        env.graphics_loader  = loaders.ResourceLoader.get();
        env.engine_loader    = loaders.ResourceLoader.get();
        env.audio_loader     = loaders.ResourceLoader.get();
        env.game_loader      = loaders.ResourceLoader.get();
        env.directory        = GetPath();
        env.user_home        = DiscoverUserHome();
        env.game_home        = GenerateGameHome(env.user_home, identifier);
        engine->SetEnvironment(env);

        wdk::Config::Attributes attrs;
        attrs.surfaces.window = true;
        attrs.double_buffer   = true;
        attrs.srgb_buffer     = true;
        bool srgb_buffer      = true;
        base::JsonReadSafe(json["config"], "red_size", &attrs.red_size);
        base::JsonReadSafe(json["config"], "green_size", &attrs.green_size);
        base::JsonReadSafe(json["config"], "blue_size", &attrs.blue_size);
        base::JsonReadSafe(json["config"], "alpha_size", &attrs.alpha_size);
        base::JsonReadSafe(json["config"], "stencil_size", &attrs.stencil_size);
        base::JsonReadSafe(json["config"], "depth_size", &attrs.depth_size);
        base::JsonReadSafe(json["config"], "sampling", &attrs.sampling);
        base::JsonReadSafe(json["config"], "srgb", &srgb_buffer);
        attrs.srgb_buffer = srgb_buffer;

        DEBUG("OpenGL Config:");
        DEBUG("Red: %1, Green: %2, Blue: %3, Alpha: %4, Stencil: %5, Depth: %6",
            attrs.red_size, attrs.green_size, attrs.blue_size, attrs.alpha_size,
            attrs.stencil_size, attrs.depth_size);
        DEBUG("Sampling: %1", attrs.sampling);

        auto context = std::make_shared<WindowContext>(attrs, debug_context);

        unsigned window_width  = 0;
        unsigned window_height = 0;
        bool window_can_resize = true;
        bool window_has_border = true;
        bool window_set_fullscreen = false;
        bool window_vsync = false;
        bool window_show_cursor = false;
        bool window_grab_mouse  = false;
        bool window_save_geometry = false;

        base::JsonReadSafe(json["window"], "width", &window_width);
        base::JsonReadSafe(json["window"], "height", &window_height);
        base::JsonReadSafe(json["window"], "can_resize", &window_can_resize);
        base::JsonReadSafe(json["window"], "has_border", &window_has_border);
        base::JsonReadSafe(json["window"], "set_fullscreen", &window_set_fullscreen);
        base::JsonReadSafe(json["window"], "vsync", &window_vsync);
        base::JsonReadSafe(json["window"], "cursor", &window_show_cursor);
        base::JsonReadSafe(json["window"], "grab_mouse", &window_grab_mouse);
        base::JsonReadSafe(json["window"], "save_geometry", &window_save_geometry);

        engine::Engine::EngineConfig config;
        config.ticks_per_second   = 1.0f;
        config.updates_per_second = 60.0f;

        if (json.contains("physics"))
        {
            const auto& physics_settings = json["physics"];
            base::JsonReadSafe(physics_settings, "enabled", &config.physics.enabled);
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
            base::JsonReadSafe(audio, "pcm_caching", &config.audio.enable_pcm_caching);
        }
        const auto& window_state_file = base::JoinPath(env.game_home, "_app_state.json");

        if (vsync_override.has_value())
            window_vsync = vsync_override.value();

        wdk::Window window;

        // Create application window
        window.Create(title, window_width, window_height, context->GetVisualID(),
            window_can_resize, window_has_border, true);
        window.ShowCursor(window_show_cursor);
        window.GrabMouse(window_grab_mouse);

        // if we have previous saved geometry then restore the window
        // based on that geometry. we're always starting in the windowed
        // mode for the loading/splash screen and then transition
        // to fullscreen mode later if fullscreen mode is enabled.
        if (window_save_geometry)
        {
            if (base::FileExists(window_state_file))
            {
                LoadWindowState(window_state_file, window);
            }
            else CenterWindowOnScreen(window);
        }
        else CenterWindowOnScreen(window);

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
        engine->SetEngineConfig(config);
        engine->SetTracer(trace_logger.get());

        // do pre-load / splash screen content load
        {
            engine::Engine::LoadingScreenSettings settings;

            if (json.contains("loading_screen"))
            {
                const auto& splash = json["loading_screen"];
                base::JsonReadSafe(splash, "font", &settings.font_uri);
            }

            auto screen = engine->CreateLoadingScreen(settings);

            const auto& classes = loaders.ContentLoader->ListClasses();
            for (size_t i=0; i<classes.size(); ++i)
            {
                DEBUG("Loading %1 class. [name='%2', id=%3]", classes[i].type, classes[i].name, classes[i].id);

                engine::Engine::ContentClass klass;
                klass.type = classes[i].type;
                klass.name = classes[i].name;
                klass.id   = classes[i].id;
                engine->PreloadClass(klass, i, classes.size()-1, screen.get());

                wdk::native_event_t event;
                while (wdk::PeekEvent(event))
                {
                    window.ProcessEvent(event);
                }
            }
            DEBUG("Class loading done!");
        }

        if (window_set_fullscreen)
        {
            window.SetFullscreen(window_set_fullscreen);

            ProcessEvents(window);

            engine->OnRenderingSurfaceResized(window.GetSurfaceWidth(),
                                              window.GetSurfaceHeight());
        }

        engine->Load();
        engine->Start();

        engine->SetTracingOn(trace_enabled_counter > 0);

        // Connect the engine's window event listener to the window.
        if (auto* listener = engine->GetWindowListener())
            wdk::Connect(window, *listener);

        bool quit = false;
        // initialize to false, so that if the window was requested to go into full screen
        // after being created we'll already do the state transition properly and
        // invoke the application handlers.
        bool fullscreen = false;

        std::vector<bool> enable_tracing;

        auto frames_total  = 0u;
        auto frame_count   = 0u;
        auto frame_seconds = 0.0;

        // keep a running average over the last N iterations of the
        // game loop in order to try to detect some anomalies, i.e. janky
        // i.e. "shit frames".
        std::size_t iteration_index = 0;
        std::size_t iteration_counter = 0;
        std::vector<double> iteration_times;
        iteration_times.resize(10);
        // running / current iteration time average based on the values
        // in the iteration_times vector.
        double iteration_time_sum = 0.0;
        double iteration_time_avg = 0.0;

        ElapsedSeconds<TimeId::LoopTime>();

        while (engine->IsRunning() && !quit)
        {
            // Remember that the tracing state cannot be changed while the
            // tracing stack has entries. I.e. the state can only change before
            // any tracing statements are ever entered on the trace stack!
            if (!enable_tracing.empty())
            {
                // We might have received multiple application requests to change the
                // tracing state, i.e. nested calls. Therefore we must queue them and
                // then process in batch while keeping count of what the final tracer
                // state will be!
                for (bool on_off: enable_tracing)
                {
                    if (on_off)
                        ++trace_enabled_counter;
                    else if (trace_enabled_counter)
                        --trace_enabled_counter;
                    else WARN("Incorrect number of tracing enable/disable requests detected.");
                }
                enable_tracing.clear();
                const auto enabled = trace_enabled_counter > 0;
                DEBUG("Performance tracing update. [value=%1]", enabled ? "ON" : "OFF");
                base::EnableTracing(enabled);
                engine->SetTracingOn(enabled);
            }

            TRACE_START();
            TRACE_ENTER(Frame);

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
                else if (auto* ptr = std::get_if<engine::Engine::EnableTracing>(&request))
                    enable_tracing.push_back(ptr->enabled);
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
            const auto time_step = ElapsedSeconds<TimeId::GameTime>();
            const auto wall_time = CurrentRuntime();

            // ask the application to take its simulation steps.
            TRACE_CALL("Engine::Update", engine->Update(time_step));

            // ask the application to draw the current frame.
            TRACE_CALL("Engine::Draw", engine->Draw());

            // indicate end of iteration.
            TRACE_CALL("Engine::EndMainLoop", engine->EndMainLoop());
            TRACE_LEAVE(Frame);

            const auto loop_time_now = ElapsedSeconds<TimeId::LoopTime>();
            const auto loop_time_old = iteration_times[iteration_index];

            iteration_time_sum += loop_time_now;
            iteration_time_sum -= loop_time_old;
            iteration_times[iteration_index] = loop_time_now;
            iteration_index = (iteration_index + 1) % 10;
            iteration_counter++;

            // how should this work? take the median and standard deviation
            // and consider jank when it's some STD away from the median?
            // use an absolute value?
            // relative value? percentage ?
            const bool likely_jank_frame = (iteration_counter >= iteration_times.size()) &&
                                           (loop_time_now > (iteration_time_avg * jank_factor));
            if (likely_jank_frame && report_jank)
            {
                WARN("Likely bad frame detected. Time %1ms vs %2ms avg.",
                     unsigned(loop_time_now * 1000.0), unsigned(iteration_time_avg * 1000.0));
                if (trace_logger) {
                    trace_logger->RenameBlock("BadFrame", 0);
                }
            }
            iteration_time_avg = iteration_time_sum / (double)iteration_times.size();

            if (trace_logger)
            {
                if (trace_jank && likely_jank_frame)
                    trace_logger->Write(*trace_writer.get());
                else if (!trace_jank)
                    trace_logger->Write(*trace_writer.get());
            }

            // do some simple statistics bookkeeping to measure the current
            // FPS based on the number of frames over the past second
            ++frames_total;
            ++frame_count;
            frame_seconds += time_step;
            if (frame_seconds >= 1.0)
            {
                const auto fps = frame_count / frame_seconds;
                engine::Engine::HostStats stats;
                stats.current_fps         = fps;
                stats.num_frames_rendered = frames_total;
                stats.total_wall_time     = wall_time;
                engine->SetHostStats(stats);

                frame_count = 0;
                frame_seconds = 0.0f;
                //DEBUG("Game loop average %1ms", unsigned(iteration_time_avg * 1000.0));
            }

        } // main loop

        engine->SetTracer(nullptr);

        engine->Stop();
        engine->Save();
        engine->Shutdown();
        engine.reset();

        context->Dispose();

        wdk::Disconnect(window);

        if (window.IsFullscreen())
        {
            window.SetFullscreen(false);

            ProcessEvents(window);
        }

        if (window_save_geometry)
        {
            SaveWindowState(window_state_file, window);
        }

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
