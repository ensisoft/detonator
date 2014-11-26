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
#include "warnpop.h"

#include "mainwindow.h"

namespace invaders
{

MainWindow::MainWindow()
{
    ui_.setupUi(this);
    QSettings settings("Ensisoft", "Invaders");
    const auto wwidth  = settings.value("window/width", width()).toInt();
    const auto wheight = settings.value("window/height", height()).toInt();
    const auto xpos = settings.value("window/xpos", x()).toInt();
    const auto ypos = settings.value("window/ypos", y()).toInt();
    resize(wwidth, wheight);
    move(xpos, ypos);

    QObject::connect(ui_.game, SIGNAL(quitGame()), 
        this, SLOT(close()));

    const auto& inst = QApplication::applicationDirPath();
    const auto& data = inst + "/data/";

    QDir dir(data);
    QStringList filters("level_*.txt");
    QStringList entries = dir.entryList(filters);
    for ( const auto& entry : entries)
    {
        const auto& file = data + entry;
        ui_.game->loadLevel(file); 
    }
}


MainWindow::~MainWindow()
{
    QSettings settings("Ensisoft", "Invaders");
    settings.setValue("window/width", width());
    settings.setValue("window/height", height());
    settings.setValue("window/xpos", x());
    settings.setValue("window/ypos", y());
}

} // invaders