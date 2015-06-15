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
#  include <QSettings>
#  include <QDir>
#  include <QStringList>
#  include <QtDebug>
#include "warnpop.h"

#include "mainwindow.h"

namespace invaders
{

MainWindow::MainWindow()
{
    ui_.setupUi(this);
    QSettings settings("Ensisoft", "Invaders");
    const auto wwidth  = settings.value("window/width", 1200).toInt();
    const auto wheight = settings.value("window/height", 700).toInt();
    const auto xpos = settings.value("window/xpos", x()).toInt();
    const auto ypos = settings.value("window/ypos", y()).toInt();
    resize(wwidth, wheight);
    move(xpos, ypos);

    QObject::connect(ui_.game, SIGNAL(quitGame()), 
        this, SLOT(close()));

    QObject::connect(ui_.game, SIGNAL(enterFullScreen()),
        this, SLOT(enterFullScreen()));
    QObject::connect(ui_.game, SIGNAL(leaveFullScreen()),
        this, SLOT(leaveFullScreen()));

    const auto& inst = QApplication::applicationDirPath();
    const auto& file = inst + "/data/levels.txt";

    ui_.game->loadLevels(file); 

    QStringList levels = settings.value("game/levels").toStringList();
    for (int i=0; i<levels.size(); ++i)
    {
        const auto name = levels[i];
        GameWidget::LevelInfo info;
        info.name = name;
        info.highScore = settings.value(name + "/highscore").toUInt();
        info.locked = settings.value(name + "/locked").toBool();
        ui_.game->setLevelInfo(info);
    }

    const GameWidget::Profile EASY {"Easy", 1.8f, 2, 7, 30};
    const GameWidget::Profile MEDIUM {"Medium", 2.0f, 2, 4, 35};
    const GameWidget::Profile CHINESE {"Chinese", 2.2f, 2, 4, 40};

    loadProfile(EASY);
    loadProfile(MEDIUM);
    loadProfile(CHINESE);

    setWindowTitle(tr("Invaders %1.%2").arg(MAJOR_VERSION).arg(MINOR_VERSION));
}


MainWindow::~MainWindow()
{
    QSettings settings("Ensisoft", "Invaders");
    if (isFullScreen())
    {
        settings.setValue("window/width", width_);
        settings.setValue("window/height", height_);
    }
    else
    {
        settings.setValue("window/width", width());
        settings.setValue("window/height", height());
    }
    settings.setValue("window/xpos", x());
    settings.setValue("window/ypos", y());

    QStringList levels;
    for (unsigned i=0; ; ++i)
    {
        GameWidget::LevelInfo info;
        if (!ui_.game->getLevelInfo(info, i))
            break;

        levels << info.name;
        settings.setValue(info.name + "/highscore", info.highScore);
        settings.setValue(info.name + "/locked", info.locked);
    }
    settings.setValue("game/levels", levels);

}

void MainWindow::setMasterUnlock(bool onOff)
{
    ui_.game->setMasterUnlock(onOff);
}

void MainWindow::setUnlimitedWarps(bool onOff)
{
    ui_.game->setUnlimitedWarps(onOff);
}

void MainWindow::setUnlimitedBombs(bool onOff)
{
    ui_.game->setUnlimitedBombs(onOff);
}

void MainWindow::setPlaySound(bool onOff)
{
    ui_.game->setPlaySounds(onOff);
}

void MainWindow::enterFullScreen()
{
    width_  = width();
    height_ = height();

    showFullScreen();

    ui_.game->setFullscreen(true);
}

void MainWindow::leaveFullScreen()
{
    showNormal();
    
    resize(width_, height_);

    ui_.game->setFullscreen(false);
}

void MainWindow::loadProfile(GameWidget::Profile profile)
{
    QSettings settings("Ensisoft", "Invaders");
    QString name = profile.name;
    profile.speed = settings.value(name + "/speed", profile.speed).toFloat();
    profile.spawnCount = settings.value(name + "/spawnCount", profile.spawnCount).toUInt();
    profile.spawnInterval = settings.value(name + "/spawnInterval", profile.spawnInterval).toUInt();
    profile.numEnemies = settings.value(name + "/enemyCount", profile.numEnemies).toUInt();

    ui_.game->setProfile(profile);

    qDebug() << "Game Profile:" << profile.name;
    qDebug() << "Speed:" << profile.speed;
    qDebug() << "spawnCount:" << profile.spawnCount;
    qDebug() << "spawnInterval:" << profile.spawnInterval;
    qDebug() << "enemyCount: " << profile.numEnemies;
}

} // invaders