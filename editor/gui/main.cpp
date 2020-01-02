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
#  include <QApplication>
#  include <QSurfaceFormat>
#  include <QStringList>
#  include <QFileInfo>
#  include <QDir>
#  include <boost/version.hpp>
#include "warnpop.h"

#include <sstream>
#include <iostream>
#include <vector>

#include "base/logging.h"
#include "app/eventlog.h"
#include "app/utility.h"
#include "settings.h"
#include "eventwidget.h"
#include "mainwindow.h"

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
}

int main(int argc, char* argv[])
{
    // set the logger object for the subsystem to use, we'll 
    // direct all this to the terminal for now.
    base::CursesLogger logger;
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);
    DEBUG("It's alive!");

    copyright();

    try
    {

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
        QSurfaceFormat::setDefaultFormat(format);

        QApplication app(argc, argv);

        // Load master settings.
        gui::Settings settings("Ensisoft", APP_TITLE);

        // Create the application main window into which we add
        // main widgets.
        gui::MainWindow window(settings);
    
        // Event widget
        gui::EventWidget events;
        window.attachPermanentWidget(&events);

        window.loadState();
        window.prepareFileMenu();
        window.prepareWindowMenu();
        window.prepareMainTab();
        window.startup();
        window.showWindow();        

        // run the mainloop
        app.exec();

        // its important to keep in mind that the modules and widgets
        // are created simply on the stack here and what is the actual
        // destruction order. our mainwindow outlives the widgets and modules
        // so before we start destructing those objects its better to make clean
        // all the references to those in the mainwindow.        
        window.detachAllWidgets();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Oops... something went wrong.";
        std::cerr << std::endl;
    }
    DEBUG("Exiting...");
    return 0;
}


