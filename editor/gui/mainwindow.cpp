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
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"

#include "app/format.h"
#include "app/utility.h"
#include "app/eventlog.h"
#include "mainwindow.h"
#include "mainwidget.h"
#include "settings.h"

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
}

MainWindow::~MainWindow()
{}


void MainWindow::attachPermanentWidget(MainWidget* widget)
{
    Q_ASSERT(!widget->parent());

    const int index  = mWidgets.size();
    const auto& text = widget->windowTitle();
    const auto& icon = widget->windowIcon();

    // Each permanent widget gets a view action.
    // Permanent widgets are those that instead of being closed
    // are just hidden and toggled in the View menu.
    QAction* action = mUI.menuView->addAction(text);
    action->setCheckable(true);
    action->setChecked(true);
    action->setProperty("index", index);

    QObject::connect(action, SIGNAL(triggered()), this,
        SLOT(actionWindowToggleView_triggered()));

    mWidgets.push_back(widget);
    mActions.push_back(action);
    widget->setProperty("permanent", true);
}

void MainWindow::attachSessionWidget(std::unique_ptr<MainWidget> widget)
{
    Q_ASSERT(!widget->parent());

    widget->setProperty("permanent", false);

    mWidgets.push_back(widget.get());

    const auto& text = widget->windowTitle();
    const auto& icon = widget->windowIcon();

    const auto count = mUI.mainTab->count();
    mUI.mainTab->addTab(widget.get(), icon, text);
    mUI.mainTab->setCurrentIndex(count);
    widget.release();
}

void MainWindow::detachAllWidgets()
{
    mUI.mainTab->clear();

    for (auto* widget : mWidgets)
    {
        widget->setParent(nullptr);
        const auto permanent = widget->property("permanent").toBool();
        if (!permanent) 
            delete widget;
    }
    mWidgets.clear();
}

void MainWindow::closeWidget(MainWidget* widget)
{
    const auto index = mUI.mainTab->indexOf(widget);
    if (index == -1)
        return;
    on_mainTab_tabCloseRequested(index);
}

void MainWindow::loadState()
{
    const auto xdim = mSettings.get("MainWindow", "width",  width());
    const auto ydim = mSettings.get("MainWindow", "height", height());
    const auto xpos = mSettings.get("MainWindow", "xpos", x());
    const auto ypos = mSettings.get("MainWindow", "ypos", y());

    QDesktopWidget desktop;
    const auto screen = desktop.availableGeometry();
    if (xpos < screen.width() -50 && ypos < screen.height() - 50)
        move(xpos, ypos);

    resize(xdim, ydim);

    const auto show_statbar = mSettings.get("window", "show_statusbar", true);
    const auto show_toolbar = mSettings.get("window", "show_toolbar", true);
    mUI.mainToolBar->setVisible(show_toolbar);
    mUI.statusbar->setVisible(show_statbar);
    mUI.actionViewToolbar->setChecked(show_toolbar);
    mUI.actionViewStatusbar->setChecked(show_statbar);

    for (std::size_t i=0; i<mWidgets.size(); ++i)
    {
        const auto text = mWidgets[i]->objectName();
        Q_ASSERT(!text.isEmpty());
        const auto icon = mWidgets[i]->windowIcon();
        //const auto info = mWidgets[i]->getInformation();
        const auto show = mSettings.get("window_visible_tabs", text, true);
        if (show)
        {
            const auto title = mWidgets[i]->windowTitle();
            mUI.mainTab->insertTab(i, mWidgets[i], icon, title);
        }
        if (i < mActions.size())
            mActions[i]->setChecked(show);

        mWidgets[i]->loadState(mSettings);
    }


    bool success = true;

    for (auto* widget : mWidgets)
    {
        DEBUG("Loading widget '%1'", widget->windowTitle());

        success |= widget->loadState(mSettings);
    }
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
    for (auto* widget : mWidgets)
    {
        widget->startup();
    }
}

void MainWindow::showWindow()
{
    show();   
}


void MainWindow::showWidget(const QString& title)
{
    const auto it = std::find_if(std::begin(mWidgets), std::end(mWidgets),
        [&](const MainWidget* widget) {
            return widget->windowTitle() == title;
        });
    if (it == std::end(mWidgets))
        return;

    showWidget(std::distance(std::begin(mWidgets), it));
}

void MainWindow::showWidget(MainWidget* widget)
{
    auto it = std::find(std::begin(mWidgets), std::end(mWidgets), widget);
    if (it == std::end(mWidgets))
        return;

    showWidget(std::distance(std::begin(mWidgets), it));
}

void MainWindow::showWidget(std::size_t index)
{
    //BOUNDSCHECK(mWidgets, index);
    //BOUNDSCHECK(mActions, index);
    auto* widget = mWidgets[index];
    auto* action = mActions[index];

    // if already show, then do nothing
    auto pos = mUI.mainTab->indexOf(widget);
    if (pos != -1)
        return;

    action->setChecked(true);

    const auto& icon = widget->windowIcon();
    const auto& text = widget->windowTitle();
    mUI.mainTab->insertTab(index, widget, icon, text);
    prepareWindowMenu();

}

void MainWindow::hideWidget(MainWidget* widget)
{
    auto it = std::find(std::begin(mWidgets), std::end(mWidgets), widget);

    Q_ASSERT(it != std::end(mWidgets));

    auto index = std::distance(std::begin(mWidgets), it);

    hideWidget(index);
}

void MainWindow::hideWidget(std::size_t index)
{
    auto* widget = mWidgets[index];
    auto* action = mActions[index];

    // if already not shown then do nothing
    auto pos = mUI.mainTab->indexOf(widget);
    if (pos == -1)
        return;

    action->setChecked(false);

    mUI.mainTab->removeTab(pos);

    prepareWindowMenu();
}


void MainWindow::on_mainTab_currentChanged(int index)
{
    if (mCurrentWidget)
        mCurrentWidget->deactivate();

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
    const auto* widget   = static_cast<MainWidget*>(mUI.mainTab->widget(index));
    const auto parent    = widget->property("parent-object");
    const auto permanent = widget->property("permanent").toBool();

    if (widget == mCurrentWidget)
        mCurrentWidget = nullptr;

    mUI.mainTab->removeTab(index);

    // find in the array
    const auto it = std::find(std::begin(mWidgets), std::end(mWidgets), widget);

    ASSERT(it != std::end(mWidgets));
    ASSERT(*it == widget);

    if (permanent)
    {
        const auto index = std::distance(std::begin(mWidgets), it);
        //BOUNDSCHECK(mActions, index);

        auto* action = mActions[index];
        action->setChecked(false);
    }
    else
    {
        delete widget;
        mWidgets.erase(it);
    }
    prepareWindowMenu();

    // see if we have a parent
    if (parent.isValid())
    {
        auto* p = parent.value<QObject*>();

        auto it = std::find_if(std::begin(mWidgets), std::end(mWidgets),
            [&](MainWidget* w) {
                return static_cast<QObject*>(w) == p;
            });
        if (it != std::end(mWidgets))
            focusWidget(*it);
    }
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

void MainWindow::actionWindowToggleView_triggered()
{
    // the signal comes from the action object in
    // the view menu. the index is the index to the widgets array

    const auto* action = static_cast<QAction*>(sender());
    const auto index   = action->property("index").toInt();
    const bool visible = action->isChecked();

    //BOUNDSCHECK(mWidgets, index);
    //BOUNDSCHECK(mActions, index);

    if (visible)
    {
        showWidget(index);
    }
    else
    {
        hideWidget(index);
    }
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
    // refresh the UI state.

    for (auto* w : mWidgets)
    {
        w->refresh();
    }

    // update the maintab
    const auto numVisibleTabs = mUI.mainTab->count();

    for (int i=0; i<numVisibleTabs; ++i)
    {
        const auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        const auto& icon   = widget->windowIcon();
        const auto& text   = widget->windowTitle();
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

    for (auto* widget : mWidgets)
    {
        widget->shutdown();
    }

    event->accept();
}

bool MainWindow::saveState()
{
    bool success = true;

    mSettings.set("MainWindow", "width", width());
    mSettings.set("MainWindow", "height", height());
    mSettings.set("MainWindow", "xpos", x());
    mSettings.set("MainWindow", "ypos", y());
    mSettings.set("window", "show_toolbar", mUI.mainToolBar->isVisible());
    mSettings.set("window", "show_statusbar", mUI.statusbar->isVisible());    
 
    for (const auto* widget : mWidgets)
    {
        if (widget->property("permanent").toBool())
        {
            DEBUG("Saving widget '%1'", widget->windowTitle());
            success |= widget->saveState(mSettings);
        }

    }
    return success;
}



} // namespace
