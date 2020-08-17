// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft
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
#include "warnpop.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
#include <filesystem>

#include "audio/player.h"
#include "audio/device.h"
#include "base/logging.h"
#include "gamelib/loader.h"
#include "graphics/device.h"
#include "graphics/painter.h"
#include "misc/homedir.h"
#include "misc/settings.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"

#if defined(LINUX_OS)
#  include <fenv.h>
#endif
#include "gamewidget.h"

namespace invaders {

    audio::AudioPlayer* g_audio;
    game::ResourceLoader* g_loader;

} // invaders

int game_main(int argc, char* argv[])
{
#if defined(_NDEBUG)
    bool debugLog = true;
#else
    bool debugLog = false;
#endif
    for (int i=1; i<argc; ++i)
    {
        const std::string a(argv[i]);
        if (a == "--debug-log")
            debugLog = true;
    }

    base::CursesLogger logger;
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(debugLog);

    DEBUG("It's alive!");
    INFO("%1 %2", GAME_TITLE, GAME_VERSION);
    INFO("Copyright (c) 2010-2018 Sami Vaisanen");
    INFO("http://www.ensisoft.com");
    INFO("http://github.com/ensisoft/pinyin-invaders");

    game::ResourceLoader loader;
    invaders::g_loader = &loader;

    // todo: resolve the directory where the executable is.
    // currently expects the current working directory to contain the content.json file
    invaders::g_loader->LoadResources(".", "content.json");
    gfx::SetResourceLoader(invaders::g_loader);
    audio::AudioPlayer audio_player(audio::AudioDevice::Create(GAME_TITLE));
    invaders::g_audio = &audio_player;

    misc::HomeDir::Initialize(".pinyin-invaders");
    misc::Settings settings;

    if (base::FileExists(misc::HomeDir::MapFile("settings.json")))
        settings.LoadFromFile(misc::HomeDir::MapFile("settings.json"));

    DEBUG("Initialize OpenGL");
    // context integration glue code that puts together
    // wdk::Context and gfx::Device
    class WindowContext : public gfx::Device::Context
    {
    public:
        WindowContext()
        {
            wdk::Config::Attributes attrs;
            attrs.red_size  = 8;
            attrs.green_size = 8;
            attrs.blue_size = 8;
            attrs.alpha_size = 8;
            attrs.stencil_size = 8;
            attrs.surfaces.window = true;
            attrs.double_buffer = true;
            attrs.sampling = wdk::Config::Multisampling::MSAA4;
            attrs.srgb_buffer = true;

            mConfig   = std::make_unique<wdk::Config>(attrs);
            mContext  = std::make_unique<wdk::Context>(*mConfig, 2, 0,  false, //debug
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
    private:
        std::unique_ptr<wdk::Context> mContext;
        std::unique_ptr<wdk::Surface> mSurface;
        std::unique_ptr<wdk::Config>  mConfig;
        wdk::uint_t mVisualID = 0;
    };

    // Create context and rendering window.
    auto context = std::make_shared<WindowContext>();
    auto window  = std::make_unique<wdk::Window>();
    window->Create(GAME_TITLE " " GAME_VERSION, 1024, 768, context->GetVisualID());
    context->SetWindowSurface(*window);

    // create graphics device and painter
    auto device  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, context);
    auto painter = gfx::Painter::Create(device);

    invaders::GameWidget game(*window);
    game.initArgs(argc, argv);
    game.load(settings);
    game.launch();

    // connect window events to the game
    wdk::Connect(*window, game);

    using clock = std::chrono::high_resolution_clock;

    auto start = clock::now();

    while (game.isRunning())
    {
        const auto end  = clock::now();
        const auto gone = end - start;
        const auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(gone).count();

        game.updateGame(ms);
        game.renderGame(*device, *painter);

        static unsigned frames  = 0;
        static unsigned runtime = 0;
        frames++;
        runtime += ms;
        if (runtime >= 1000)
        {
            const float fps = frames / (runtime / 1000.0f);
            game.setFps(fps);
            runtime = 0;
            frames  = 0;
        }

        wdk::native_event_t event;
        while (wdk::PeekEvent(event))
        {
            window->ProcessEvent(event);
        }

        start = end;
    }

    game.save(settings);

    settings.SaveToFile(misc::HomeDir::MapFile("settings.json"));

    context->Dispose();
    window->Destroy();
    return 0;
}

int main(int argc, char* argv[])
{
#if defined(LINUX_OS)
    // SIGFPE on floating point exception
    feenableexcept(FE_INVALID   |
                   FE_DIVBYZERO |
                   FE_OVERFLOW  |
                   FE_UNDERFLOW);
    DEBUG("Enabled floating point exceptions");
#endif


    try
    {
        return game_main(argc, argv);
    }
    catch (const std::exception& bollocks)
    {
        std::cerr << "Oops.. there was a problem: " << bollocks.what();
    }
    return 1;
}
