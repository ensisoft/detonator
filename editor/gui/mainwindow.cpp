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

#define LOGTAG "mainwindow"

#include "config.h"

#include "warnpush.h"
#  include <QDesktopWidget>
#  include <QMessageBox>
#  include <QFileInfo>
#  include <QWheelEvent>
#  include <QFileInfo>
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"
#include "editor/app/format.h"
#include "editor/app/utility.h"
#include "editor/app/eventlog.h"
#include "editor/gui/mainwindow.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/settings.h"
#include "editor/gui/particlewidget.h"
#include "editor/gui/materialwidget.h"

namespace gui
{

MainWindow::MainWindow(Settings& settings) : mSettings(settings)
{
    mUI.setupUi(this);
    mUI.actionExit->setShortcut(QKeySequence::Quit);
    mUI.actionWindowClose->setShortcut(QKeySequence::Close);
    mUI.actionWindowNext->setShortcut(QKeySequence::Forward);
    mUI.actionWindowPrev->setShortcut(QKeySequence::Back);
    mUI.statusbar->setVisible(true);
    mUI.mainToolBar->setVisible(true);
    mUI.actionViewToolbar->setChecked(true);
    mUI.actionViewStatusbar->setChecked(true);

    // start periodic refresh timer
    QObject::connect(&mTimer, SIGNAL(timeout()), this, SLOT(refreshUI()));
    mTimer.setInterval(500);
    mTimer.start();

    auto& events = app::EventLog::get();
    QObject::connect(&events, SIGNAL(newEvent(const app::Event&)),
        this, SLOT(showNote(const app::Event&)));
    mUI.eventlist->setModel(&events);
}

MainWindow::~MainWindow()
{}

void MainWindow::closeWidget(MainWidget* widget)
{
    const auto index = mUI.mainTab->indexOf(widget);
    if (index == -1)
        return;
    mUI.mainTab->removeTab(index);
}

void MainWindow::loadState()
{
    const auto xdim = mSettings.getValue("MainWindow", "width",  width());
    const auto ydim = mSettings.getValue("MainWindow", "height", height());
    const auto xpos = mSettings.getValue("MainWindow", "xpos", x());
    const auto ypos = mSettings.getValue("MainWindow", "ypos", y());

    QDesktopWidget desktop;
    const auto screen = desktop.availableGeometry();
    if (xpos < screen.width() -50 && ypos < screen.height() - 50)
        move(xpos, ypos);

    resize(xdim, ydim);

    const auto show_statbar = mSettings.getValue("MainWindow", "show_statusbar", true);
    const auto show_toolbar = mSettings.getValue("MainWindow", "show_toolbar", true);
    const auto show_eventlog = mSettings.getValue("MainWindow", "show_event_log", true);
    const auto show_workspace = mSettings.getValue("MainWindow", "show_workspace", true);
    mUI.mainToolBar->setVisible(show_toolbar);
    mUI.statusbar->setVisible(show_statbar);
    mUI.eventlogDock->setVisible(show_eventlog);
    mUI.workspaceDock->setVisible(show_workspace);
    mUI.actionViewToolbar->setChecked(show_toolbar);
    mUI.actionViewStatusbar->setChecked(show_statbar);
    mUI.actionViewEventlog->setChecked(show_eventlog);
    mUI.actionViewWorkspace->setChecked(show_workspace);

    // todo: load the previously open tabs from temporary state files.


    if (show_eventlog)
    {

    }

    bool success = true;
    if (!success)
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("There was a problem restoring the application state.\r\n"
            "Some state might be inconsistent. Sorry about that.\r\n"
            "See the application log for more details."));
        msg.exec();
    }
}

void MainWindow::focusWidget(const MainWidget* widget)
{
    const auto index = mUI.mainTab->indexOf(const_cast<MainWidget*>(widget));
    if (index == -1)
        return;
    mUI.mainTab->setCurrentIndex(index);
}

void MainWindow::prepareFileMenu()
{

}

void MainWindow::prepareWindowMenu()
{
    mUI.menuWindow->clear();

    const auto count = mUI.mainTab->count();
    const auto curr  = mUI.mainTab->currentIndex();
    for (int i=0; i<count; ++i)
    {
        const auto& text = mUI.mainTab->tabText(i);
        const auto& icon = mUI.mainTab->tabIcon(i);
        QAction* action  = mUI.menuWindow->addAction(icon, text);
        action->setCheckable(true);
        action->setChecked(i == curr);
        action->setProperty("tab-index", i);
        if (i < 9)
            action->setShortcut(QKeySequence(Qt::ALT | (Qt::Key_1 + i)));

        QObject::connect(action, SIGNAL(triggered()),
            this, SLOT(actionWindowFocus_triggered()));
    }
    // and this is in the window menu
    mUI.menuWindow->addSeparator();
    mUI.menuWindow->addAction(mUI.actionWindowClose);
    mUI.menuWindow->addAction(mUI.actionWindowNext);
    mUI.menuWindow->addAction(mUI.actionWindowPrev);
}

void MainWindow::prepareMainTab()
{
    mUI.mainTab->setCurrentIndex(0);
}

void MainWindow::startup()
{
    if (QFileInfo("content.json").exists())
        mWorkspace.LoadContent("content.json");
    if (QFileInfo("workspace.json").exists())
        mWorkspace.LoadWorkspace("workspace.json");

    attachWidget(new MaterialWidget);
    attachWidget(new ParticleEditorWidget);

    const auto num_widgets = mUI.mainTab->count();
    for (int i=0; i<num_widgets; ++i)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        widget->startup();
        widget->setWorkspace(&mWorkspace);
    }
    mUI.workspace->setModel(mWorkspace.GetResourceModel());
}

void MainWindow::showWindow()
{
    show();
}

void MainWindow::on_mainTab_currentChanged(int index)
{
    if (mCurrentWidget)
        mCurrentWidget->deactivate();

    mCurrentWidget = nullptr;
    mUI.mainToolBar->clear();
    mUI.menuTemp->clear();

    if (index != -1)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(index));

        widget->activate();
        widget->addActions(*mUI.mainToolBar);
        widget->addActions(*mUI.menuTemp);

        auto title = widget->windowTitle();
        auto space = title.indexOf(" ");
        if (space != -1)
            title.resize(space);

        mUI.menuTemp->setTitle(title);
        mCurrentWidget = widget;
    }

    // add the stuff that is always in the edit menu
    mUI.mainToolBar->addSeparator();

    prepareWindowMenu();

}

void MainWindow::on_mainTab_tabCloseRequested(int index)
{
    auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(index));
    if (widget == mCurrentWidget)
        mCurrentWidget = nullptr;

    // does not delete the widget.
    mUI.mainTab->removeTab(index);

    widget->shutdown();
    widget->setParent(nullptr);
    delete widget;

    // rebuild window menu.
    prepareWindowMenu();

}

void MainWindow::on_actionExit_triggered()
{
    this->close();
}

void MainWindow::on_actionWindowClose_triggered()
{
    const auto cur = mUI.mainTab->currentIndex();
    if (cur == -1)
        return;

    on_mainTab_tabCloseRequested(cur);
}

void MainWindow::on_actionWindowNext_triggered()
{
    // cycle to next tab in the maintab

    const auto cur = mUI.mainTab->currentIndex();
    if (cur == -1)
        return;

    const auto size = mUI.mainTab->count();
    const auto next = (cur + 1) % size;
    mUI.mainTab->setCurrentIndex(next);
}

void MainWindow::on_actionWindowPrev_triggered()
{
    // cycle to previous tab in the maintab

    const auto cur = mUI.mainTab->currentIndex();
    if (cur == -1)
        return;

    const auto size = mUI.mainTab->count();
    const auto prev = (cur == 0) ? size - 1 : cur - 1;
    mUI.mainTab->setCurrentIndex(prev);
}

void MainWindow::on_actionZoomIn_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->zoomIn();
}
void MainWindow::on_actionZoomOut_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->zoomOut();
}

void MainWindow::on_actionReloadShaders_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->reloadShaders();
}


void MainWindow::actionWindowFocus_triggered()
{
    // this signal comes from an action in the
    // window menu. the index is the index of the widget
    // in the main tab. the menu is rebuilt
    // when the maintab configuration changes.

    auto* action = static_cast<QAction*>(sender());
    const auto tab_index = action->property("tab-index").toInt();

    action->setChecked(true);

    mUI.mainTab->setCurrentIndex(tab_index);
}

void MainWindow::refreshUI()
{
    const auto num_widgets = mUI.mainTab->count();

    // refresh the UI state, and update the tab widget icon/text
    // if needed.
    for (int i=0; i<num_widgets; ++i)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        widget->refresh();
        const auto& icon = widget->windowIcon();
        const auto& text = widget->windowTitle();
        mUI.mainTab->setTabText(i, text);
        mUI.mainTab->setTabIcon(i, icon);
    }
}

void MainWindow::showNote(const app::Event& event)
{
    if (event.type == app::Event::Type::Note)
    {
        mUI.statusbar->showMessage(event.message, 5000);
    }
}



void MainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();

    // try to perform an orderly shutdown.
    if (!saveState())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("There was a problem saving the application state.\r\n"
            "Do you still want to quit the application?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    while (mUI.mainTab->count())
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(0));
        widget->shutdown();
        widget->setParent(nullptr);
        // cleverly enough this will remove the tab. so the loop
        // here must be carefully done to access the tab at index 0
        delete widget;
    }
    mUI.mainTab->clear();

    event->accept();
}

bool MainWindow::eventFilter(QObject* destination, QEvent* event)
{
    if (destination != mCurrentWidget)
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
            mCurrentWidget->zoomIn();
        else if (num_zoom_steps < 0)
            mCurrentWidget->zoomOut();
    }

    return true;
}

bool MainWindow::saveState()
{
    if (!mWorkspace.SaveContent("content.json"))
        return false;
    if (!mWorkspace.SaveWorkspace("workspace.json"))
        return false;

    mSettings.setValue("MainWindow", "width", width());
    mSettings.setValue("MainWindow", "height", height());
    mSettings.setValue("MainWindow", "xpos", x());
    mSettings.setValue("MainWindow", "ypos", y());
    mSettings.setValue("MainWindow", "show_toolbar", mUI.mainToolBar->isVisible());
    mSettings.setValue("MainWindow", "show_statusbar", mUI.statusbar->isVisible());
    mSettings.setValue("MainWindow", "show_eventlog", mUI.eventlogDock->isVisible());
    mSettings.setValue("MainWindow", "show_workspace", mUI.workspaceDock->isVisible());

    bool success = true;

    // todo: for open tabs generate a temporary session file where the state goes.

    return success;
}

void MainWindow::attachWidget(MainWidget* widget)
{
    Q_ASSERT(!widget->parent());
    const auto& text = widget->windowTitle();
    const auto& icon = widget->windowIcon();

    const auto count = mUI.mainTab->count();
    mUI.mainTab->addTab(widget, icon, text);
    mUI.mainTab->setCurrentIndex(count);

    // We need to install this event filter so that we can universally grab
    // Mouse wheel up/down + Ctrl and conver these into zoom in/out actions.
    widget->installEventFilter(this);

    // rebuild window menu and shortcuts
    prepareWindowMenu();
}

} // namespace
