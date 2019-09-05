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
#include "warnpush.h"
#  include <QTextStream>
#  include <QDir>
#  include <QFile>
#  include <QtDebug>
#include "warnpop.h"

#include <stdexcept>
#include <cstdlib>
#include <cassert>

#include "level.h"

namespace {

QString readLine(QTextStream& stream)
{
    QString line;
    while (!stream.atEnd())
    {
        line = stream.readLine();
        if (line.isEmpty())
            continue;
        if (line[0] == '#')
            continue;
        break;
    }
    return line;
}

unsigned readInt(QTextStream& stream, const QString& key)
{
    const auto line = readLine(stream);
    const auto toks = line.split("=", QString::SkipEmptyParts);
    if (toks.size() != 2)
        throw std::runtime_error("unexpected data");

    const auto tok = toks[0].trimmed();

    if (tok != key)
        throw std::runtime_error("invalid key");

    const auto ret = toks[1].toUInt();
    return ret;
}

QString readStr(QTextStream& stream, const QString& key)
{
    const auto line = readLine(stream);
    const auto toks = line.split("=", QString::SkipEmptyParts);
    if (toks.size() != 2)
        throw std::runtime_error("unexpected data");

    const auto tok = toks[0].trimmed();
    if (tok != key)
        throw std::runtime_error("invalid key");

    return toks[1].trimmed();
}

QString joinTokens(const QStringList& toks, int from)
{
    QString ret;
    for (int i=from; i < toks.size(); ++i)
    {
        ret.append(toks[i]);
        ret.append(" ");
    }
    return ret;
}

} // namespace

namespace invaders
{

Level::Level()
{
    reset();
}

Level::~Level()
{}

void Level::load(const QString& file)
{
    QFile io(file);
    if (!io.open(QIODevice::ReadOnly))
        throw std::runtime_error("failed to load levels");

    QTextStream stream(&io);
    stream.setCodec("UTF-8");

    const auto beg = readLine(stream);
    if (beg != "BEGIN")
        throw std::runtime_error("no level begin was found");

    name_ = readLine(stream);

    // read the enemy data
    while (!stream.atEnd())
    {
        const auto line = readLine(stream);
        if (line == "END")
            break;
        const auto toks = line.split(" ", QString::SkipEmptyParts);
        if (toks.size() < 3)
            throw std::runtime_error("level data format error");

        Level::enemy enemy;
        enemy.string     = toks[0];
        enemy.killstring = toks[1];
        enemy.score      = toks[2].toInt();
        enemy.help       = joinTokens(toks, 3);
        enemies_.push_back(enemy);
    }
}

void Level::reset()
{
    randMax_ = enemies_.size();
}

bool Level::validate() const
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

    for (size_t i=0; i<enemies_.size(); ++i)
    {
        for (size_t j=0; j<enemies_.size(); ++j)
        {
            // don't compare to self
            if (j == i) continue;
            // we can have for example "zuo" and "zuo" with different meaning.
            // this is fine.
            if (enemies_[j].killstring == enemies_[i].killstring)
                continue;
            // catch a case of "shuo" and "shu"
            if (enemies_[j].killstring.startsWith(enemies_[i].killstring))
            {
                qDebug() << enemies_[j].killstring << " - " << enemies_[i].killstring;
                return false;
            }
        }
    }
    return true;

}

Level::enemy Level::spawn()
{
    const auto n = std::rand() % randMax_;
    const auto r = enemies_[n];
    std::swap(enemies_[n], enemies_[randMax_-1]);

    if (randMax_ > 1)
        randMax_--;
    else randMax_ = enemies_.size();

    return r;
}

std::vector<std::unique_ptr<Level>> Level::loadLevels(const QString& file)
{
    QFile io(file);
    if (!io.open(QIODevice::ReadOnly))
        throw std::runtime_error("failed to load levels");

    QTextStream stream(&io);
    stream.setCodec("UTF-8");

    std::vector<std::unique_ptr<Level>> ret;

    while (!stream.atEnd())
    {
        const auto line = readLine(stream);
        if (line != "BEGIN")
            continue;

        std::unique_ptr<Level> next(new Level);
        next->name_ = readLine(stream);

        // read the enemy data
        bool end = false;
        while (!stream.atEnd())
        {
            const auto line = readLine(stream);
            if (line == "END")
            {
                end = true;
                break;
            }
            const auto toks = line.split(" ", QString::SkipEmptyParts);
            if (toks.size() < 3)
                throw std::runtime_error("level data format error");

            Level::enemy enemy;
            enemy.string     = toks[0];
            enemy.killstring = toks[1];
            enemy.score      = toks[2].toInt();
            enemy.help       = joinTokens(toks, 3);
            next->enemies_.push_back(enemy);
        }
        if (!end)
            throw std::runtime_error("no end in sight...");

        ret.push_back(std::move(next));
    }
    return ret;
}

} // invaders
