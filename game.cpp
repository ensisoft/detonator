// Copyright (c) 2014 Sami Väisänen, Ensisoft 
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

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "game.h"
#include "level.h"

namespace invaders
{

Game::Game(unsigned width, unsigned height) : tick_(0), width_(width), height_(height), identity_(1), level_(nullptr)
{}

Game::~Game()
{}

void Game::tick()
{
    if (!level_)
        return;

    // prune invaders
    auto end = std::partition(std::begin(invaders_), std::end(invaders_),
        [&](const invader& i) {
            return i.xpos != 0;
        });
    for (auto it = end; it != std::end(invaders_); ++it)
    {
        const auto& inv = *it;
        on_invader_victory(inv);
        score_.points -= std::min(inv.score, score_.points);
        score_.victor++;
        score_.pending--;
    }

    invaders_.erase(end, std::end(invaders_));

    // update live invaders
    for (auto& i : invaders_)
    {
        i.xpos -= 1;
    }

    const auto spawnCount    = setup_.spawnCount;
    const auto spawnInterval = setup_.spawnInterval;    
    const auto enemyCount    = setup_.numEnemies;

    if (spawned_ == enemyCount)
    {
        // todo: spawn THE BOSS

        if (invaders_.empty())
            on_level_complete(score_);
    }
    else if (spawned_ < enemyCount)
    {
        if ((tick_ % spawnInterval) == 0)
        {
            for (auto i=0; i<spawnCount; ++i)
            {
                if (++spawned_ == enemyCount)
                    break;
                const auto enemy = level_->spawn();
                invader inv;
                inv.killstring = enemy.killstring;
                inv.string     = enemy.string;
                inv.score      = enemy.score;            
                inv.ypos       = std::rand() % height_;
                inv.xpos       = width_ + i;
                inv.identity   = identity_++;
                invaders_.push_back(inv);
                on_invader_spawn(inv);
            }
        }
    }
    ++tick_;
}

void Game::fire(const Game::missile& missile)
{
    auto it = std::find_if(std::begin(invaders_), std::end(invaders_),
        [&](const invader& i) {
            return i.killstring == missile.string;
        });
    if (it == std::end(invaders_))
        return;

    auto inv = *it;

    score_.points += killScore(inv.score);
    score_.killed++;
    score_.pending--;

    on_invader_kill(*it, missile);

    invaders_.erase(it);
}

void Game::play(Level* level, Game::setup setup)
{
    invaders_.clear();
    level_   = level;
    tick_    = 0;
    spawned_ = 0;
    score_   = score{0, 0, 0, 0};
    score_.pending = setup.numEnemies;
    setup_ = setup;
}

void Game::quitLevel()
{
    invaders_.clear();
    level_ = nullptr;
    std::memset(&score_, 0, sizeof(score_));
    std::memset(&setup_, 0, sizeof(setup_));
}

unsigned Game::killScore(unsigned points) const 
{
    return points;
}

} // invaders