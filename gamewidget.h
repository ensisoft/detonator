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
#include "warnpush.h"
#  include <QOpenGLWidget>
#  include <QObject>
#  include <QTimer>
#  include <QElapsedTimer>
#  include <QString>
#include "warnpop.h"

#include <list>
#include <map>
#include <memory>
#include <chrono>
#include <stack>

namespace gfx {
    class Painter;
    class GraphicsDevice;
} // namespace

namespace invaders
{
    class Game;
    class Level;

    class GameWidget : public QOpenGLWidget
    {
        Q_OBJECT

    public:
        // Level info for persisting level data
        struct LevelInfo {
            QString name;
            unsigned highScore;
            bool locked;
        };

        // game profile settings, for example "easy", "medium" etc.
        struct Profile {
            QString  name;
            float speed;
            unsigned spawnCount;
            unsigned spawnInterval;
            unsigned numEnemies;
        };

        GameWidget();
       ~GameWidget();

        // load level data from the specified data file
        void loadLevels(const QString& file);

        // unlock the level identified by it's name
        void unlockLevel(const QString& name);

        // restore previously stored level info
        void setLevelInfo(const LevelInfo& info);

        // retrieve the lavel info for the level at index.
        // returns true when index is a valid index, otherwise false.
        bool getLevelInfo(LevelInfo& info, unsigned index) const;

        // set the gaming profile
        void setProfile(const Profile& profile);

        // launch the game contents.
        void launchGame();

        // step the game forward by dt milliseconds.
        void updateGame(float dt);

        // render current game state.
        void renderGame();

        // set to true to unlock all levels.
        void setMasterUnlock(bool onOff);

        // set to true to have unlimited time warps
        void setUnlimitedWarps(bool onOff)
        { mUnlimitedWarps = onOff; }

        // set to true to have unlimited bombs
        void setUnlimitedBombs(bool onOff)
        { mUnlimitedBombs = onOff; }

        // set to true to play sound effects.
        void setPlaySounds(bool onOff);

        // set to true to play awesome game music
        void setPlayMusic(bool onOff)
        { mPlayMusic = onOff; }

        // set to true to display current fps
        void setShowFps(bool onOff)
        { mShowFps = onOff; }

        // set most current fps.
        // if setShowFps has been set to true will display
        // the current fps in the top left corner of the window.
        void setFps(float fps)
        { mCurrentfps = fps; }

        bool getPlaySounds() const
        { return mPlaySounds; }

        bool getPlayMusic() const
        { return mPlayMusic; }

        bool isRunning() const
        { return mRunning; }

    private:
        virtual void initializeGL() override;
        virtual void closeEvent(QCloseEvent* close) override;
        virtual void paintEvent(QPaintEvent* paint) override;
        virtual void keyPressEvent(QKeyEvent* press) override;
    private:
        void playMusic();

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
        class Background;

    private:
        std::unique_ptr<Background> mBackground;
        std::stack<std::unique_ptr<State>> mStates;

        std::map<unsigned, std::unique_ptr<Invader>> mInvaders;
        std::vector<std::unique_ptr<Level>> mLevels;
        std::vector<LevelInfo> mLevelInfos;
        std::vector<Profile> mProfiles;
        std::list<std::unique_ptr<Animation>> mAnimations;

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
    private:
        std::size_t mMusicTrackId = 0;
    private:
        std::shared_ptr<gfx::GraphicsDevice> mCustomGraphicsDevice;
        std::unique_ptr<gfx::Painter> mCustomGraphicsPainter;
    };

} // invaders

