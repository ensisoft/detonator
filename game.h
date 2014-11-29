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
#  include <QtGui/QVector2D>
#  include <QString>
#include "warnpop.h"

#include <memory>
#include <deque>
#include <functional>

namespace invaders
{
    class Level;

    class Game
    {
    public:
        struct invader {
            unsigned ypos;
            unsigned xpos;
            unsigned identity;
            unsigned score;
            unsigned velocity;
            QString  string;
            QString  killstring;
        };

        struct missile {
            QVector2D position;
            QString string;
        };

        struct score {
            unsigned points;
            unsigned killed;
            unsigned victor;
            unsigned pending;
            unsigned maxpoints;
        };

        struct setup {
            unsigned numEnemies;
            unsigned spawnCount;
            unsigned spawnInterval;
        };

        std::function<void (const invader&, const missile& m)> on_invader_kill;
        std::function<void (const invader&)> on_invader_spawn;        
        std::function<void (const invader&)> on_invader_victory;
        std::function<void (const invader&)> on_invader_warning;
        std::function<void (const score&)> on_level_complete;

        Game(unsigned width, unsigned heigth);
       ~Game();

        // advance game simulation by one increment
        void tick();

        // launch a missile at the current player position
        void fire(const missile& m);

        // start playing a level
        void play(Level* level, Game::setup setup);

        void quitLevel();

        // get game space width
        unsigned width() const 
        { return width_; }

        // get game space height
        unsigned height() const
        { return height_; }

        const 
        score& getScore() const 
        { return score_; }

        const
        std::deque<invader>& invaders() const 
        { return invaders_; }

        bool isRunning() const 
        { return level_ != nullptr; }


    private:
        unsigned killScore(const invader& inv) const;
        void spawn();
        void spawnBoss();
        bool isSpawnTick() const;

    private:
        std::deque<invader> invaders_;
        unsigned tick_;
        unsigned spawned_;
        unsigned width_;
        unsigned height_;
        unsigned identity_;
        score score_;
        setup setup_;
        Level* level_;
    };

} // invaders