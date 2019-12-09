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
#  include <QString>
#include "warnpop.h"

#include <cstddef>
#include <vector>
#include <memory>

namespace invaders
{
    // Level is a factory for spawning enemies and configuring the game
    class Level
    {
    public:
        struct Enemy {

            // the points you get for killing this bastard
            unsigned score = 0;

            // enemy representation string.
            QString  viewstring;

            // the correct killstring to kill the enemy
            QString  killstring;

            // help/description of the enemy
            // (word definition)
            QString  help;
        };

        // Reset level for the next game play using this level.
        void Reset();

        // Validate that the loaded contents are valid.
        bool Validate() const;

        // Spawn a new enemy.
        Enemy SpawnEnemy();

        // Get the human readable name of the level.
        QString GetName() const
        { return mName; }

        // Get a list of available enemies in this level.
        const std::vector<Enemy>& GetEnemies() const
        { return mEnemies; }

        // load a list of levels from a level file.
        static
        std::vector<std::unique_ptr<Level>> LoadLevels(const QString& file);

    private:
        QString  mName;
        std::vector<Enemy> mEnemies;
        std::size_t mRandMax = 0;
    };
} // invaders