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
#include "warnpop.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
#include <filesystem>

#include "gamelib/loader.h"
#include "audio/player.h"
#include "audio/device.h"
#include "base/logging.h"
#include "misc/homedir.h"
#include "misc/settings.h"

#if defined(LINUX_OS)
#  include <fenv.h>
#endif
#include "gamewidget.h"

namespace invaders {

    audio::AudioPlayer* g_audio;
    game::ResourceLoader* g_loader;

} // invaders



void loadProfile(invaders::GameWidget::Profile profile,
                 invaders::GameWidget& widget,
                 misc::Settings& settings)
{
    const auto& profile_name = base::ToUtf8(profile.name);
    profile.speed         = settings.GetValue(profile_name, "speed", profile.speed);
    profile.spawnCount    = settings.GetValue(profile_name, "spawnCount", profile.spawnCount);
    profile.spawnInterval = settings.GetValue(profile_name, "spawnInterval", profile.spawnInterval);
    profile.numEnemies    = settings.GetValue(profile_name, "enemyCount", profile.numEnemies);

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

    game::ResourceLoader loader;
    invaders::g_loader = &loader;

    // todo: resolve the directory where the executable is.
    // currently expects the current working directory to contain the content.json file
    invaders::g_loader->LoadResources(".", "content.json");
    gfx::SetResourceLoader(invaders::g_loader);
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

    misc::HomeDir::Initialize(".pinyin-invaders");
    misc::Settings settings;

    if (base::FileExists(misc::HomeDir::MapFile("settings.json")))
        settings.LoadFromFile(misc::HomeDir::MapFile("settings.json"));

    const auto width      = settings.GetValue("window", "width", 1200);
    const auto height     = settings.GetValue("window", "height", 700);
    const auto fullscreen = settings.GetValue("window", "fullscreen", false);
    const auto playSound  = settings.GetValue("audio", "sound", true);
    const auto playMusic  = settings.GetValue("audio", "music", true);
    const auto& levels    = settings.GetValue("game", "levels", std::vector<std::string>());

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
        const auto& level_name = levels[i];
        invaders::GameWidget::LevelInfo info;
        info.name      = base::FromUtf8(level_name);
        info.highScore = settings.GetValue(level_name, "highscore", 0);
        info.locked    = settings.GetValue(level_name, "locked", true);
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

    settings.SetValue("window", "width",  window.getSurfaceWidth());
    settings.SetValue("window", "height", window.getSurfaceHeight());
    settings.SetValue("window", "fullscreen", window.isFullscreen());

    // tear down.
    window.close();

    std::vector<std::string> level_names;
    for (unsigned i=0; ; ++i)
    {
        invaders::GameWidget::LevelInfo info;
        if (!window.getLevelInfo(info, i))
            break;
        const auto& level_name = base::ToUtf8(info.name);
        level_names.push_back(level_name);
        settings.SetValue(level_name, "highscore", info.highScore);
        settings.SetValue(level_name, "locked", info.locked);
    }
    settings.SetValue("game", "levels", level_names);
    settings.SetValue("audio", "sound", window.getPlaySounds());
    settings.SetValue("audio", "music", window.getPlayMusic());

    settings.SaveToFile(misc::HomeDir::MapFile("settings.json"));

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
