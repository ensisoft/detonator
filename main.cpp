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
#  include <QApplication>
#  include <QStringList>
#  include <QtDebug>
#  include <QSettings>
#  include <QElapsedTimer>
#  include <QtGui/QScreen>
#include "warnpop.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <thread>

#if defined(LINUX_OS)
#  include <fenv.h>
#endif
#include "gamewidget.h"

#if defined(ENABLE_AUDIO)
#  include "audio.h"

namespace invaders {

AudioPlayer* g_audio;

} // invaders

#endif // ENABLE_AUDIO

void loadProfile(invaders::GameWidget::Profile profile,
                 invaders::GameWidget& widget,
                 const QSettings& settings)
{
    QString name          = profile.name;
    profile.speed         = settings.value(name + "/speed", profile.speed).toFloat();
    profile.spawnCount    = settings.value(name + "/spawnCount", profile.spawnCount).toUInt();
    profile.spawnInterval = settings.value(name + "/spawnInterval", profile.spawnInterval).toUInt();
    profile.numEnemies    = settings.value(name + "/enemyCount", profile.numEnemies).toUInt();

    widget.setProfile(profile);

    qDebug() << "Game Profile:" << profile.name;
    qDebug() << "Speed:" << profile.speed;
    qDebug() << "spawnCount:" << profile.spawnCount;
    qDebug() << "spawnInterval:" << profile.spawnInterval;
    qDebug() << "enemyCount: " << profile.numEnemies;
}

float computeDefaultTimeStep()
{
    const QScreen* screen = QGuiApplication::primaryScreen();
    const auto refresh_rate = screen->refreshRate();
    const auto time_step = 1000.0f / refresh_rate;
    qDebug() << "Screen Hz " << refresh_rate;
    qDebug() << "Time Step " << time_step;
    return time_step;
}

int game_main(int argc, char* argv[])
{
#ifdef ENABLE_AUDIO
    #ifdef USE_PULSEAUDIO
    std::unique_ptr<invaders::AudioDevice> pa(new invaders::PulseAudio("Pinyin-Invaders"));
    #endif
    #ifdef USE_WAVEOUT
    std::unique_ptr<invaders::AudioDevice> pa(new invaders::Waveout("Pinyin-Invaders"));
    #endif
    pa->init();
    invaders::AudioPlayer player(std::move(pa));
    invaders::g_audio = &player;
#endif

#if defined(LINUX_OS)
    // SIGFPE on floating point exception
    feenableexcept(FE_INVALID   |
                   FE_DIVBYZERO |
                   FE_OVERFLOW  |
                   FE_UNDERFLOW);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("Pinyin-Invaders");

    bool masterUnlock = false;
    bool unlimitedWarps = false;
    bool unlimitedBombs = false;
    bool showFps = false;

    const auto& args = app.arguments();
    for (const auto& a : args)
    {
        if (a == "--unlock-all")
            masterUnlock = true;
        else if (a == "--unlimited-warps")
            unlimitedWarps = true;
        else if (a == "--unlimited-bombs")
            unlimitedBombs = true;
        else if (a == "--show-fps")
            showFps = true;
    }

    invaders::GameWidget window;

    window.setMasterUnlock(masterUnlock);
    window.setUnlimitedWarps(unlimitedWarps);
    window.setUnlimitedBombs(unlimitedBombs);
    window.setShowFps(showFps);

    QSettings settings("Ensisoft", "Invaders");
    const auto width  = settings.value("window/width", 1200).toInt();
    const auto height = settings.value("window/height", 700).toInt();
    const auto xpos = settings.value("window/xpos", window.x()).toInt();
    const auto ypos = settings.value("window/ypos", window.y()).toInt();
    window.resize(width, height);
    window.move(xpos, ypos);

    const auto playSound = settings.value("audio/sound", true).toBool();
    const auto playMusic = settings.value("audio/music", true).toBool();
    window.setPlayMusic(playMusic);
    window.setPlaySounds(playSound);

    const auto& levels_txt = QApplication::applicationDirPath() + "/data/levels.txt";
    window.loadLevels(levels_txt);

    QStringList levels = settings.value("game/levels").toStringList();
    for (int i=0; i<levels.size(); ++i)
    {
        const auto name = levels[i];
        invaders::GameWidget::LevelInfo info;
        info.name = name;
        info.highScore = settings.value(name + "/highscore").toUInt();
        info.locked = settings.value(name + "/locked").toBool();
        window.setLevelInfo(info);
    }

    const invaders::GameWidget::Profile EASY    {"Easy",    1.6f, 2, 7, 30};
    const invaders::GameWidget::Profile MEDIUM  {"Medium",  1.8f, 2, 4, 35};
    const invaders::GameWidget::Profile CHINESE {"Chinese", 2.0f, 2, 4, 40};

    loadProfile(EASY, window, settings);
    loadProfile(MEDIUM, window, settings);
    loadProfile(CHINESE, window, settings);

    window.launchGame();
    window.show();

    const float TimeStep = computeDefaultTimeStep();

    unsigned frames = 0;

    QElapsedTimer timer;
    timer.start();

    while (window.running())
    {
        app.processEvents();

        window.updateGame(TimeStep);
        window.renderGame();
        frames++;

        const auto ms = timer.elapsed();

        if (ms >= 1000)
        {
            const float fps = frames / (ms / 1000.0f);
            frames    = 0;
            timer.restart();
            window.setFps(fps);
        }
    }

    settings.setValue("window/width",  window.lastWindowWidth());
    settings.setValue("window/height", window.lastWindowHeight());
    settings.setValue("window/xpos", window.x());
    settings.setValue("window/ypos", window.y());

    levels.clear();
    for (unsigned i=0; ; ++i)
    {
        invaders::GameWidget::LevelInfo info;
        if (!window.getLevelInfo(info, i))
            break;

        levels << info.name;
        settings.setValue(info.name + "/highscore", info.highScore);
        settings.setValue(info.name + "/locked", info.locked);
    }
    settings.setValue("game/levels", levels);
    settings.setValue("audio/sound", window.getPlaySounds());
    settings.setValue("audio/music", window.getPlayMusic());
    settings.sync();

    return 0;
}

int main(int argc, char* argv[])
{
    try
    {
        return game_main(argc, argv);
    }
    catch (const std::exception& bollocks)
    {
        std::cerr << "Oops.. there was a problem: " << bollocks.what();
    }
    return 1;
}
