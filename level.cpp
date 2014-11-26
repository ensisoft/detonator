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
    const auto toks = line.split("=");
    if (toks.size() != 2)
        throw std::runtime_error("unexpected data");

    if (toks[0] != key)
        throw std::runtime_error("invalid key");

    const auto ret = toks[1].toUInt();
    return ret;
}

} // namespace

namespace invaders
{

Level::Level() : spawncount_(0), spawninterval_(0), enemycount_(0), seedvalue_(0)
{}

Level::~Level()
{}

void Level::load(const QString& file)
{
    QFile io(file);
    if (!io.open(QIODevice::ReadOnly))
        throw std::runtime_error("failed to load levels");

    QTextStream stream(&io);
    stream.setCodec("UTF-8");    

    spawncount_    = readInt(stream, "spawnCount");
    spawninterval_ = readInt(stream, "spawnInterval");
    enemycount_    = readInt(stream, "enemyCount");

    // read the enemy data
    while (!stream.atEnd())
    {
        const auto line = readLine(stream);
        const auto toks = line.split(" ");
        if (toks.size() != 3)
            throw std::runtime_error("level data format error");

        Level::enemy enemy;
        enemy.string     = toks[0];
        enemy.killstring = toks[1];
        enemy.score      = toks[2].toInt();
        enemies_.push_back(enemy);
    }

    qDebug() << "Level spawn count: " << spawncount_;
    qDebug() << "Level spawn interval: " << spawninterval_;
    qDebug() << "Level enemy count: " << enemycount_;    
}

Level::enemy Level::spawn()
{
    return enemies_[std::rand() % enemies_.size()];
}

} // invaders