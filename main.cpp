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
#  include <QTime>
#  include <QtDebug>
#include "warnpop.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <thread>

#if defined(LINUX_OS)
#  include <fenv.h>
#endif
#include "mainwindow.h"

#if defined(ENABLE_AUDIO)
#  include "audio.h"

namespace invaders {

AudioPlayer* g_audio;

} // invaders

#endif // ENABLE_AUDIO

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

    invaders::MainWindow window;

    window.setMasterUnlock(masterUnlock);
    window.setUnlimitedWarps(unlimitedWarps);
    window.setUnlimitedBombs(unlimitedBombs);
    window.setShowFps(showFps);
    window.launchGame();
    window.show();

    const float TimeStep = 1000/60.0;

    unsigned frames  = 0;
    unsigned second  = 0;

    QTime stamp = QTime::currentTime();
    while (window.isVisible())
    {
        const auto time = QTime::currentTime();
        const auto msec = stamp.msecsTo(time);

        app.processEvents();

        float step = msec;
        while (step > 0.0f)
        {
            const auto dt = std::min(TimeStep, step);
            window.updateGame(dt);
            step -= dt;
            stamp = time;
        }

        frames++;
        second += msec;
        if (second >= 1000)
        {
            float fps = frames / (second / 1000.0f);
            //qDebug("Fps: %f", fps);
            frames = 0;
            second = 0;
            window.setFps(fps);
        }

        window.renderGame();

        // since we're drawing on the CPU our framerates will be all over the place
        // not to mention sucking all the CPU, we're going to do a little wait here
        // to simulate vsync and throttle the frames a little.
        const auto now = QTime::currentTime();
        const auto ms  = time.msecsTo(now);
        if (ms < TimeStep)
            std::this_thread::sleep_for(std::chrono::milliseconds(unsigned(TimeStep - ms)));

    }
    return 0;
    //return app.exec();
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