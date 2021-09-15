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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>
#include <variant>
#include <string>
#include <queue>

#include "base/platform.h"
#include "base/logging.h"
#include "audio/loader.h"
#include "audio/format.h"
#include "engine/classlib.h"
#include "engine/loader.h"
#include "engine/color.h"
#include "graphics/device.h"
#include "graphics/loader.h"
#include "wdk/events.h"
#include "wdk/listener.h"

// this header defines the interface between the main application and the
// actual game engine that is built into a shared library (.dll or .so).
// The library needs to implement the CreateEngine function and return
// a new engine instance.
// The host application will provide for the engine the environment specific
// resources such as the rendering context, resource loaders etc.
// Once the engine has been created the host application will enter the
// main loop and start calling the engine functions to update, draw etc.

namespace engine
{
    // The engine interface provides an abstract interface and a binary
    // firewall for separating the actual game engine implementation from
    // the host application and its environment. The host application will
    // be responsible for creating resources such as windows, rendering context
    // etc and providing those to the engine. The engine will then respond
    // to the events coming from the host application and also perform the
    // normal game activities such as updating state, drawing etc.
    class Engine
    {
    public:
        virtual ~Engine() = default;

        // Request to resize the host window to some particular size.
        // The size specifies the *inside* area of the window i.e. the
        // renderable surface dimensions and exclude any window border,
        // caption/title bar and/or decorations.
        struct ResizeWindow {
            // Desired window rendering surface width.
            unsigned width  = 0;
            // Desired window rendering surface height.
            unsigned height = 0;
        };
        // Request to move the window to some particular location relative
        // to the desktop origin.
        struct MoveWindow {
            // The new window X position in the desktop.
            int xpos = 0;
            // The new window Y position in the desktop.
            int ypos = 0;
        };
        // Request to have the window put to the full-screen
        // mode or back into windowed mode. In full-screen mode
        // the window has no borders, no title bar or any kind
        // of window decoration and it covers the whole screen.
        // Note that this is only a *request* which means it's
        // possible that for whatever reason the transition does
        // not take place. (For example the user rejected the request
        // or the the platform doesn't support the concept of
        // full screen windows). In order to understand whether the
        // transition *did* happen the application should listen for
        // OnEnterFullScreen (and OnLeaveFullScreen) events.
        struct SetFullScreen {
            // Request the window to be to put into full-screen mode
            // when true, or back to window mode when false.
            bool fullscreen = false;
        };
        // Request to toggle the current window full-screen mode.
        // See comments in @SetFullScreen about possible limitations.
        struct ToggleFullScreen {};

        // Request to quit.
        struct QuitApp {
            int exit_code = 0;
        };

        struct ShowMouseCursor {
            bool show = true;
        };

        struct GrabMouse {
            bool grab = false;
        };

        // Union of possible window requests.
        using Request = std::variant<
            ResizeWindow,
            MoveWindow,
            SetFullScreen,
            ToggleFullScreen,
            QuitApp,
            GrabMouse,
            ShowMouseCursor>;

        // During the runtime of the application the application may request
        // the host to provide some service. The application may queue such
        // requests and then provide them in the implementation of this API
        // function. The host application will process any such request once
        // per application main loop iteration. If there are no more requests
        // then false should be returned otherwise returning true indicates
        // that a new request was available.
        // There's no actual guarantee that any of these requests are honored,
        // that depends on the host implementation. Therefore they are just that
        // *requests*. The application should not assume that some particular
        // result happens as the result of the request processing.
        virtual bool GetNextRequest(Request* out)
        { return false;}

        // Debugging options that might be set through some interface.
        // It's up to the application whether any of these will be
        // supported in anyway.
        struct DebugOptions {
            bool debug_pause = false;
            bool debug_draw = false;
            bool debug_log  = false;
            bool debug_show_fps = false;
            bool debug_print_fps = false;
            bool debug_show_msg = false;
            std::string debug_font;
        };
        // Set the debug options.
        virtual void SetDebugOptions(const DebugOptions& debug)
        {}

        virtual void DebugPrintString(const std::string& str)
        {}

        // Parameters pertaining to the environment of the application.
        struct Environment {
            // Interface for accessing resource classes such as scenes, entities
            // materials etc.
            engine::ClassLibrary* classlib = nullptr;
            // Interface for accessing game data packaged with the game.
            // Not the data *generated* by the game such as save games.
            engine::Loader* game_data_loader = nullptr;
            // Interface for accessing low level graphics resources such as shaders
            // textures and fonts.
            gfx::Loader* graphics_loader = nullptr;
            // Interface for accessing low level audio resources.
            audio::Loader* audio_loader = nullptr;
            // Path to the top level directory where the app/game is
            // I.e. where the GameMain, config.json, content.json etc. files
            // are. UTF-8 encoded.
            std::string directory;
            // Path to the user's home directory. for example /home/roger/
            // or c:\Documents and Settings\roger
            // UTF-8 encoded.
            std::string user_home;
            // Path to the recommended game data directory for data generated
            // by the game such as save games etc.
            std::string game_home;
        };

        // Set the engine execution environment. Called once in the
        // beginning before entering the main loop.
        virtual void SetEnvironment(const Environment& env)
        {}

        // Configuration for the engine/application
        struct EngineConfig {
            // The default texture minification filter setting.
            gfx::Device::MinFilter default_min_filter = gfx::Device::MinFilter::Bilinear;
            // the default texture magnification filter setting.
            gfx::Device::MagFilter default_mag_filter = gfx::Device::MagFilter::Linear;

            // the current expected number of Update calls per second.
            unsigned updates_per_second = 60;
            // The current expected number of Tick calls per second.
            unsigned ticks_per_second = 1;
            // configuration data for the physics engine.
            struct {
                // number of velocity iterations to take per simulation step.
                unsigned num_velocity_iterations = 8;
                // number of position iterations to take per simulation step.
                unsigned num_position_iterations = 3;
                // gravity vector of the world.
                glm::vec2 gravity = {0.0f, 1.0f};
                // scaling vector for transforming objects from scene world units
                // into physics world units and back.
                // if scale is for example {100.0f, 100.0f} it means 100 scene units
                // to a single physics world unit. 
                glm::vec2 scale = {1.0f, 1.0f};
            } physics;
            struct {
                // initial visibility.
                bool show = true;
                // the cursors shape id
                std::string drawable;
                // the cursors material id.
                std::string material;
                // normalized hotspot of the cursor
                glm::vec2 hotspot = {0.0f, 0.0f};
                // size of the cursor in pixels.
                glm::vec2 size = {20.0f, 20.0f};
            } mouse_cursor;
            struct Audio {
                // PCM sample rate of the audio output.
                unsigned sample_rate = 44100;
                // number of output channels. 1 = monoaural, 2 stereo.
                audio::Channels channels = audio::Channels::Stereo;
                // PCM audio data type.
                audio::SampleType sample_type = audio::SampleType::Float32;
                // Expected approximate audio buffer size in milliseconds.
                unsigned buffer_size = 20;
            } audio;
            // the default clear color.
            Color4f clear_color = {0.2f, 0.3f, 0.4f, 1.0f};
        };
        // Set the game engine configuration. Called once in the beginning
        // before Start is called.
        virtual void SetEngineConfig(const EngineConfig& conf)
        {}

        // Called once on application startup. The arguments
        // are the arguments given to the application the command line
        // when the process is started. If false is returned
        // it is used to indicate that there was a problem applying
        // the arguments and the application should not continue.
        virtual bool ParseArgs(int argc, const char* argv[])
        { return true; }

        struct InitParams {
            // name of the "main" game script for loading the game.
            std::string game_script;
            // application name/title.
            std::string application_name;
            // context is the current rendering context that can be used
            // to create the graphics device(s).
            gfx::Device::Context* context = nullptr;
            // Width (in pixels) of the current rendering surface such as a window
            // or an off-screen buffer.
            unsigned surface_width = 0;
            // Height (in pixels) of the current rendering surface such as a window
            // or an off-screen buffer.
            unsigned surface_height = 0;
        };

        // Initialize the engine and its resources and subsystems
        // such as the graphics and audio.
        virtual void Init(const InitParams& init) {}

        // Load the game and its data and/or previous state.
        // Called once before entering the main game update/render loop.
        // Should return true if successful,otherwise false on error.
        virtual bool Load()
        { return true; }

        // Start the game. This is called once before entering the
        // main game update/render loop.
        virtual void Start() {}

        // Called once at the start of every iteration of the main application
        // loop. The calls to Tick, Draw, Update  are sandwiched between
        // the calls to BeginMainLoop and EndMainLoop which is called at the
        // end of the mainloop as the very last thing.
        virtual void BeginMainLoop() {}
        virtual void EndMainLoop() {};

        // Draw the next frame.
        virtual void Draw() {}

        // Update the application.
        // dt is the current game time step to take. The time step to take
        // is variable and depends on how long previous iteration of the
        // main game loop took.
        virtual void Update(double dt) {}

        // Stop the game. Called once after exiting the main loop and
        // before calling save and shutdown.
        virtual void Stop() {}

        // Save the game and its current state.
        // Called once after leaving the main game update/render loop.
        virtual void Save() {}

        // Shutdown the engine. Called once after leaving the
        // main game update/render loop. Release any resources here.
        virtual void Shutdown() {}

        // Returns true if the application is still running. When
        // this returns false the main loop is exited and the application
        // will then perform shutdown and exit.
        virtual bool IsRunning() const
        { return true; }

        // Get the window listener object that is used to handle
        // the window events coming from the current application window.
        virtual wdk::WindowListener* GetWindowListener()
        { return nullptr; }

        // Statistics collected by the host application (process).
        struct HostStats {
            // The current frames per second.
            float current_fps = 0.0f;
            // The total time the application has been running.
            double total_wall_time = 0.0;
            // The total number of frames rendered.
            unsigned num_frames_rendered = 0;
        };
        // Set the current statistics collected by the host application (process).
        // This is called approximately once per second.
        virtual void SetHostStats(const HostStats& stats) {}

        // Statistics collected/provided by the app/game.
        struct Stats {
            double total_game_time = 0.0;
        };
        // Get the current statistics collected by the app implementation.
        // Returns false if not available.
        virtual bool GetStats(Stats* stats) const { return false; }

        // Ask the engine to take a screenshot of the current default (window)
        // rendering surface and write it out as an image file.
        virtual void TakeScreenshot(const std::string& filename) const {}
        // Called when the primary rendering surface in which the application
        // renders for display has been resized. Note that this may not be
        // the same as the current window and its size if an off-screen rendering
        // is being done!
        // This is called once on application startup and then every time when
        // the rendering surface size changes.
        virtual void OnRenderingSurfaceResized(unsigned width, unsigned height) {}
        // Called when the application enters full screen mode. This can be as a
        // response to the application's request to enter full screen mode or
        // the mode change can be initiated through the host application.
        // Either way the application should not assume anything about the current
        // full screen state unless these 2 callbacks are invoked.
        virtual void OnEnterFullScreen() {}
        // Called when the application leaves the full screen mode and goes back
        // into windowed mode. See the notes in OnEnterFullScreen about how this
        // transition can take place.
        virtual void OnLeaveFullScreen() {}
    protected:
    private:
    };

    // Utility/helper class to manage application requests.
    class AppRequestQueue
    {
    public:
        using Request = engine::Engine::Request;

        inline bool GetNext(Request* out)
        {
            if (mQueue.empty())
                return false;
            *out = mQueue.front();
            mQueue.pop();
            return true;
        }
        inline void MoveWindow(int x, int y)
        { mQueue.push(engine::Engine::MoveWindow {x, y }); }
        inline void ResizeWindow(unsigned width, unsigned height)
        { mQueue.push(engine::Engine::ResizeWindow{width, height}); }
        inline void SetFullScreen(bool fullscreen)
        { mQueue.push(engine::Engine::SetFullScreen{fullscreen}); }
        inline void ToggleFullScreen()
        { mQueue.push(engine::Engine::ToggleFullScreen{}); }
        inline void Quit(int exit_code)
        { mQueue.push(engine::Engine::QuitApp{exit_code }); }
        inline void ShowMouseCursor(bool yes_no)
        { mQueue.push(engine::Engine::ShowMouseCursor {yes_no } ); }
        inline void GrabMouse(bool yes_no)
        { mQueue.push(engine::Engine::GrabMouse {yes_no} ); }
    private:
        std::queue<Request> mQueue;
    };

} // namespace

// Win32 / MSVS export/import declarations
#if defined(__MSVC__)
#  define GAMESTUDIO_EXPORT __declspec(dllexport)
#  define GAMESTUDIO_IMPORT __declspec(dllimport)
#  if defined(GAMESTUDIO_GAMELIB_IMPLEMENTATION)
#    define GAMESTUDIO_API __declspec(dllexport)
#  else 
#    define GAMESTUDIO_API __declspec(dllimport)
#  endif
#else
#  define GAMESTUDIO_EXPORT
#  define GAMESTUDIO_IMPORT
#  define GAMESTUDIO_API
#endif

// Main interface for bootstrapping/loading the game/app
extern "C" {
    // return a new app implementation allocated on the free store.
    GAMESTUDIO_API engine::Engine* Gamestudio_CreateEngine();
} // extern "C"

typedef engine::Engine* (*Gamestudio_CreateEngineFunc)();

// The below interface only exists currently for simplifying
// the structure of the builds. I.e the dependencies for creating
// environment objects (such as ContentLoader) can be wrapped inside
// the game library itself and this lets the loader application to
// remain free of these dependencies.
// This is currently only an implementation detail and this mechanism
// might go away. However currently we provide this helper that will
// do the wrapping and then expect the game libs to include the right
// translation unit in their builds.
struct Gamestudio_Loaders {
    std::unique_ptr<engine::JsonFileClassLoader> ContentLoader;
    std::unique_ptr<engine::FileResourceLoader> ResourceLoader;
};

extern "C" {
    GAMESTUDIO_API void Gamestudio_CreateFileLoaders(Gamestudio_Loaders* out);
    GAMESTUDIO_API void Gamestudio_SetGlobalLogger(base::Logger* logger, bool debug);
} // extern "C"

typedef void (*Gamestudio_CreateFileLoadersFunc)(Gamestudio_Loaders*);
typedef void (*Gamestudio_SetGlobalLoggerFunc)(base::Logger*, bool debug);
