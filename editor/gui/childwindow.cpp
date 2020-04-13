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

#define LOGTAG "childwindow"

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include "base/assert.h"
#include "editor/app/eventlog.h"
#include "editor/gui/childwindow.h"
#include "editor/gui/mainwidget.h"

namespace gui
{

ChildWindow::ChildWindow(MainWidget* widget) : mWidget(widget)
{
    const auto& icon = mWidget->windowIcon();
    const auto& text = mWidget->windowTitle();
    const auto* klass = mWidget->metaObject()->className();
    DEBUG("Create new child window (widget=%1) '%2'", klass, text);

    mUI.setupUi(this);
    setWindowTitle(text);
    setWindowIcon(icon);

    mUI.verticalLayout->addWidget(widget);
    QString menu_name = klass;
    menu_name.remove("gui::");
    menu_name.remove("Widget");
    mUI.menuTemp->setTitle(menu_name);

    mWidget->activate();
    mWidget->addActions(*mUI.toolBar);
    mWidget->addActions(*mUI.menuTemp);

    // We need to install this event filter so that we can universally grab
    // Mouse wheel up/down + Ctrl and conver these into zoom in/out actions.
    mWidget->installEventFilter(this);
}

ChildWindow::~ChildWindow()
{
    const auto& text  = mWidget->windowTitle();
    const auto& klass = mWidget->metaObject()->className();
    DEBUG("Destroy child window (widget=%1) '%2'", klass, text);

    mUI.verticalLayout->removeWidget(mWidget);

    mWidget->shutdown();
    mWidget->setParent(nullptr);
    delete mWidget;
}

void ChildWindow::RefreshUI()
{
    mWidget->refresh();
    // update the window icon and title if they have changed.
    const auto& icon = mWidget->windowIcon();
    const auto& text = mWidget->windowTitle();
    setWindowTitle(text);
    setWindowIcon(icon);
}

void ChildWindow::on_actionClose_triggered()
{
    // this will create close event
    close();
}

void ChildWindow::on_actionReloadShaders_triggered()
{
    mWidget->reloadShaders();
    INFO("'%1' shaders reloaded.", mWidget->windowTitle());
}

void ChildWindow::closeEvent(QCloseEvent* event)
{
    // for now just accept, later on we could ask the user when there
    // are pending changes whether he'd like to save his shit
    event->accept();

    // we could emit an event here to indicate that the window
    // is getting closed but that's a sure-fire way of getting
    // unwanted recursion that will fuck shit up.  (i.e this window
    // getting deleted which will run the destructor which will
    // make this function have an invalided *this* pointer. bad.
    // so instead of doing that we just set a flag and the
    // mainwindow will check from time to if the window object
    // should be deleted.
    mClosed = true;
}

bool ChildWindow::eventFilter(QObject* destination, QEvent* event)
{
    if (destination != mWidget)
        return QObject::eventFilter(destination, event);

    if (event->type() != QEvent::Wheel)
        return QObject::eventFilter(destination, event);

    const auto* wheel = static_cast<QWheelEvent*>(event);
    const auto mods = wheel->modifiers();
    if (mods != Qt::ControlModifier)
        return QObject::eventFilter(destination, event);

    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_zoom_steps = num_steps.y();

    //DEBUG("Zoom steps: %1", num_zoom_steps);

    for (int i=0; i<std::abs(num_zoom_steps); ++i)
    {
        if (num_zoom_steps > 0)
            mWidget->zoomIn();
        else if (num_zoom_steps < 0)
            mWidget->zoomOut();
    }

    return true;
}

} // namespace
