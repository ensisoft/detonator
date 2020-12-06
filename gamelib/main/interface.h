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

#pragma once

#include <memory>
#include <variant>
#include <queue>

#include "base/platform.h"
#include "base/logging.h"
#include "gamelib/classlib.h"
#include "graphics/device.h"
#include "graphics/resource.h"
#include "wdk/events.h"
#include "wdk/window_listener.h"

// this header defines the interface between a app/game that
// is built into a shared object (.dll or .so) and the runner/main
// application that loads the shared object.
// The library needs to implement the MakeApp function and return
// a new app instance.
// The host application will invoke methods on the app as appropriate
// and setup the environment specific resources such as the rendering
// context an device and resource loader.

namespace game
{
    // Main callback interface for implementing application specific
    // functionality at certain times during the lifetime of the app.
    // A game/app specific implementation of this interface is created
    // by the library level function MakeApp. Then the host (the callee)
    // application will start invoking the interface methods. Some methods
    // are only called once per lifetime while some are called repeatedly.
    // The idea is that using this application "template" it'd be simple
    // to write a new application typical to this "framework" (i.e. a simple
    // game).
    class App
    {
    public:
        virtual ~App() = default;

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
        // mode or back into windowed mode.
        struct SetFullscreen {
            // Request the window to be to put into full-screen mode
            // when true, or back to window mode when false.
            bool fullscreen = false;
        };
        // Request to toggle the current window full-screen mode.
        struct ToggleFullscreen {};

        // Request to quit.
        struct QuitApp {};

        // Union of possible window requests.
        using Request = std::variant<
            ResizeWindow,
            MoveWindow,
            SetFullscreen,
            ToggleFullscreen,
            QuitApp>;

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

        // Parameters pertaining to the environment of the application.
        struct Environment {
            // Interface for accessing resources (content) implemented
            // by the graphics subsystem, i.e. drawable shapes and materials.
            game::ClassLibrary* classlib = nullptr;
        };

        // Called whenever there are changes to the current environment
        // of the application. The provided environment object contains
        // a collection of interface objects for accessing the game
        // content at various levels from file based access (i.e. things
        // such as textures, fonts, shaders) to more derived resources
        // i.e. materials and drawable shapes to high level game assets
        // such as animations.
        // The pointers will remain valid until the next call of
        // SetEnvironment.
        virtual void SetEnvironment(const Environment& env)
        {}

        // Called once on application startup. The arguments
        // are the arguments given to the application the command line
        // when the process is started. If false is returned
        // it is used to indicate that there was a problem applying
        // the arguments and the application should not continue.
        virtual bool ParseArgs(int argc, const char* argv[])
        { return true; }

        // Initialize the application and it's graphics resources.
        // The context is the current rendering context that can be used
        // to create the graphics device(s).
        // Surface width and height are the current rendering surface
        // (could be window, could be an off-screen buffer) sizes.
        virtual void Init(gfx::Device::Context* context,
            unsigned surface_width, unsigned surface_height) {}

        // Load the game and its data and/or previous state.
        // Called once before entering the main game update/render loop.
        virtual void Load() {}

        // Start the application. This is called once before entering the
        // main game update/render loop.
        virtual void Start() {}

        // Draw the next frame.
        virtual void Draw() {}

        // Tick the application. Invoked on regular interval. Current time
        // is the total current accumulated application time.
        virtual void Tick(double current_time)  {}

        // Update the application. Time is current total application time
        // and dt is the time step in seconds.
        virtual void Update(double current_time, double dt) {}

        // Save the game and its current state.
        // Called once after leaving the main game update/render loop.
        virtual void Save() {}

        // Shutdown the application. Called once after leaving the
        // main game update/render loop.
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

        // Some collected statistics of the current application
        // and it's runtime.
        struct Stats {
            // The current frames per second.
            float current_fps = 0.0f;
            // The total time the application has been running.
            double total_wall_time = 0.0f;
            // The total accumulated *game* time.
            double total_game_time = 0.0f;
            // The total number of frames rendered.
            unsigned num_frames_rendered = 0;
        };
        // Update the collected runtime statistics. This is called
        // approximately once per second.
        virtual void UpdateStats(const Stats& stats) {}

        // Called when the primary rendering surface in which the application
        // renders for display has been resized. Note that this may not be
        // the same as the current window and its size if an off-screen rendering
        // is being done!
        // This is called once on application startup and then every time when
        // the rendering surface size changes.
        virtual void OnRenderingSurfaceResized(unsigned width, unsigned height) {}

    protected:
    private:
    };

    // Utility/helper class to manage application requests.
    class AppRequestQueue
    {
    public:
        using Request = game::App::Request;

        inline bool GetNext(Request* out)
        {
            if (mQueue.empty())
                return false;
            *out = mQueue.front();
            mQueue.pop();
            return true;
        }
        inline void MoveWindow(int x, int y)
        { mQueue.push(game::App::MoveWindow { x, y }); }
        inline void ResizeWindow(unsigned width, unsigned height)
        { mQueue.push(game::App::ResizeWindow{width, height}); }
        inline void SetFullscreen(bool fullscreen)
        { mQueue.push(game::App::SetFullscreen{fullscreen}); }
        inline void ToggleFullscreen()
        { mQueue.push(game::App::ToggleFullscreen{}); }
        inline void Quit()
        { mQueue.push(game::App::QuitApp{}); }
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
    GAMESTUDIO_API game::App* MakeApp();
} // extern "C"

typedef game::App* (*MakeAppFunc)();

// The below interface only exists currently for simplifying
// the structure of the builds. I.e the dependencies for creating
// environment objects (such as ContentLoader) can be wrapped inside
// the game library itself and this lets the loader application to
// remain free of these dependencies.
// This is currently only an implementation detail and this mechanism
// might go away. However currently we provide this helper that will
// do the wrapping and then expect the game libs to include the right
// translation unit in their builds.
extern "C" {
    GAMESTUDIO_API void CreateDefaultEnvironment(game::ClassLibrary** classlib);
    GAMESTUDIO_API void DestroyDefaultEnvironment(game::ClassLibrary* classlib);
    GAMESTUDIO_API void SetResourceLoader(gfx::ResourceLoader* loader);
    GAMESTUDIO_API void SetGlobalLogger(base::Logger* logger);
} // extern "C"

typedef void (*CreateDefaultEnvironmentFunc)(game::ClassLibrary**);
typedef void (*DestroyDefaultEnvironmentFunc)(game::ClassLibrary*);
typedef void (*SetResourceLoaderFunc)(gfx::ResourceLoader*);
typedef void (*SetGlobalLoggerFunc)(base::Logger*);
