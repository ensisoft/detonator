// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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
#  include <boost/version.hpp>
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

    INFO("http://www.ensisoft.com");
    INFO("https://www.github.com/ensisoft/pinyin-invaders");
    INFO("Compiler: %1 %2", COMPILER_NAME , COMPILER_VERSION);
    INFO("Compiled: " __DATE__ ", " __TIME__);
    INFO("Copyright (c) Sami Väisänen 2020");
    INFO(APP_TITLE " " APP_VERSION);

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
}

class ForwardingLogger : public base::Logger
{
public:
    virtual void Write(base::LogEvent type, const char* file, int line, const char* msg) const override
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

    virtual void Write(base::LogEvent type, const char* msg) const override
    {
        mLogger.Write(type, msg);
    }
    virtual void Flush() override
    {
        mLogger.Flush();
    }
private:
    base::CursesLogger mLogger;
};

int main(int argc, char* argv[])
{
    try
    {
        bool use_dark_style = true;
        for (int i=1; i<argc; ++i)
        {
            if (!std::strcmp("--no-dark-style", argv[i]))
                use_dark_style = false;
        }

        // prefix with a . to make this a "hidden" dir
        // which is the convention on Linux
        app::InitializeAppHome("." APP_TITLE);

        // set the logger object for the subsystem to use, we'll
        // direct all this to the terminal for now.
        //base::CursesLogger logger;
        ForwardingLogger logger;
        base::SetGlobalLog(&logger);
        base::EnableDebugLog(true);
        DEBUG("It's alive!");

        copyright();

        // turn on Qt logging: QT_LOGGING_RULES = qt.qpa.gl
        // turns out this attribute is needed in order to make Qt
        // create a GLES2 context.
        // https://lists.qt-project.org/pipermail/interest/2015-February/015404.html
        //QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
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
        QSurfaceFormat::setDefaultFormat(format);

        QApplication app(argc, argv);

        // Add a path for Qt to look for the plugins at runtime
        // note that this needs to be called *after* the QApplication object has been created.
        QCoreApplication::addLibraryPath(app::JoinPath(QCoreApplication::applicationDirPath(), "plugins"));

        if (use_dark_style)
        {
            QFile style(":qdarkstyle/style.qss");
            if (style.open(QIODevice::ReadOnly))
            {
                QTextStream stream(&style);
                app.setStyleSheet(stream.readAll());
                INFO("Loaded qdarkstyle. Start with --no-dark-style to disable.");
            }
            else
            {
                ERROR("Failed to load qdarkstyle.");
            }
        }
        // Create the application main window into which we add
        // main widgets.
        gui::MainWindow window;

        window.loadState();
        window.prepareFileMenu();
        window.prepareWindowMenu();
        window.prepareMainTab();
        window.showWindow();

        // run the mainloop
        app.exec();

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


