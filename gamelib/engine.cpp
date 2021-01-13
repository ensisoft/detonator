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

// this translation unit implements the gamelib library interface.
#define GAMESTUDIO_GAMELIB_IMPLEMENTATION

#include "warnpush.h"
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <cstring>
#include <cmath>

#include "gamelib/engine.h"
#include "base/logging.h"
#include "base/format.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/resource.h"
#include "gamelib/animation.h"
#include "gamelib/classlib.h"
#include "gamelib/renderer.h"
#include "gamelib/entity.h"
#include "gamelib/physics.h"
#include "gamelib/game.h"
#include "gamelib/lua.h"
#include "gamelib/main/interface.h"

namespace
{

// Default game engine implementation. Implements the main App interface
// which is the interface that enables the game host to communicate
// with the application/game implementation in order to update/tick/etc.
// the game and also to handle input from keyboard and mouse.
class DefaultGameEngine : public game::App,
                          public wdk::WindowListener
{
public:
    virtual bool ParseArgs(int argc, const char* argv[]) override
    {
        DEBUG("Parsing cmd line args.");

        for (int i = 1; i < argc; ++i)
        {
            if (!std::strcmp("--debug", argv[i]))
            {
                mDebugLogging = true;
                mDebugDrawing = true;
            }
            else if (!std::strcmp("--debug-log", argv[i]))
                mDebugLogging = true;
            else if (!std::strcmp("--debug-draw", argv[i]))
                mDebugDrawing = true;
        }
        base::EnableDebugLog(mDebugLogging);
        INFO("Debug logging is '%1'", mDebugLogging ? "ON" : "OFF");
        INFO("Debug drawing is '%1'", mDebugDrawing ? "ON" : "OFF");
        return true;
    }
    virtual bool GetNextRequest(Request* out) override
    { return mRequests.GetNext(out); }

    // Application implementation
    virtual void Start()
    {
        DEBUG("Engine starting.");
        mGame->LoadGame(mClasslib);
    }
    virtual void Init(gfx::Device::Context* context, unsigned surface_width, unsigned surface_height) override
    {
        DEBUG("Engine initializing. Surface %1x%2", surface_width, surface_height);
        mDevice  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, context);
        mPainter = gfx::Painter::Create(mDevice);
        mPainter->SetSurfaceSize(surface_width, surface_height);
        mSurfaceWidth  = surface_width;
        mSurfaceHeight = surface_height;
        mGame = std::make_unique<game::LuaGame>(mDirectory + "/lua");
        mGame->SetPhysicsEngine(&mPhysics);
    }
    virtual void SetEnvironment(const Environment& env) override
    {
        mClasslib  = env.classlib;
        mDirectory = env.directory;
        mRenderer.SetLoader(mClasslib);
        mPhysics.SetLoader(mClasslib);
    }
    virtual void SetEngineConfig(const EngineConfig& conf) override
    {
        mPhysics.SetScale(conf.physics.scale);
        mPhysics.SetGravity(conf.physics.gravity);
        mPhysics.SetNumPositionIterations(conf.physics.num_position_iterations);
        mPhysics.SetNumVelocityIterations(conf.physics.num_velocity_iterations);
        mPhysics.SetTimestep(1.0f / conf.updates_per_second);
        mDevice->SetDefaultTextureFilter(conf.default_min_filter);
        mDevice->SetDefaultTextureFilter(conf.default_mag_filter);
    }

    virtual void Draw() override
    {
        mDevice->BeginFrame();
        mDevice->ClearColor(gfx::Color4f(0.2, 0.3, 0.4, 1.0f));
        mPainter->SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
        mPainter->SetTopLeftView(mSurfaceWidth, mSurfaceHeight);
        mRenderer.BeginFrame();

        if (mScene)
        {
            gfx::Transform transform;
            transform.MoveTo(mSurfaceWidth * 0.5, mSurfaceHeight * 0.5);
            mRenderer.Draw(*mScene, *mPainter, transform);
            mRenderer.EndFrame();

            if (mDebugDrawing && mPhysics.HaveWorld())
            {
                mPhysics.DebugDrawObjects(*mPainter, transform);
            }
        }

        mDevice->EndFrame(true);
        mDevice->CleanGarbage(120);
    }

    virtual void Tick(double time) override
    {
        mGame->Tick(time);
    }

    virtual void Update(double time, double dt) override
    {
        // process the game actions.
        game::Game::Action action;
        while (mGame->GetNextAction(&action))
        {
            if (auto* ptr = std::get_if<game::Game::PlaySceneAction>(&action))
                LoadScene(ptr->klass);
        }

        if (!mScene)
            return;

        mScene->Update(dt);
        mGame->Update(time, dt);

        if (mPhysics.HaveWorld())
        {
            mPhysics.Tick();
            mPhysics.UpdateScene(*mScene);
        }
        mRenderer.Update(*mScene, time, dt);
    }
    virtual void Shutdown() override
    {
        DEBUG("Engine shutdown");
        mDevice.reset();
    }
    virtual bool IsRunning() const override
    { return mRunning; }
    virtual wdk::WindowListener* GetWindowListener() override
    { return this; }

    virtual void UpdateStats(const Stats& stats) override
    {
        //DEBUG("fps: %1, wall_time: %2, game_time: %3, frames: %4",
        //      stats.current_fps, stats.total_wall_time, stats.total_game_time, stats.num_frames_rendered);
    }
    virtual void OnRenderingSurfaceResized(unsigned width, unsigned height)
    {
        DEBUG("Rendering surface resized to %1x%2", width, height);
        mSurfaceWidth = width;
        mSurfaceHeight = height;
        mPainter->SetSurfaceSize(width, height);
    }

    // WindowListener
    virtual void OnWantClose(const wdk::WindowEventWantClose&) override
    {
        // todo: handle ending play, saving game etc.
        mRunning = false;
    }
    virtual void OnKeydown(const wdk::WindowEventKeydown& key) override
    {
        if (key.symbol == wdk::Keysym::KeyS &&
            key.modifiers.test(wdk::Keymod::Control) &&
            key.modifiers.test(wdk::Keymod::Shift))
        {
            TakeScreenshot();
        }
        //DEBUG("OnKeyDown: %1, %2", ModifierString(key.modifiers), magic_enum::enum_name(key.symbol));
        mGame->OnKeyDown(key);
    }
    virtual void OnKeyup(const wdk::WindowEventKeyup& key) override
    {
        //DEBUG("OnKeyUp: %1, %2", ModifierString(key.modifiers), magic_enum::enum_name(key.symbol));
        mGame->OnKeyUp(key);
    }
    virtual void OnChar(const wdk::WindowEventChar& text) override
    {
        mGame->OnChar(text);
    }
    virtual void OnMouseMove(const wdk::WindowEventMouseMove& mouse) override
    {
        mGame->OnMouseMove(mouse);
    }
    virtual void OnMousePress(const wdk::WindowEventMousePress& mouse) override
    {
        mGame->OnMousePress(mouse);
    }
    virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse) override
    {
        mGame->OnMouseRelease(mouse);
    }
private:
    void LoadScene(game::ClassHandle<game::SceneClass> klass)
    {
        if (mScene)
        {
            mGame->EndPlay();
            mPhysics.DeleteAll();
            mScene.reset();
        }
        mScene = game::CreateSceneInstance(klass);
        mPhysics.CreateWorld(*mScene);
        mGame->BeginPlay(mScene.get());
    }

    static std::string ModifierString(wdk::bitflag<wdk::Keymod> mods)
    {
        std::string ret;
        if (mods.test(wdk::Keymod::Control))
            ret += "Ctrl+";
        if (mods.test(wdk::Keymod::Shift))
            ret += "Shift+";
        if (mods.test(wdk::Keymod::Alt))
            ret += "Alt+";
        if (!ret.empty())
            ret.pop_back();
        return ret;
    }
    void TakeScreenshot()
    {
        const auto& rgba = mDevice->ReadColorBuffer(mSurfaceWidth, mSurfaceHeight);
        gfx::WritePNG(rgba, "screenshot.png");
        INFO("Wrote screenshot.png");
    }
private:
    unsigned mSurfaceWidth  = 0;
    unsigned mSurfaceHeight = 0;
    // game dir where the executable is.
    std::string mDirectory;
    // queue of outgoing requests regarding the environment
    // such as the window size/position etc that the game host
    // may/may not support.
    game::AppRequestQueue mRequests;
    // Interface for accessing the game content and resources
    // such as animations, materials etc.
    game::ClassLibrary* mClasslib = nullptr;
    // The graphics painter device.
    std::unique_ptr<gfx::Painter> mPainter;
    // The graphics device.
    std::shared_ptr<gfx::Device> mDevice;
    // The rendering subsystem.
    game::Renderer mRenderer;
    // The physics subsystem.
    game::PhysicsEngine mPhysics;
    // Current scene or nullptr if no scene.
    std::unique_ptr<game::Scene> mScene;
    // Game logic implementation.
    std::unique_ptr<game::Game> mGame;
    // flag to indicate whether the app is still running or not.
    bool mRunning = true;
    // flag for debug level logging.
    bool mDebugLogging = false;
    // flag for debug drawing.
    bool mDebugDrawing = false;
};

} //namespace

extern "C" {
GAMESTUDIO_EXPORT game::App* MakeApp()
{
    DEBUG("Engine");
    return new DefaultGameEngine;
}
} // extern C
