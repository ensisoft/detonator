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
#  include <QGuiApplication>
#  include <QScreen>
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
#include "editor/gui/animationwidget.h"

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

    const QList<QScreen*>& screens = QGuiApplication::screens();
    const QScreen* screen0 = screens[0];
    const QSize& size = screen0->availableVirtualSize();
    // try not to reposition the application to an offset that is not
    // within the current visible area
    if (xpos < size.width() && ypos < size.height())
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

    bool success = true;

    const auto& temp_file_list = mSettings.getValue("MainWindow", "temp_file_list", QStringList());
    for (const auto& file : temp_file_list)
    {
        Settings settings(file);
        if (!settings.Load())
        {
            success = false;
            continue;
        }
        const auto& class_name = settings.getValue("MainWindow", "class_name",
            QString(""));
        MainWidget* widget = nullptr;
        if (class_name == MaterialWidget::staticMetaObject.className())
            widget = new MaterialWidget;
        else if (class_name == ParticleEditorWidget::staticMetaObject.className())
            widget = new ParticleEditorWidget;
        else if (class_name == AnimationWidget::staticMetaObject.className())
            widget = new AnimationWidget;

        // bug, probably forgot to modify the if/else crap above.
        ASSERT(widget);

        if (!widget->loadState(settings))
        {
            WARN("Widget '%1 failed to load state.", widget->windowTitle());
        }
        attachWidget(widget);
        // remove the file, no longer needed.
        QFile::remove(file);
        DEBUG("Loaded widget '%1'", widget->windowTitle());
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
    if (QFileInfo("content.json").exists())
        mWorkspace.LoadContent("content.json");
    if (QFileInfo("workspace.json").exists())
        mWorkspace.LoadWorkspace("workspace.json");

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

        const auto& title = widget->windowTitle();
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

void MainWindow::on_actionNewMaterial_triggered()
{
    attachWidget(new MaterialWidget);
}

void MainWindow::on_actionNewParticleSystem_triggered()
{
    attachWidget(new ParticleEditorWidget);
}

void MainWindow::on_actionNewAnimation_triggered()
{
    attachWidget(new AnimationWidget);
}

void MainWindow::on_actionEditResource_triggered()
{
    const auto& indices = mUI.workspace->selectionModel()->selectedRows();
    for (int i=0; i<indices.size(); ++i)
    {
        const auto& res = mWorkspace.GetResource(indices[i].row());
        switch (res.GetType())
        {
            case app::Resource::Type::Material:
                attachWidget(new MaterialWidget(res));
                break;
            case app::Resource::Type::ParticleSystem:
                attachWidget(new ParticleEditorWidget(res, &mWorkspace));
                break;
            case app::Resource::Type::Animation:
                attachWidget(new AnimationWidget(res, &mWorkspace));
                break;
        }
    }
}

void MainWindow::on_actionDeleteResource_triggered()
{
    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Question);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setText(tr("Are you sure you want to delete the selected resources ?"));
    if (msg.exec() == QMessageBox::No)
        return;
    QModelIndexList selected = mUI.workspace->selectionModel()->selectedRows();
    mWorkspace.DeleteResources(selected);
}

void MainWindow::on_actionSaveWorkspace_triggered()
{
    if (!mWorkspace.SaveContent("content.json"))
    {
        ERROR("Failed to save workspace content in: '%1'", "content.json");
        return;
    }

    if (!mWorkspace.SaveWorkspace("workspace.json"))
    {
        ERROR("Failed to save workspace in '%1'", "workspace.json");
        return;
    }
    NOTE("Workspace saved in '%1 and '%2'", "content.json", "workspace.json");
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

void MainWindow::on_workspace_customContextMenuRequested(QPoint)
{
    const auto& indices = mUI.workspace->selectionModel()->selectedRows();
    mUI.actionDeleteResource->setEnabled(!indices.isEmpty());
    mUI.actionEditResource->setEnabled(!indices.isEmpty());

    QMenu menu(this);
    menu.addAction(mUI.actionNewMaterial);
    menu.addAction(mUI.actionNewParticleSystem);
    menu.addAction(mUI.actionNewAnimation);
    menu.addSeparator();
    menu.addAction(mUI.actionEditResource);
    menu.addSeparator();
    menu.addAction(mUI.actionDeleteResource);
    menu.exec(QCursor::pos());
}

void MainWindow::on_workspace_doubleClicked()
{
    on_actionEditResource_triggered();
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

    QStringList temp_file_list;

    const auto tabs = mUI.mainTab->count();
    for (int i=0; i<tabs; ++i)
    {
        const auto& temp = app::RandomString();
        const auto& path = app::GetAppFilePath("temp");
        const auto& file = app::GetAppFilePath("temp/" + temp + ".json");
        QDir dir;
        if (!dir.mkpath(path))
        {
            ERROR("Failed to create folder: '%1'", path);
            success = false;
            continue;
        }
        const auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));

        Settings settings(file);
        settings.setValue("MainWindow", "class_name", widget->metaObject()->className());
        if (!widget->saveState(settings))
        {
            ERROR("Failed to save widget '%1' settings.", widget->windowTitle());
            success = false;
            continue;
        }
        if (!settings.Save())
        {
            ERROR("Failed to save widget '%1' settings.",  widget->windowTitle());
            success = false;
            continue;
        }
        temp_file_list << file;
        DEBUG("Saved widget '%1'", widget->windowTitle());
    }
    mSettings.setValue("MainWindow", "temp_file_list", temp_file_list);

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

    // set the currently opened workspace.
    widget->setWorkspace(&mWorkspace);

    // rebuild window menu and shortcuts
    prepareWindowMenu();
}

} // namespace
