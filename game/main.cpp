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
#  include <QStringList>
#  include <QSettings>
#include "warnpop.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include "audio/player.h"
#include "audio/device.h"
#include "base/logging.h"

#if defined(LINUX_OS)
#  include <fenv.h>
#endif
#include "gamewidget.h"

namespace invaders {

    audio::AudioPlayer* g_audio;

} // invaders



void loadProfile(invaders::GameWidget::Profile profile,
                 invaders::GameWidget& widget,
                 const QSettings& settings)
{
    QString name          = QString::fromStdWString(profile.name);
    profile.speed         = settings.value(name + "/speed", profile.speed).toFloat();
    profile.spawnCount    = settings.value(name + "/spawnCount", profile.spawnCount).toUInt();
    profile.spawnInterval = settings.value(name + "/spawnInterval", profile.spawnInterval).toUInt();
    profile.numEnemies    = settings.value(name + "/enemyCount", profile.numEnemies).toUInt();

    widget.setProfile(profile);

    DEBUG("Game Profile: %1", profile.name);
    DEBUG("Speed %1", profile.speed);
    DEBUG("Spawn Count %1", profile.spawnCount);
    DEBUG("Spawn Interval %1", profile.spawnInterval);
    DEBUG("Enemy Count %1", profile.numEnemies);
}

int game_main(int argc, char* argv[])
{
    bool masterUnlock = false;
    bool unlimitedWarps = false;
    bool unlimitedBombs = false;
    bool showFps = false;
#if defined(_NDEBUG)
    bool debugLog = true;
#else
    bool debugLog = false;
#endif
    for (int i=1; i<argc; ++i)
    {
        const std::string a(argv[i]);
        if (a == "--unlock-all")
            masterUnlock = true;
        else if (a == "--unlimited-warps")
            unlimitedWarps = true;
        else if (a == "--unlimited-bombs")
            unlimitedBombs = true;
        else if (a == "--show-fps")
            showFps = true;
        else if (a == "--debug-log")
            debugLog = true;
    }

    base::CursesLogger logger;
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(debugLog);

    DEBUG("It's alive!");
    INFO("%1 %2", GAME_TITLE, GAME_VERSION);
    INFO("Copyright (c) 2010-2018 Sami Vaisanen");
    INFO("http://www.ensisoft.com");
    INFO("http://github.com/ensisoft/pinyin-invaders");
    

    audio::AudioPlayer audio_player(audio::AudioDevice::Create(GAME_TITLE));
    invaders::g_audio = &audio_player;

#if defined(LINUX_OS)
    // SIGFPE on floating point exception
    feenableexcept(FE_INVALID   |
                   FE_DIVBYZERO |
                   FE_OVERFLOW  |
                   FE_UNDERFLOW);
    DEBUG("Enabled floating point exceptions");
#endif

    QSettings settings("Ensisoft", "Invaders");
    const auto width      = settings.value("window/width", 1200).toInt();
    const auto height     = settings.value("window/height", 700).toInt();
    const auto fullscreen = settings.value("window/fullscreen", false).toBool();
    const auto playSound  = settings.value("audio/sound", true).toBool();
    const auto playMusic  = settings.value("audio/music", true).toBool();
    const auto& levels    = settings.value("game/levels").toStringList();
    
    invaders::GameWidget window;
    window.setMasterUnlock(masterUnlock);
    window.setUnlimitedWarps(unlimitedWarps);
    window.setUnlimitedBombs(unlimitedBombs);
    window.setShowFps(showFps);
    window.setPlayMusic(playMusic);
    window.setPlaySounds(playSound);
    window.loadLevels(L"data/levels.txt");

    // set the saved level data 
    for (int i=0; i<levels.size(); ++i)
    {
        const auto name = levels[i];
        invaders::GameWidget::LevelInfo info;
        info.name      = name.toStdWString();
        info.highScore = settings.value(name + "/highscore").toUInt();
        info.locked    = settings.value(name + "/locked").toBool();
        window.setLevelInfo(info);
    }

    const invaders::GameWidget::Profile EASY    {L"Easy",    1.6f, 2, 7, 30};
    const invaders::GameWidget::Profile MEDIUM  {L"Medium",  1.8f, 2, 4, 35};
    const invaders::GameWidget::Profile CHINESE {L"Chinese", 2.0f, 2, 4, 40};

    loadProfile(EASY, window, settings);
    loadProfile(MEDIUM, window, settings);
    loadProfile(CHINESE, window, settings);

    window.initializeGL(width, height);
    window.setFullscreen(fullscreen);
    window.launchGame();

    using clock = std::chrono::high_resolution_clock;

    auto start = clock::now();
    
    while (window.isRunning())
    {
        const auto end  = clock::now();
        const auto gone = end - start;
        const auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(gone).count();

        window.updateGame(ms);
        window.renderGame();
    
        if (showFps)
        {
            static unsigned frames  = 0;
            static unsigned runtime = 0;
            frames++;
            runtime += ms;
            if (runtime >= 1000)
            {
                const float fps = frames / (runtime / 1000.0f);
                window.setFps(fps);
                runtime = 0;
                frames  = 0;
            }
        }
        window.pumpMessages();
        start = end;
    }

    settings.setValue("window/width",  window.getSurfaceWidth());
    settings.setValue("window/height", window.getSurfaceHeight());
    settings.setValue("window/fullscreen", window.isFullscreen());

    // tear down.
    window.close();

    QStringList level_names;
    for (unsigned i=0; ; ++i)
    {
        invaders::GameWidget::LevelInfo info;
        if (!window.getLevelInfo(info, i))
            break;

        level_names << QString::fromStdWString(info.name);
        settings.setValue(QString::fromStdWString(info.name + L"/highscore"), info.highScore);
        settings.setValue(QString::fromStdWString(info.name + L"/locked"), info.locked);
    }
    settings.setValue("game/levels", level_names);
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
