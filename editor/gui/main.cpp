// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define LOGTAG "main"

#include "config.h"

#include "warnpush.h"
#  include <QCoreApplication>
#  include <QApplication>
#  include <QSurfaceFormat>
#  include <QStringList>
#  include <QFileInfo>
#  include <QDir>
#  include <QFile>
#  include <QTextStream>
#  include <QEventLoop>
#  include <QObject>
#  include <boost/version.hpp>
#  include <box2d/box2d.h>
#  include <nlohmann/json.hpp>
#  include <hb.h> // harfbuzz
#  include <sol/sol.hpp>
#include "warnpop.h"

#include <sstream>
#include <iostream>
#include <vector>

#include "base/logging.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/gui/mainwindow.h"

void copyright()
{
    const auto boost_major    = BOOST_VERSION / 100000;
    const auto boost_minor    = BOOST_VERSION / 100 % 1000;
    const auto boost_revision = BOOST_VERSION % 100;

    INFO("http://http://mpg123.de/");
    INFO("mpg123 - Fast console MPEG Audio Player and decoder library. 1.26.4");

    INFO("http://www.boost.org");
    INFO("Boost software library %1.%2.%3", boost_major, boost_minor, boost_revision);

    INFO("http://www.small-icons.com/stock-icons/16x16-free-application-icons.htm");
    INFO("http://www.aha-soft.com");
    INFO("Copyright (c) 2009 Aha-Soft");
    INFO("16x16 Free Application Icons");

    INFO("http://www.famfamfam.com/lab/icons/silk/");
    INFO("Silk Icon Set 1.3 Copyright (c) Mark James");

    INFO("http://qt.nokia.com");
    INFO("Qt cross-platform application and UI framework %1", QT_VERSION_STR);

    INFO("Copyright (C) 2013-2017 Mattia Basaglia <mattia.basaglia@gmail.com>");
    INFO("https://github.com/mbasaglia/Qt-Color-Widgets");
    INFO("Qt Color Widgets");

    INFO("Copyright (c) 2013-2019 Colin Duquesnoy");
    INFO("https://github.com/ColinDuquesnoy/QDarkStyleSheet");
    INFO("QDarkStyleSheet Dark Qt style 2.8");

    INFO("Copyright (c) 2019-2020 Waqar Ahmed -- <waqar.17a@gmail.com>");
    INFO("https://github.com/Waqar144/QSourceHighlite");
    INFO("Qt syntax highlighter");

    INFO("Copyright (c) 2005 - 2012 G-Truc Creation (www.g-truc.net)");
    INFO("https://github.com/g-truc/glm");
    INFO("OpenGL Mathematics (GLM) 0.9.9.8");

    INFO("Copyright (c) 2019 Erin Catto");
    INFO("https://box2d.org/");
    INFO("Box2D a 2D Physics Engine for Games %1.%2.%3", b2_version.major, b2_version.minor, b2_version.revision);

    INFO("Copyright (C) 2005-2017 Erik de Castro Lopo <erikd@mega-nerd.com>");
    INFO("http://libsndfile.github.io/libsndfile/");
    INFO("libsndfile C library for sampled audio data. 1.0.30");

    INFO("Copyright (c) 2012-2016, Erik de Castro Lopo <erikd@mega-nerd.com>");
    INFO("http://http://libsndfile.github.io/libsamplerate/");
    INFO("libsamplerate C library for audio resampling/sample rate conversion. 0.2.1");

    INFO("Copyright (c) 2013-2019 Niels Lohmann <http://nlohmann.me>");
    INFO("https://github.com/nlohmann/json");
    INFO("JSON for Modern C++ %1.%2.%3", NLOHMANN_JSON_VERSION_MAJOR,
         NLOHMANN_JSON_VERSION_MINOR, NLOHMANN_JSON_VERSION_MINOR);

    INFO("Copyright (c) 2019 Daniil Goncharov <neargye@gmail.com>");
    INFO("https://github.com/Neargye/magic_enum");
    INFO("Magic Enum C++ 0.6.4");

    INFO("Copyright (c) 2017 Sean Barrett");
    INFO("http://nothings.org/stb");
    INFO("Public domain image loader v2.23");

    INFO("Copyright (c) 2017 Sean Barrett");
    INFO("http://nothings.org/stb");
    INFO("Public domain image writer v1.13");

    INFO("Copyright © 2011  Google, Inc.");
    INFO("Harfbuzz text shaping library %1", HB_VERSION_STRING);

    INFO("Copyright (C) 1996-2020 by David Turner, Robert Wilhelm, and Werner Lemberg.");
    INFO("Freetype text rendering library 2.10.4");

    INFO("Copyright (C) 1994-2020 Lua.org, PUC-Rio.");
    INFO("Lua.org, PUC-Rio, Brazil (http://www.lua.org)");
    INFO("https://github.com/lua/lua");
    INFO("Lua %1.%2.%3", LUA_VERSION_MAJOR, LUA_VERSION_MINOR, LUA_VERSION_RELEASE);

    INFO("Copyright (c) 2013-2020 Rapptz, ThePhD, and contributors");
    INFO("https://github.com/ThePhD/sol2");
    INFO("https://sol2.rtfd.io");
    INFO("sol2 C++ Lua library binding. %1", SOL_VERSION_STRING);

    INFO("http://www.ensisoft.com");
    INFO("https://www.github.com/ensisoft/gamestudio");
    INFO("Compiler: %1 %2", COMPILER_NAME , COMPILER_VERSION);
    INFO("Compiled: " __DATE__ ", " __TIME__);
    INFO("Copyright (c) Sami Väisänen 2020-2021");
    INFO(APP_TITLE " " APP_VERSION);
}

class ForwardingLogger : public base::Logger
{
public:
    ForwardingLogger() : mLogger(std::cout)
    {
        mLogger.EnableTerminalColors(true);
    }

    virtual void Write(base::LogEvent type, const char* file, int line, const char* msg) override
    {
        // forward Error and warnings to the application log too.
        if (type == base::LogEvent::Error)
            ERROR(msg);
        else if (type == base::LogEvent::Warning)
            WARN(msg);
        else if (type == base::LogEvent::Info)
            INFO(msg);
        mLogger.Write(type, file, line, msg);
    }

    virtual void Write(base::LogEvent type, const char* msg) override
    {
        mLogger.Write(type, msg);
    }
    virtual void Flush() override
    {
        mLogger.Flush();
    }
private:
    base::OStreamLogger mLogger;
};

int main(int argc, char* argv[])
{
    try
    {
        // prefix with a . to make this a "hidden" dir
        // which is the convention on Linux
        app::InitializeAppHome("." APP_TITLE);

        // set the logger object for the subsystem to use, we'll
        // direct all this to the terminal for now.
        base::LockedLogger<ForwardingLogger> logger((ForwardingLogger()));
        base::SetGlobalLog(&logger);
        base::EnableDebugLog(true);
        DEBUG("It's alive!");

        copyright();

        // turn on Qt logging: QT_LOGGING_RULES = qt.qpa.gl
        // turns out this attribute is needed in order to make Qt
        // create a GLES2 context.
        // https://lists.qt-project.org/pipermail/interest/2015-February/015404.html
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        //QCoreApplication::setAttribute(Qt::AA_NativeWindows);

        // set the aliases for icon search paths
        QDir::setSearchPaths("icons", QStringList(":/16x16_ico_png"));
        QDir::setSearchPaths("level", QStringList(":/32x32_ico_png"));

        QSurfaceFormat format;
        format.setVersion(2, 0);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setRenderableType(QSurfaceFormat::OpenGLES);
        format.setDepthBufferSize(0); // currently we don't care
        format.setAlphaBufferSize(8);
        format.setRedBufferSize(8);
        format.setGreenBufferSize(8);
        format.setBlueBufferSize(8);
        format.setStencilBufferSize(8);
        format.setSamples(4);
        format.setSwapInterval(0);
        format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        QSurfaceFormat::setDefaultFormat(format);

        QApplication app(argc, argv);

        // Add a path for Qt to look for the plugins at runtime
        // note that this needs to be called *after* the QApplication object has been created.
        QCoreApplication::addLibraryPath(app::JoinPath(QCoreApplication::applicationDirPath(), "plugins"));

        // Create the application main window into which we add
        // main widgets.
        gui::MainWindow window(app);

        window.loadState();
        window.showWindow();

        // run the mainloop
        // this isn't the conventional way to run a qt based
        // application's main loop. normally one would just call
        // app.exec() but it seems to greatly degrade the performance
        // up to an order of magnitude difference in rendering perf
        // as measured by frames per second.
        // the problem with this type of loop however is that on a modern
        // machine with performant GPU were the GPU workloads are small
        // and without sync to VBLANK enabled we're basically going
        // to be running a busy loop here burning a lot cycles for nothing.
        while (!window.isClosed())
        {
            app.processEvents();
            if (window.isClosed())
                break;

            // why are we not calling iterateMainLoop directly here??
            // the problem has to do with modal dialogs. When a modal
            // dialog is open Qt enters a temporary event loop which
            // would mean that this code would not get a chance to render.
            // thus the iteration of the main loop code in the mainwindow
            // is triggered by an event posted to the application queue

            if (!window.haveAcceleratedWindows())
            {
                DEBUG("Enter slow event loop.");
                // enter a temporary "slow" event loop until there are again
                // windows that require "acceleration" i.e. continuous game loop
                // style processing.
                QEventLoop loop;
                QObject::connect(&window, &gui::MainWindow::newAcceleratedWindowOpen, &loop, &QEventLoop::quit);
                QObject::connect(&window, &gui::MainWindow::aboutToClose, &loop, &QEventLoop::quit);
                loop.exec();
                DEBUG("Exit slow event loop.");
            }
        }

        DEBUG("Exiting...");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Oops... something went wrong.";
        std::cerr << e.what();
        std::cerr << std::endl;
    }
    std::cout << "Have a good day.\n";
    std::cout << std::endl;
    return 0;
}


