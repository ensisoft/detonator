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
#include "editor/gui/childwindow.h"
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
    const auto window_xdim = mSettings.getValue("MainWindow", "width",  width());
    const auto window_ydim = mSettings.getValue("MainWindow", "height", height());
    const auto window_xpos = mSettings.getValue("MainWindow", "xpos", x());
    const auto window_ypos = mSettings.getValue("MainWindow", "ypos", y());
    const auto show_statbar = mSettings.getValue("MainWindow", "show_statusbar", true);
    const auto show_toolbar = mSettings.getValue("MainWindow", "show_toolbar", true);
    const auto show_eventlog = mSettings.getValue("MainWindow", "show_event_log", true);
    const auto show_workspace = mSettings.getValue("MainWindow", "show_workspace", true);

    const QList<QScreen*>& screens = QGuiApplication::screens();
    const QScreen* screen0 = screens[0];
    const QSize& size = screen0->availableVirtualSize();
    // try not to reposition the application to an offset that is not
    // within the current visible area
    if (window_xpos < size.width() && window_ypos < size.height())
        move(window_xpos, window_ypos);

    resize(window_xdim, window_ydim);

    mUI.mainToolBar->setVisible(show_toolbar);
    mUI.statusbar->setVisible(show_statbar);
    mUI.eventlogDock->setVisible(show_eventlog);
    mUI.workspaceDock->setVisible(show_workspace);
    mUI.actionViewToolbar->setChecked(show_toolbar);
    mUI.actionViewStatusbar->setChecked(show_statbar);
    mUI.actionViewEventlog->setChecked(show_eventlog);
    mUI.actionViewWorkspace->setChecked(show_workspace);

    if (!loadWorkspace())
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

bool MainWindow::loadWorkspace()
{
    if (QFileInfo("content.json").exists())
        mWorkspace.LoadContent("content.json");
    if (QFileInfo("workspace.json").exists())
        mWorkspace.LoadWorkspace("workspace.json");

    // desktop dimensions
    const QList<QScreen*>& screens = QGuiApplication::screens();
    const QScreen* screen0 = screens[0];
    const QSize& size = screen0->availableVirtualSize();

    // Load workspace windows and their content.
    bool success = true;

    const auto& session_files = mWorkspace.GetProperty("session_files", QStringList());
    for (const auto& file : session_files)
    {
        Settings settings(file);
        if (!settings.Load())
        {
            WARN("Failed to load session settings file '%1'.", file);
            success = false;
            continue;
        }
        const auto& klass = settings.getValue("MainWindow", "class_name", QString(""));
        MainWidget* widget = nullptr;
        if (klass == MaterialWidget::staticMetaObject.className())
            widget = new MaterialWidget(&mWorkspace);
        else if (klass == ParticleEditorWidget::staticMetaObject.className())
            widget = new ParticleEditorWidget(&mWorkspace);
        else if (klass == AnimationWidget::staticMetaObject.className())
            widget = new AnimationWidget(&mWorkspace);

        // bug, probably forgot to modify the if/else crap above.
        ASSERT(widget);

        if (!widget->loadState(settings))
        {
            WARN("Widget '%1 failed to load state.", widget->windowTitle());
            success = false;
        }
        const bool has_own_window = settings.getValue("MainWindow", "has_own_window", false);
        if (has_own_window)
        {
            ChildWindow* window = showWidget(widget, true);
            const auto xpos  = settings.getValue("MainWindow", "window_xpos", window->x());
            const auto ypos  = settings.getValue("MainWindow", "window_ypos", window->y());
            const auto width = settings.getValue("MainWindow", "window_width", window->width());
            const auto height = settings.getValue("MainWindow", "window_height", window->height());
            if (xpos < size.width() && ypos < size.height())
                window->move(xpos, ypos);

            window->resize(width, height);
        }
        else
        {
            showWidget(widget, false);
        }
        // remove the file, no longer needed.
        QFile::remove(file);
        DEBUG("Loaded widget '%1'", widget->windowTitle());
    }

    mUI.workspace->setModel(mWorkspace.GetResourceModel());
    return success;
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

        QString name = widget->metaObject()->className();
        name.remove("gui::");
        name.remove("Widget");
        mUI.menuTemp->setTitle(name);
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
    INFO("'%1' shaders reloaded.", mCurrentWidget->windowTitle());
}

void MainWindow::on_actionNewMaterial_triggered()
{
    showWidget(new MaterialWidget(&mWorkspace), false);
}

void MainWindow::on_actionNewParticleSystem_triggered()
{
    showWidget(new ParticleEditorWidget(&mWorkspace), false);
}

void MainWindow::on_actionNewAnimation_triggered()
{
    showWidget(new AnimationWidget(&mWorkspace), false);
}

void MainWindow::on_actionEditResource_triggered()
{
    const auto open_new_window = false;

    editResources(open_new_window);
}

void MainWindow::on_actionEditResourceNewWindow_triggered()
{
    const auto open_new_window = true;

    editResources(open_new_window);
}

void MainWindow::on_actionEditResourceNewTab_triggered()
{
    const auto open_new_window = false;

    editResources(open_new_window);
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
    mUI.actionEditResourceNewWindow->setEnabled(!indices.isEmpty());
    mUI.actionEditResourceNewTab->setEnabled(!indices.isEmpty());

    QMenu menu(this);
    menu.addAction(mUI.actionNewMaterial);
    menu.addAction(mUI.actionNewParticleSystem);
    menu.addAction(mUI.actionNewAnimation);
    menu.addSeparator();
    menu.addAction(mUI.actionEditResource);
    menu.addAction(mUI.actionEditResourceNewWindow);
    menu.addAction(mUI.actionEditResourceNewTab);
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
    const auto num_widgets  = mUI.mainTab->count();
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

    // cull child windows that have been closed.
    // note that we do it this way to avoid having problems with callbacks and recursions.
    for (size_t i=0; i<mChildWindows.size(); )
    {
        ChildWindow* child = mChildWindows[i];
        if (child->IsClosed())
        {
            const auto last = mChildWindows.size() - 1;
            std::swap(mChildWindows[i], mChildWindows[last]);
            mChildWindows.pop_back();
            delete child;
        }
        else
        {
            ++i;
        }
    }


    // refresh the child windows
    for (auto* child : mChildWindows)
    {
        child->RefreshUI();
    }

    mWorkspace.Tick();
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
    // delete widget objects in the main tab.
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

    // delete child windows
    for (auto* child : mChildWindows)
    {
        // when the child window is deleted it will do
        // widget shutdown.
        // todo: maybe this should be explicit rather than implicit via dtor ?
        child->close();
        delete child;
    }
    mChildWindows.clear();

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
    bool success = true;

    // session files list, stores the list of temp files
    // generated for each currently open widget
    QStringList session_file_list;

    // for each widget that is currently open in the maintab
    // we generate a temporary json file in which we save the UI state
    // of that widget. When the application is relaunched we use the
    // data in the JSON file to recover the widget and it's contents.
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
        session_file_list << file;
        DEBUG("Saved widget '%1'", widget->windowTitle());
    }

    // for each widget that is contained inside a window (instead of being in the main tab)
    // we (also) generate a temporary JSON file in which we save the widget's UI state.
    // When the application is relaunched we use the data in the JSON to recover
    // the widget and it's contents and also to recreate a new containing ChildWindow
    // with same dimensions and desktop position.
    for (const auto* child : mChildWindows)
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
        const MainWidget* widget = child->GetWidget();

        Settings settings(file);
        settings.setValue("MainWindow", "class_name", widget->metaObject()->className());
        settings.setValue("MainWindow", "has_own_window", true);
        settings.setValue("MainWindow", "window_xpos", child->x());
        settings.setValue("MainWindow", "window_ypos", child->y());
        settings.setValue("MainWindow", "window_width", child->width());
        settings.setValue("MainWindow", "window_height", child->height());
        if (!widget->saveState(settings))
        {
            ERROR("Failed to save widget '%1' settings.", widget->windowTitle());
            success = false;
            continue;
        }
        if (!settings.Save())
        {
            ERROR("Failed to save widget '%1' settings.", widget->windowTitle());
            success = false;
        }
        session_file_list << file;
        DEBUG("Saved widget '%1'", widget->windowTitle());
    }
    // save the list of temp windows for session widgets in
    // the current workspace
    mWorkspace.SetProperty("session_files", session_file_list);


    if (!mWorkspace.SaveContent("content.json"))
        return false;
    if (!mWorkspace.SaveWorkspace("workspace.json"))
        return false;

    // persist the properties of the mainwindow itself.
    mSettings.setValue("MainWindow", "width", width());
    mSettings.setValue("MainWindow", "height", height());
    mSettings.setValue("MainWindow", "xpos", x());
    mSettings.setValue("MainWindow", "ypos", y());
    mSettings.setValue("MainWindow", "show_toolbar", mUI.mainToolBar->isVisible());
    mSettings.setValue("MainWindow", "show_statusbar", mUI.statusbar->isVisible());
    mSettings.setValue("MainWindow", "show_eventlog", mUI.eventlogDock->isVisible());
    mSettings.setValue("MainWindow", "show_workspace", mUI.workspaceDock->isVisible());
    return success;
}

ChildWindow* MainWindow::showWidget(MainWidget* widget, bool new_window)
{
    Q_ASSERT(!widget->parent());

    if (new_window)
    {
        // create a new child window that will hold the widget.
        ChildWindow* child = new ChildWindow(widget);
        child->show();
        mChildWindows.push_back(child);
        return child;
    }

    // show the widget in the main tab of widgets.
    const auto& text = widget->windowTitle();
    const auto& icon = widget->windowIcon();
    const auto count = mUI.mainTab->count();
    mUI.mainTab->addTab(widget, icon, text);
    mUI.mainTab->setCurrentIndex(count);

    // rebuild window menu and shortcuts
    prepareWindowMenu();

    // We need to install this event filter so that we can universally grab
    // Mouse wheel up/down + Ctrl and conver these into zoom in/out actions.
    widget->installEventFilter(this);

    // no child window
    return nullptr;
}

void MainWindow::editResources(bool open_new_window)
{
    const auto& indices = mUI.workspace->selectionModel()->selectedRows();
    for (int i=0; i<indices.size(); ++i)
    {
        const auto& res = mWorkspace.GetResource(indices[i].row());
        switch (res.GetType())
        {
            case app::Resource::Type::Material:
                showWidget(new MaterialWidget(&mWorkspace, res), open_new_window);
                break;
            case app::Resource::Type::ParticleSystem:
                showWidget(new ParticleEditorWidget(&mWorkspace, res), open_new_window);
                break;
            case app::Resource::Type::Animation:
                showWidget(new AnimationWidget(&mWorkspace, res), open_new_window);
                break;
        }
    }
}

} // namespace
