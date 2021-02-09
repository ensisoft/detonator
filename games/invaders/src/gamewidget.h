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

#pragma once

#include "config.h"

#include <list>
#include <map>
#include <memory>
#include <stack>
#include <string>

#include "audio/sample.h"
#include "audio/player.h"
#include "audio/device.h"
#include "engine/main/interface.h"
#include "engine/renderer.h"
#include "graphics/device.h"
#include "graphics/painter.h"
#include "wdk/window_listener.h"

namespace invaders
{
    class Game;
    class Level;

    class GameWidget : public game::App, public wdk::WindowListener
    {
    public:
        // Level info for persisting level data
        struct LevelInfo {
            std::wstring name;
            unsigned highScore = 0;
            bool locked = true;
        };

        // game profile settings, for example "easy", "medium" etc.
        struct Profile {
            std::wstring name;
            float speed = 0.0f;
            unsigned spawnCount = 0;
            unsigned spawnInterval = 0;
            unsigned numEnemies = 0;
        };

        GameWidget();
       ~GameWidget();

        // App implementation
        virtual bool GetNextRequest(Request* out) override
        { return mRequests.GetNext(out); }
        virtual bool ParseArgs(int argc, const char* argv[]) override;
        virtual void SetEnvironment(const Environment& env) override
        { mClassLib = env.classlib; }
        virtual void Init(gfx::Device::Context* context,
            unsigned surface_width, unsigned surface_height) override;
        virtual void Load() override;
        virtual void Start() override;
        virtual void Save() override;
        virtual void Update(double wall_time, double game_time, double dt) override;
        virtual void Draw() override;
        virtual bool IsRunning() const override
        { return mRunning; }
        virtual void OnRenderingSurfaceResized(unsigned width, unsigned height) override
        {
            mRenderWidth  = width;
            mRenderHeight = height;
            mPainter->SetSurfaceSize(width, height);
        }
        virtual void OnEnterFullScreen() override
        { mFullscreen = true; }
        virtual void OnLeaveFullScreen() override
        { mFullscreen = false; }
        virtual wdk::WindowListener* GetWindowListener() override
        { return this; }
        virtual void UpdateStats(const game::App::Stats& stats) override
        { mCurrentfps = stats.current_fps; }

        // Window listener implementation.
        virtual void OnKeydown(const wdk::WindowEventKeydown& key) override;
        virtual void OnWantClose(const wdk::WindowEventWantClose& close) override;
    private:
        void PlayMusic();
        std::unique_ptr<game::Entity> CreateEntityByName(const std::string& name) const;

    private:
        class State;
        class MainMenu;
        class GameHelp;
        class Settings;
        class About;
        class PlayGame;
        class Scoreboard;

        class Animation;
        class Asteroid;
        class Explosion;
        class Smoke;
        class Sparks;
        class Debris;
        class BigExplosion;
        class Score;
        class Invader;
        class Missile;
        class UFO;

    private:
        std::stack<std::unique_ptr<State>> mStates;

        std::map<unsigned, std::unique_ptr<Invader>> mInvaders;
        std::vector<std::unique_ptr<Level>> mLevels;
        std::vector<LevelInfo> mLevelInfos;
        std::vector<Profile> mProfiles;
        std::list<std::unique_ptr<Animation>> mAnimations;
        std::unique_ptr<game::Entity> mBackground;

        std::unique_ptr<Game> mGame;
        unsigned mCurrentLevel   = 0;
        unsigned mCurrentProfile = 0;
        float mTickDelta     = 0.0f;
        float mWarpRemaining = 0.0f;
        float mWarpFactor    = 1.0f;
        float mCurrentfps    = 0.0f;
    private:
        bool mMasterUnlock   = false;
        bool mUnlimitedBombs = false;
        bool mUnlimitedWarps = false;
        bool mPlaySounds = true;
        bool mPlayMusic  = true;
        bool mShowFps    = false;
        bool mRunning    = true;
        bool mFullscreen = false;
    private:
        std::vector<std::shared_ptr<audio::AudioSample>> mMusicTracks;
        std::unique_ptr<audio::AudioPlayer> mAudioPlayer;
        std::size_t mMusicTrackId = 0;
        std::size_t mMusicTrackIndex = 0;
    private:
        std::shared_ptr<gfx::Device> mDevice;
        std::unique_ptr<gfx::Painter> mPainter;
        unsigned mRenderWidth  = 0;
        unsigned mRenderHeight = 0;
        game::AppRequestQueue mRequests;
        game::ClassLibrary* mClassLib = nullptr;
        game::Renderer mRenderer;
    };

} // invaders

