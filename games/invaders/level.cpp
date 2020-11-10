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

#include <stdexcept>
#include <sstream>
#include <fstream>

#include "base/utility.h"
#include "base/format.h"
#include "level.h"

namespace {

std::vector<std::string> Split(const std::string& s)
{
    std::vector<std::string> ret;
    std::stringstream ss(s);
    while (ss.good())
    {
        std::string tok;
        ss >> tok;
        ret.push_back(std::move(tok));
    }
    return ret;
}

std::string Concat(const std::vector<std::string>& toks, size_t from)
{
    std::string ret;
    for (; from < toks.size(); ++from)
        ret += (toks[from] + " ");

    return ret;
}

} // namespace

namespace invaders
{

void Level::Reset()
{
    mRandMax = mEnemies.size();
}

bool Level::Validate() const
{
    // scan the contents of the level data to make sure that
    // we don't have any problems.
    // one particular problem we must check for is that
    // of having syllables that are prefix of another syllable.
    // this will be confusing. for example if a level has both:
    // 書 shu  10 book / letter
    // 說 shuo 14 to speak / say
    // shu is a prefix of shuo and the if the player is intending to kill
    // "shuo" but there's a "shu" in the game play "shu" will be destroyed.
    // easist fix for this problem is to make sure that levels do not
    // contain data where syllables are each others prefix.

    for (size_t i=0; i<mEnemies.size(); ++i)
    {
        for (size_t j=0; j<mEnemies.size(); ++j)
        {
            // don't compare to self
            if (j == i) continue;
            // we can have for example "zuo" and "zuo" with different meaning.
            // this is fine.
            if (mEnemies[j].killstring == mEnemies[i].killstring)
                continue;
            // catch a case of "shuo" and "shu"
            if (mEnemies[j].killstring.find(mEnemies[i].killstring) == std::wstring::size_type(0))
                return false;
        }
    }
    return true;

}

Level::Enemy Level::SpawnEnemy()
{
    const auto n = std::rand() % mRandMax;
    const auto r = mEnemies[n];
    std::swap(mEnemies[n], mEnemies[mRandMax - 1]);

    if (mRandMax > 1)
        mRandMax--;
    else mRandMax = mEnemies.size();

    return r;
}

std::vector<std::unique_ptr<Level>> Level::LoadLevels(const std::wstring& file)
{
#if defined(WINDOWS_OS)
    std::fstream in(file); // msvs extension
#elif defined(LINUX_OS)
    std::fstream in(base::ToUtf8(file), std::ios::in | std::ios::binary);
#endif
    if (!in.is_open())
        throw std::runtime_error("Failed to open file: " + base::detail::ToString(file));

    std::vector<std::unique_ptr<Level>> levels;

    while (in.good() && !in.eof())
    {
        std::string line;
        std::getline(in, line);
        if (line.empty())
            continue;
        else if (line[0] == '#')
            continue;
        else if (line != "BEGIN")
            continue;

        std::getline(in, line);

        auto level = std::make_unique<Level>();
        level->mName = base::FromUtf8(line);

        // read the enemy data
        bool read_end = false;
        while (in.good() && !in.eof())
        {
            std::string line;
            std::getline(in, line);
            if (line.empty())
                continue;
            else if(line[0] == '#')
                continue;
            else if (line == "END") {
                read_end = true;
                break;
            }
            const auto& toks = Split(line);
            if (toks.size() < 3)
                throw std::runtime_error("Level data format error: " + line);

            Level::Enemy enemy;
            enemy.viewstring  = base::FromUtf8(toks[0]);
            enemy.killstring  = base::FromUtf8(toks[1]);
            enemy.score       = std::stoul(toks[2]);
            enemy.help        = base::FromUtf8(Concat(toks, 3));
            level->mEnemies.push_back(std::move(enemy));
        }
        if (!read_end)
            throw std::runtime_error("Level data is corrupt. No END marker was found.");

        levels.push_back(std::move(level));
    }
    return levels;
}

} // invaders
