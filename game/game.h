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

#include <memory>
#include <deque>
#include <string>
#include <vector>
#include <functional>

namespace invaders
{
    class Level;

    // Implements the main game logic
    class Game
    {
    public:
        // the type of the invader
        enum class InvaderType {
            // Normal enemy
            Regular,
            // Boss of the level
            Boss
        };

        struct Invader {
            // The current x position of the invader
            // in the game space
            unsigned ypos = 0;
            // The current y position of the invader
            // in the game space.
            unsigned xpos = 0;
            // Unique id for each invader.
            unsigned identity = 0;
            // The final score when the invader is killed.
            // Initially zero and only computed later.
            unsigned score = 0;
            // The speed of the invader expressed in units/ticks,
            // i.e. how many game space units does the invader
            // advance on each game tick.
            unsigned speed = 0;
            // The list of characters (in Pinyin) required
            // to kill this invader.
            std::deque<std::wstring> killList;
            // The list of characters (in Chinese) required
            // to kill this invader.
            std::deque<std::wstring> viewList;
            // The type of the invader. Either boss or normal.
            InvaderType type;
            // True when the shield is on.
            bool shield = false;
            // How many ticks the shield is on.
            unsigned shield_on_ticks = 0;
            // How many ticks the shield is off.
            unsigned shield_off_ticks = 0;
        };

        struct Missile {
            float launch_position_x = 0.0f;
            float launch_position_y = 0.0f;
            std::wstring string;
        };

        struct Bomb {

        };

        struct Timewarp {
            float duration = 0.0f;
            float factor   = 0.0f;
        };

        // The aggregate score/tally of the game play.
        struct Score {
            // Current combined score.
            unsigned points = 0;
            // How many enemies have been killed.
            unsigned killed = 0;
            // How many enemies are still coming.
            unsigned pending = 0;
            // Maximum points that can be scored in the current level.
            unsigned maxpoints = 0;
        };

        // Setup the game with some parameters that control
        // the game play.
        struct Setup {
            // How many enemies are to be played in total.
            unsigned numEnemies    = 0;
            // How many enemies to spawn at a single spawn tick.
            unsigned spawnCount    = 0;
            // The interval between spawning enemies expressed in ticks.
            unsigned spawnInterval = 0;
            // The number of bombs allotted to the player.
            unsigned numBombs      = 0;
            // The number of warps allotted to the player.
            unsigned numWarps      = 0;
        };


        std::function<void (const Invader&, const Missile& m, unsigned score)> onMissileKill;
        std::function<void (const Invader&, const Missile& m)> onMissileDamage;
        std::function<void (const Invader&, const Missile& m)> onMissileFire;
        std::function<void (const Invader&, const Bomb& b, unsigned score)> onBombKill;
        std::function<void (const Invader&, const Bomb& b)> onBombDamage;
        std::function<void (const Invader&, bool)> onToggleShield;
        std::function<void (const Bomb& b)> onBomb;
        std::function<void (const Timewarp& w)> onWarp;
        std::function<void (const Invader&)> onInvaderSpawn;
        std::function<void (const Invader&)> onInvaderVictory;
        std::function<void (const Invader&)> onInvaderWarning;
        std::function<void (const Score&)> onLevelComplete;

        // Construct a new game with the given game space
        // dimesions.
        Game(unsigned width, unsigned heigth);
       ~Game();
        // Advance game simulation by one increment.
        void Tick();
        // Launch a missile at the current player position.
        // Returns true if the missile was actually fired at
        // any enemy. Otherwise false.
        bool FireMissile(const Missile& m);
        // Ignite a bomb in the game if any currently exist.
        // Returns true if bomb was ignited, otherwise false.
        bool IgniteBomb(const Bomb& b);
        // Enter a timewarp.
        // Returns true if timewarp was started, otherwise false
        bool EnterTimewarp(const Timewarp& w);
        // Start playing a level.
        void Play(Level* level, const Game::Setup& setup);
        // Quit playing the current level/game.
        void Quit();
        // Get game space width.
        unsigned GetWidth() const
        { return mWidth; }
        // Get game space height.
        unsigned GetHeight() const
        { return mHeight; }
        // Get the number of currently available bombs.
        unsigned GetNumBombs() const
        { return mSetup.numBombs; }
        // Get the number of currently available warps.
        unsigned GetNumWarps() const
        { return mSetup.numWarps; }
        // Get the current score.
        const Score& GetScore() const
        { return mScore; }
        // Returns true if game is currently running
        // otherwise false.
        bool IsRunning() const
        { return mLevel != nullptr; }

    private:
        unsigned ComputeKillScore(const Invader& inv) const;
        bool HasShield(const Invader& inv) const;
        void SpawnNext();
        void SpawnBoss();
        bool IsTimeToSpawn() const;

    private:
        // Width of the game space.
        const unsigned mWidth  = 0;
        // Height of the game space.
        const unsigned mHeight = 0;
        // The current tick counter.
        // Used to express the passing of time
        // in game ticks.
        unsigned mCurrentTick  = 0;
        // How many enemies have been spawned so far.
        unsigned mSpawnCount   = 0;
        // The enemies/invaders currently in the game.
        std::vector<Invader> mInvaders;
        // Per game space row queue size added to the
        // Invaders xposition when spawed. (additional
        // distance they need to travel).
        std::vector<unsigned> mSlots;
        // Tally of the game scoring.
        Score mScore;
        // Game play parameters
        Setup mSetup;
        // Current level we're playing.
        Level* mLevel  = nullptr;
        // Have spawned boss or not.
        bool mHaveBoss = false;
    };

} // invaders
