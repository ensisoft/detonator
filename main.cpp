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
#  include <QtGui/QApplication>
#  include <QStringList>
#include "warnpop.h"

#if defined(LINUX_OS)
#  include <fenv.h>
#endif
#include "mainwindow.h"

#if defined(ENABLE_AUDIO)
#  include "audio.h"

namespace invaders {

AudioPlayer* g_audio;

} // invaders

#endif

int main(int argc, char* argv[])
{
#ifdef ENABLE_AUDIO
    std::unique_ptr<invaders::AudioDevice> pa(new invaders::PulseAudio("Pinyin-Invaders"));
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

    bool masterUnlock = false;
    bool unlimitedWarps = false;
    bool unlimitedBombs = false;
    bool playSound = true;

    const auto& args = app.arguments();
    for (const auto& a : args)
    {
        if (a == "--unlock-all")
            masterUnlock = true;
        else if (a == "--unlimited-warps")
            unlimitedWarps = true;
        else if (a == "--unlimited-bombs")
            unlimitedBombs = true;
        else if (a == "--no-sound")
            playSound = false;
    }

    invaders::MainWindow window;

    window.setMasterUnlock(masterUnlock);
    window.setUnlimitedWarps(unlimitedWarps);
    window.setUnlimitedBombs(unlimitedBombs);
    window.setPlaySound(playSound);
    window.show();

    return app.exec();
}