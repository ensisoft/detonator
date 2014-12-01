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
#include <map>
#include <cstdlib>
#include <cstring>
#include "game.h"
#include "level.h"

const unsigned DangerZone = 3;

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
            return i.xpos > i.speed;
        });
    for (auto it = end; it != std::end(invaders_); ++it)
    {
        const auto& inv = *it;
        onInvaderVictory(inv);
        score_.points -= std::min(inv.score, score_.points);
        score_.victor++;
        score_.pending--;
    }

    invaders_.erase(end, std::end(invaders_));

    // update live invaders
    for (auto& i : invaders_)
    {
        i.xpos -= i.speed;
        if (i.xpos < DangerZone)
        {
            onInvaderWarning(i);
        }
    }

    if (spawned_ == setup_.numEnemies)
    {
        // todo: spawn THE BOSS
        //spawnBoss();

        if (invaders_.empty())
        {
            onLevelComplete(score_);
            level_ = nullptr;
        }
    }
    else if (isSpawnTick())
    {
        spawn();
    }
    ++tick_;
}

void Game::fire(const Game::missile& missile)
{
    auto pos = width_;

    auto it = std::find_if(std::begin(invaders_), std::end(invaders_),
        [&](const invader& i) {
            return (i.killstring == missile.string) && 
                   (i.xpos < pos);
        });
    if (it == std::end(invaders_))
        return;

    auto& inv = *it;
    inv.score = killScore(inv);

    score_.points += inv.score;
    score_.killed++;
    score_.pending--;

    onMissileKill(*it, missile);

    invaders_.erase(it);
}

void Game::ignite(const bomb& b)
{
    if (!setup_.numBombs)
        return;

    auto end = std::partition(std::begin(invaders_), std::end(invaders_),
        [&](const invader& i) {
            return i.xpos < width_;
        });

    for (auto it = std::begin(invaders_); it != end; ++it)
    {
        auto& inv = *it;
        inv.score = killScore(inv);
        score_.points += inv.score;
        score_.killed++;
        score_.pending--;
        onBombKill(inv, b);
    }
    invaders_.erase(std::begin(invaders_), end);

    onBomb();    

    setup_.numBombs--;
}

void Game::enter(const timewarp& w)
{
    
}

void Game::play(Level* level, Game::setup setup)
{
    invaders_.clear();
    level_           = level;
    tick_            = 0;
    spawned_         = 0;
    score_.killed    = 0;
    score_.points    = 0;
    score_.victor    = 0;
    score_.maxpoints = 0;    
    score_.pending   = setup.numEnemies;
    setup_ = setup;
}

void Game::quitLevel()
{
    invaders_.clear();
    level_ = nullptr;
    std::memset(&score_, 0, sizeof(score_));
    std::memset(&setup_, 0, sizeof(setup_));
}

unsigned Game::killScore(const invader& inv) const
{
    // scoring goes as follows.
    // there's a time factor that decreases as the invader approaches an escape.
    // if the enemy is in the warning zone (we're showing the kill string)
    // then there's no points to be given (but no penalty either)
    // otherwise award the player the invader base points  + bonus
    if (inv.xpos < DangerZone)
        return 0;

    const auto xpos   = (float)inv.xpos - DangerZone;
    const auto width  = (float)width_ - DangerZone - 1; // width_'th column is not even visible
    const auto points = inv.score * inv.speed;
    const auto bonus  = xpos / width;

    return inv.score +  (inv.score * bonus);
}

void Game::spawn()
{
    const auto spawnCount = setup_.spawnCount;
    const auto enemyCount = setup_.numEnemies;
    std::map<unsigned, unsigned> batch;

    for (unsigned i=0; i<spawnCount; ++i)
    {
        if (spawned_ == enemyCount)
            break;

        const auto enemy = level_->spawn();
        invader inv;
        inv.killstring = enemy.killstring;
        inv.string     = enemy.string;
        inv.score      = enemy.score;            
        inv.ypos       = std::rand() % height_;
        inv.xpos       = width_ + batch[inv.ypos] + i;
        inv.identity   = identity_++;
        inv.speed      = 1 + (!(std::rand() % 5));
        invaders_.push_back(inv);
        onInvaderSpawn(inv);
        spawned_++;
        score_.maxpoints += (enemy.score * 2);
        batch[inv.ypos]++;
    }
}

void Game::spawnBoss()
{
    // todo:
}

bool Game::isSpawnTick() const
{
    return !(tick_ % setup_.spawnInterval);
}

} // invaders
