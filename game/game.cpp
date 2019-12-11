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
#include <cassert>
#include "game.h"
#include "level.h"

const unsigned DangerZone = 8;

namespace invaders
{

Game::Game(unsigned width, unsigned height)
  : mWidth(width)
  , mHeight(height)
{
    // each slot corresponds to a row in the game space
    // and keeps track of additional distance each invader
    // spawned onto that row must travel.
    // This adds some distance between each invader spawned
    // onto the same row. Otherwise they'd all cluster up.
    mSlots.resize(height);
}

Game::~Game()
{}

void Game::Tick()
{
    if (!mLevel)
        return;

    for (size_t i=0; i<mSlots.size(); ++i)
    {
        if (mSlots[i])
            mSlots[i] = mSlots[i] - 1;
    }


    // prune invaders
    auto end = std::partition(std::begin(mInvaders), std::end(mInvaders),
        [&](const Invader& i) {
            return i.xpos > i.speed;
        });
    for (auto it = end; it != std::end(mInvaders); ++it)
    {
        const auto& inv = *it;
        onInvaderVictory(inv);
        mScore.points -= std::min(inv.score, mScore.points);
        mScore.pending--;
    }

    mInvaders.erase(end, std::end(mInvaders));

    // update live invaders
    for (auto& i : mInvaders)
    {
        i.xpos -= i.speed;
        if (i.xpos < DangerZone)
        {
            onInvaderWarning(i);
        }
        onToggleShield(i, HasShield(i));
    }

    if (mSpawnCount == mSetup.numEnemies)
    {
        if (!mHaveBoss)
        {
            if (mInvaders.empty())
            {
                SpawnBoss();
                mHaveBoss = true;
            }
        }
        else if (mInvaders.empty())
        {
            onLevelComplete(mScore);
            mLevel = nullptr;
        }
    }
    else if (IsTimeToSpawn())
    {
        SpawnNext();
    }

    ++mCurrentTick;
}

bool Game::FireMissile(const Game::Missile& missile)
{
    auto end = std::partition(std::begin(mInvaders), std::end(mInvaders),
        [&](const Invader& i) {
            return i.killList[0] == missile.string;
        });
    if (end == std::begin(mInvaders))
        return false;

    auto it = std::min_element(std::begin(mInvaders), end,
        [&](const Invader& a, const Invader& b) {
            return a.xpos < b.xpos;
        });

    auto& inv = *it;
    // if it's not yet visible it cannot be killed ;)
    if (inv.xpos >= mWidth)
        return false;

    // if the Invader has it's shield up we can't kill it
    if (HasShield(inv))
    {
        onMissileFire(inv, missile);
        return true;
    }

    inv.viewList.pop_front();
    inv.killList.pop_front();
    if (inv.killList.empty())
    {
        inv.score = ComputeKillScore(inv);
        mScore.points += inv.score;
        mScore.killed++;
        mScore.pending--;

        onMissileKill(inv, missile, inv.score);
        mInvaders.erase(it);
    }
    else
    {
        onMissileDamage(inv, missile);
    }
    return true;
}

bool Game::IgniteBomb(const Bomb& b)
{
    if (!mSetup.numBombs)
        return false;

    for (auto it = std::begin(mInvaders); it != std::end(mInvaders); ++it)
    {
        auto& i = *it;

        if (HasShield(i))
            continue;

        if (i.xpos < mWidth)
        {
            i.killList.pop_front();
            i.viewList.pop_front();
        }
    }

    auto end = std::partition(std::begin(mInvaders), std::end(mInvaders),
        [&](const Invader& i) {
            return i.killList.empty();
        });

    for (auto it = std::begin(mInvaders); it != end; ++it)
    {
        auto& inv = *it;
        inv.score = ComputeKillScore(inv);
        mScore.points += inv.score;
        mScore.killed++;
        mScore.pending--;
        onBombKill(inv, b, inv.score);
    }
    for (auto it = end; it != std::end(mInvaders); ++it)
    {
        auto& inv = *it;
        if (HasShield(inv))
            continue;

        onBombDamage(inv, b);
    }

    mInvaders.erase(std::begin(mInvaders), end);

    onBomb(b);

    mSetup.numBombs--;
    return true;
}

bool Game::EnterTimewarp(const Timewarp& w)
{
    if (!mSetup.numWarps)
        return false;

    onWarp(w);

    mSetup.numWarps--;
    return true;
}

void Game::Play(Level* level, const Game::Setup& setup)
{
    mInvaders.clear();
    mLevel           = level;
    mCurrentTick     = 0;
    mSpawnCount      = 0;
    mScore.killed    = 0;
    mScore.points    = 0;
    mScore.maxpoints = 0;
    mScore.pending   = setup.numEnemies + 1; // +1 for the BOSS
    mSetup    = setup;
    mHaveBoss = false;
}

void Game::Quit()
{
    mInvaders.clear();
    mLevel = nullptr;
    mScore = Score{};
    mSetup = Setup{};
}

unsigned Game::ComputeKillScore(const Invader& inv) const
{
    // scoring goes as follows.
    // there's a time factor that decreases as the Invader approaches an escape.
    // if the enemy is in the warning zone (we're showing the kill string)
    // then there's no points to be given (but no penalty either)
    // otherwise award the player the Invader base points  + bonus
    if (inv.xpos < DangerZone)
        return 0;

    const auto xpos   = (float)inv.xpos - DangerZone;
    const auto width  = (float)mWidth - DangerZone - 1; // width'th column is not even visible
    const auto points = inv.score * inv.speed;
    const auto bonus  = xpos / width;

    // put more weight on just killing the Invader than on when it's being killed
    return 0.6 * points + 0.4 * (points * bonus);
}

bool Game::HasShield(const Invader& inv) const
{
#if ENABLE_GAME_FEATURE_SHIELD
    const auto cycle = inv.shield_on_ticks + inv.shield_off_ticks;
    if (cycle == 0)
        return false;

    const auto phase = mCurrentTick % cycle;
    if (phase >= inv.shield_on_ticks)
        return true;
#endif

    return false;
}

void Game::SpawnNext()
{
    const auto spawnCount = mSetup.spawnCount;
    const auto enemyCount = mSetup.numEnemies;

    // pull a random row for the batch
    const auto batch_row = std::rand() % mHeight;

    for (unsigned i=0; i<spawnCount; ++i)
    {
        if (mSpawnCount == enemyCount)
            break;

        const auto row   = (batch_row + i * 3) % mHeight;
        assert(row <= mSlots.size());
        const auto queue = mSlots[row];

        const auto enemy = mLevel->SpawnEnemy();
        Invader inv;
        inv.killList.push_back(enemy.killstring);
        inv.viewList.push_back(enemy.viewstring);
        inv.score      = enemy.score;
        inv.ypos       = row;
        inv.xpos       = mWidth  + queue;
        inv.identity   = mSpawnCount; // use the spawn counter as the id value.
        inv.speed      = 1;
        inv.type       = InvaderType::Regular;
        inv.shield     = false;


        if (!(std::rand() % 5))
        {
            inv.speed = 2;
        }

        if (!(std::rand() % 6))
        {
            const auto enemy = mLevel->SpawnEnemy();
            inv.killList.push_back(enemy.killstring);
            inv.viewList.push_back(enemy.viewstring);
            inv.score *= 2;
            inv.speed  = 1;
        }

        if (inv.speed == 1 && inv.killList.size() == 1)
        {
            if (!(std::rand() % 5))
            {
                inv.shield_on_ticks  = 2;
                inv.shield_off_ticks = 2;
            }
        }


        inv.score *= 10;

        mInvaders.push_back(inv);
        onInvaderSpawn(inv);

        ++mSpawnCount;

        inv.xpos = mWidth - DangerZone - 1;
        mScore.maxpoints += ComputeKillScore(inv);

        // increment the slot
        mSlots[row] = queue + 5;
    }
}

void Game::SpawnBoss()
{
    Invader boss;
    boss.ypos     = mHeight / 2;
    boss.xpos     = mWidth + 5;
    boss.identity = mSpawnCount + 1;
    boss.score    = 0;
    boss.speed    = 1;
    boss.type     = InvaderType::Boss;

    for (int i=0; i<5; ++i)
    {
        const auto enemy = mLevel->SpawnEnemy();
        boss.viewList.push_back(enemy.viewstring);
        boss.killList.push_back(enemy.killstring);
        boss.score += enemy.score;
    }
    boss.score *= 17;

    onInvaderSpawn(boss);

    mInvaders.push_back(boss);

    boss.xpos = mWidth - DangerZone - 1;
    mScore.maxpoints += ComputeKillScore(boss);

}

bool Game::IsTimeToSpawn() const
{
    return !(mCurrentTick % mSetup.spawnInterval);
}

} // invaders
