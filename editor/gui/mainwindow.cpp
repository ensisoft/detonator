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
#  include <QStringList>
#  include <QProcess>
#  include <QSurfaceFormat>
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"
#include "graphics/resource.h"
#include "editor/app/format.h"
#include "editor/app/utility.h"
#include "editor/app/eventlog.h"
#include "editor/gui/mainwindow.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/childwindow.h"
#include "editor/gui/playwindow.h"
#include "editor/gui/settings.h"
#include "editor/gui/particlewidget.h"
#include "editor/gui/materialwidget.h"
#include "editor/gui/animationwidget.h"
#include "editor/gui/polygonwidget.h"
#include "editor/gui/dlgsettings.h"
#include "editor/gui/dlgimgpack.h"
#include "editor/gui/dlgpackage.h"
#include "editor/gui/dlgopen.h"
#include "editor/gui/dlgnew.h"
#include "editor/gui/dlgproject.h"
#include "editor/gui/utility.h"
#include "editor/gui/gfxwidget.h"

namespace {
// returns number of seconds elapsed since the last call
// of this function.
double ElapsedSeconds()
{
    using clock = std::chrono::steady_clock;
    static auto start = clock::now();
    auto now  = clock::now();
    auto gone = now - start;
    start = now;
    return std::chrono::duration_cast<std::chrono::microseconds>(gone).count() /
        (1000.0 * 1000.0);
}
} // namespace

namespace gui
{

MainWindow::MainWindow()
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

    // start periodic refresh timer. this is low frequency timer that is used
    // to update the widget UI if needed, such as change the icon/window title
    // and tick the workspace for periodic cleanup and stuff.
    QObject::connect(&mRefreshTimer, &QTimer::timeout, this, &MainWindow::timerRefreshUI);
    mRefreshTimer.setInterval(500);
    mRefreshTimer.start();

    auto& events = app::EventLog::get();
    QObject::connect(&events, SIGNAL(newEvent(const app::Event&)),
        this, SLOT(showNote(const app::Event&)));
    mUI.eventlist->setModel(&events);

    setWindowTitle(QString("%1").arg(APP_TITLE));
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
    // Load master settings.
    Settings settings("Ensisoft", APP_TITLE);

    const auto window_xdim    = settings.getValue("MainWindow", "width",  width());
    const auto window_ydim    = settings.getValue("MainWindow", "height", height());
    const auto window_xpos    = settings.getValue("MainWindow", "xpos", x());
    const auto window_ypos    = settings.getValue("MainWindow", "ypos", y());
    const auto show_statbar   = settings.getValue("MainWindow", "show_statusbar", true);
    const auto show_toolbar   = settings.getValue("MainWindow", "show_toolbar", true);
    const auto show_eventlog  = settings.getValue("MainWindow", "show_event_log", true);
    const auto show_workspace = settings.getValue("MainWindow", "show_workspace", true);
    const auto& dock_state    = settings.getValue("MainWindow", "toolbar_and_dock_state", QByteArray());
#if defined(POSIX_OS)
    mSettings.image_editor_executable = settings.getValue("Settings", "image_editor_executable", QString("/usr/bin/gimp"));
    mSettings.image_editor_arguments  = settings.getValue("Settings", "image_editor_arguments", QString("${file}"));
    mSettings.shader_editor_executable = settings.getValue("Settings", "shader_editor_executable", QString("/usr/bin/gedit"));
    mSettings.shader_editor_arguments  = settings.getValue("Settings", "shader_editor_arguments", QString("${file}"));
#elif defined(WINDOWS_OS)
    mSettings.image_editor_executable = settings.getValue("Settings", "image_editor_executable", QString("mspaint.exe"));
    mSettings.image_editor_arguments  = settings.getValue("Settings", "image_editor_arguments", QString("${file}"));
    mSettings.shader_editor_executable = settings.getValue("Settings", "shader_editor_executable", QString("notepad.exe"));
    mSettings.shader_editor_arguments  = settings.getValue("Settings", "shader_editor_arguments", QString("${file}"));
#endif
    mSettings.default_open_win_or_tab = settings.getValue("Settings", "default_open_win_or_tab", QString("Tab"));

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

    if (!dock_state.isEmpty())
        QMainWindow::restoreState(dock_state);

    mUI.actionSaveWorkspace->setEnabled(false);
    mUI.actionCloseWorkspace->setEnabled(false);
    mUI.menuWorkspace->setEnabled(false);
    mUI.menuEdit->setEnabled(false);
    mUI.menuTemp->setEnabled(false);
    mUI.workspace->setModel(nullptr);

    // check if we have a flag to disable workspace loading.
    // useful for development purposes when you know the workspace
    // might not load properly.
    const QStringList& args = QCoreApplication::arguments();
    for (const QString& arg : args)
    {
        if (arg == "--no-workspace")
            return;
    }

    // load previous workspace if any
    const auto& workspace = settings.getValue("MainWindow", "current_workspace", QString(""));
    if (workspace.isEmpty())
        return;

    if (!loadWorkspace(workspace))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("There was a problem loading the previous workspace."
                                        "\n'%1'\n"
                        "See the application log for more details.").arg(workspace));
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
    mUI.menuWindow->setEnabled(count != 0);
}

void MainWindow::prepareMainTab()
{

}

bool MainWindow::loadWorkspace(const QString& dir)
{
    auto workspace = std::make_unique<app::Workspace>();

    gfx::SetResourceLoader(workspace.get());

    if (!workspace->Load(dir))
    {
        return false;
    }

    const auto& settings = workspace->GetProjectSettings();
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();

    format.setSamples(settings.multisample_sample_count);
    QSurfaceFormat::setDefaultFormat(format);

    GfxWindow::SetDefaultFilter(settings.default_min_filter);
    GfxWindow::SetDefaultFilter(settings.default_mag_filter);

    // desktop dimensions
    const QList<QScreen*>& screens = QGuiApplication::screens();
    const QScreen* screen0 = screens[0];
    const QSize& size = screen0->availableVirtualSize();

    // Load workspace windows and their content.
    bool success = true;

    const auto& session_files = workspace->GetUserProperty("session_files", QStringList());
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
            widget = new MaterialWidget(workspace.get());
        else if (klass == ParticleEditorWidget::staticMetaObject.className())
            widget = new ParticleEditorWidget(workspace.get());
        else if (klass == AnimationWidget::staticMetaObject.className())
            widget = new AnimationWidget(workspace.get());
        else if (klass == PolygonWidget::staticMetaObject.className())
            widget = new PolygonWidget(workspace.get());

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

    setWindowTitle(QString("%1 - %2").arg(APP_TITLE).arg(workspace->GetName()));

    mUI.mainTab->setCurrentIndex(workspace->GetUserProperty("focused_widget_index", 0));
    mUI.workspace->setModel(workspace->GetResourceModel());
    mUI.actionSaveWorkspace->setEnabled(true);
    mUI.actionCloseWorkspace->setEnabled(true);
    mUI.menuWorkspace->setEnabled(true);
    mWorkspace = std::move(workspace);
    return success;
}

bool MainWindow::saveWorkspace()
{
    // if no workspace, the nothing to do.
    if (!mWorkspace)
        return true;

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
    mWorkspace->SetUserProperty("session_files", session_file_list);
    if (mCurrentWidget)
    {
        const auto index_of = mUI.mainTab->indexOf(mCurrentWidget);
        mWorkspace->SetUserProperty("focused_widget_index", index_of);
    }
    if (mPlayWindow)
    {
        mWorkspace->SetUserProperty("play_window_xpos", mPlayWindow->x());
        mWorkspace->SetUserProperty("play_window_ypos", mPlayWindow->y());
    }

    if (!mWorkspace->Save())
        return false;

    return true;
}

void MainWindow::closeWorkspace()
{
    if (!mWorkspace)
    {
        ASSERT(mChildWindows.empty());
        ASSERT(mUI.mainTab->count() == 0);
        ASSERT(!mPlayWindow.get());
        return;
    }

    // note that here we don't care about saving any state.
    // this is only for closing everything, closing the tabs
    // and the child windows if any are open.

    // make sure we're not getting nasty unwanted recursion
    QSignalBlocker cockblocker(mUI.mainTab);

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

    mCurrentWidget = nullptr;

    if (mPlayWindow)
    {
        mPlayWindow->close();
        mPlayWindow.reset();
    }

    // update window menu.
    prepareWindowMenu();

    mUI.actionSaveWorkspace->setEnabled(false);
    mUI.actionCloseWorkspace->setEnabled(false);
    mUI.menuWorkspace->setEnabled(false);
    mUI.menuEdit->setEnabled(false);
    mUI.menuTemp->setEnabled(false);
    mUI.workspace->setModel(nullptr);

    setWindowTitle(QString("%1").arg(APP_TITLE));

    mWorkspace.reset();

    gfx::SetResourceLoader(nullptr);
}

void MainWindow::showWindow()
{
    show();
}

bool MainWindow::iterateGameLoop()
{
    const auto elapsed_since = ElapsedSeconds();
    const auto time_step = 1.0/60.0;

    mTimeAccum += elapsed_since;

    while (mTimeAccum >= time_step)
    {
        for (int i=0; i<GetCount(mUI.mainTab); ++i)
        {
            auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
            widget->animate(time_step);
        }
        for (auto* child : mChildWindows)
        {
            child->Animate(time_step);
        }

        if (mPlayWindow)
        {
            mPlayWindow->Update(time_step);
        }
        mTimeTotal += time_step;
        mTimeAccum -= time_step;
    }

    // render all widgets
    for (int i=0; i<GetCount(mUI.mainTab); ++i)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        widget->render();
    }
    for (auto* child : mChildWindows)
    {
        child->Render();
    }

    if (mPlayWindow)
    {
        mPlayWindow->Render();
    }

    return mUI.mainTab->count() || mChildWindows.size() || mPlayWindow;
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
        mUI.menuEdit->setEnabled(true);
        //mUI.menuTemp->setVisible(true);
        mUI.menuTemp->setEnabled(true);
        mUI.menuTemp->setTitle(name);
        mCurrentWidget = widget;
    }
    else
    {
        //mUI.menuTemp->setVisible(false);
        mUI.menuTemp->setEnabled(false);
        mUI.menuEdit->setEnabled(false);
    }

    // add the stuff that is always in the edit menu
    mUI.mainToolBar->addSeparator();

    prepareWindowMenu();

}

void MainWindow::on_mainTab_tabCloseRequested(int index)
{
    auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(index));
    if (!widget->confirmClose())
        return;

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

void MainWindow::on_actionReloadTextures_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->reloadTextures();
    INFO("'%1' textures reloaded.", mCurrentWidget->windowTitle());
}

void MainWindow::on_actionNewMaterial_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    showWidget(new MaterialWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionNewParticleSystem_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    showWidget(new ParticleEditorWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionNewAnimation_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    showWidget(new AnimationWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionNewCustomShape_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    showWidget(new PolygonWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionEditResource_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
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
    mWorkspace->DeleteResources(selected);
}

void MainWindow::on_actionSaveWorkspace_triggered()
{
    if (!mWorkspace->Save())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Workspace saving failed. See the log for more information."));
        msg.exec();
        return;
    }
    NOTE("Workspace saved.");
}

void MainWindow::on_actionLoadWorkspace_triggered()
{
    const auto& dir = QFileDialog::getExistingDirectory(this,
        tr("Select Workspace Directory"));
    if (dir.isEmpty())
        return;

    // check here whether the files actually exist.
    // todo: maybe move into workspace to validate folder
    if (MissingFile(app::JoinPath(dir, "content.json")) ||
        MissingFile(app::JoinPath(dir, "workspace.json")))
    {
        // todo: could ask if the user would like to create a new workspace instead.
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr(            "The folder"
                                 "\n\n'%1'\n\n"
                       "doesn't seem contain workspace files.\n").arg(dir));
        msg.exec();
        return;
    }

    // todo: should/could ask about saving. the current workspace if we have any.

    if (!saveWorkspace())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("There was a problem saving the current workspace.\n"
            "Do you still want to continue ?");
        if (msg.exec() == QMessageBox::No)
            return;
    }
    // close existing workspace if any.
    closeWorkspace();

    // load new workspace.
    if (!loadWorkspace(dir))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to open workspace\n"
                    "\n\n'%1'\n\n"
                    "See the application log for more information.").arg(dir));
        msg.exec();
        return;
    }

    setWindowTitle(QString("%1 - %2").arg(APP_TITLE).arg(mWorkspace->GetName()));

    NOTE("Loaded workspace.");
}

void MainWindow::on_actionNewWorkspace_triggered()
{
    // Note: it might be tempting in terms of UX to just let the
    // user create a new workspace object and start working adding
    // content, however this has the problem that since we don't know
    // where the workspace would end up being saved we don't know how
    // to map content paths (relative to the workspace without location).
    // (Also it could be that at some point some of the content is
    // copied to some workspace folders.)
    // Therefore we need this clunkier UX where the user must first be
    // prompted for the location of the workspace before it can be
    // used to create content.

    // todo: might want to improve the dialog here to be a custom dialog
    // with an option to create some directory for the new workspace
    const auto& dir = QFileDialog::getExistingDirectory(this,
        tr("Select New Workspace Directory"));
    if (dir.isEmpty())
        return;

    // todo: should/could ask about saving. the current workspace if we have any.
    if (!saveWorkspace())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("There was a problem saving the current workspace.\n"
            "Do you still want to continue ?");
        if (msg.exec() == QMessageBox::No)
            return;
    }
    // close existing workspace if any.
    closeWorkspace();

    if (!MissingFile(app::JoinPath(dir, "content.json")) ||
        !MissingFile(app::JoinPath(dir, "workspace.json")))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("The selected folder seems to already contain a workspace.\n"
            "Are you sure you want to overwrite this ?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    auto workspace = std::make_unique<app::Workspace>();
    if (!workspace->OpenNew(dir))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("There was a problem creating the new workspace.\n"
            "Please see the log for details."));
        msg.exec();
        return;
    }

    mUI.workspace->setModel(workspace.get());
    mUI.actionSaveWorkspace->setEnabled(true);
    mUI.actionCloseWorkspace->setEnabled(true);
    mUI.menuWorkspace->setEnabled(true);
    setWindowTitle(QString("%1 - %2").arg(APP_TITLE).arg(workspace->GetName()));
    mWorkspace = std::move(workspace);
    gfx::SetResourceLoader(mWorkspace.get());
    NOTE("New workspace created.");
}

void MainWindow::on_actionCloseWorkspace_triggered()
{
    // todo: should/could ask about saving. the current workspace if we have any.
    if (!saveWorkspace())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("There was a problem saving the current workspace.\n"
            "Do you still want to continue ?");
        if (msg.exec() == QMessageBox::No)
            return;
    }
    // close existing workspace if any.
    closeWorkspace();
}

void MainWindow::on_actionSettings_triggered()
{
    DlgSettings dlg(this, mSettings);
    dlg.exec();
}

void MainWindow::on_actionImagePacker_triggered()
{
    DlgImgPack dlg(this);
    dlg.exec();
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
    menu.addAction(mUI.actionNewCustomShape);
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

void MainWindow::on_actionPackageResources_triggered()
{
    DlgPackage dlg(QApplication::activeWindow(), *mWorkspace);
    dlg.exec();
}

void MainWindow::on_actionSelectResourceForEditing_triggered()
{
    DlgOpen dlg(QApplication::activeWindow(), *mWorkspace);
    if (dlg.exec() == QDialog::Accepted)
    {
        app::Resource* res = dlg.GetSelected();
        if (res == nullptr)
            return;
        const auto new_window = mSettings.default_open_win_or_tab == "Window";
        switch (res->GetType())
        {
            case app::Resource::Type::Material:
                showWidget(new MaterialWidget(mWorkspace.get(), *res), new_window);
                break;
            case app::Resource::Type::ParticleSystem:
                showWidget(new ParticleEditorWidget(mWorkspace.get(), *res), new_window);
                break;
            case app::Resource::Type::Animation:
                showWidget(new AnimationWidget(mWorkspace.get(), *res), new_window);
                break;
            case app::Resource::Type::CustomShape:
                showWidget(new PolygonWidget(mWorkspace.get(), *res), new_window);
                break;
        }
    }
}

void MainWindow::on_actionNewResource_triggered()
{
    DlgNew dlg(QApplication::activeWindow());
    if (dlg.exec() == QDialog::Accepted)
    {
        const auto new_window = mSettings.default_open_win_or_tab == "Window";
        switch (dlg.GetType())
        {
            case app::Resource::Type::Material:
                showWidget(new MaterialWidget(mWorkspace.get()), new_window);
                break;
            case app::Resource::Type::ParticleSystem:
                showWidget(new ParticleEditorWidget(mWorkspace.get()), new_window);
                break;
            case app::Resource::Type::Animation:
                showWidget(new AnimationWidget(mWorkspace.get()), new_window);
                break;
            case app::Resource::Type::CustomShape:
                showWidget(new PolygonWidget(mWorkspace.get()), new_window);
                break;
        }
    }
}

void MainWindow::on_actionProjectSettings_triggered()
{
    auto settings = mWorkspace->GetProjectSettings();

    DlgProject dlg(QApplication::activeWindow(), settings);
    if (dlg.exec() == QDialog::Rejected)
        return;

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(settings.multisample_sample_count);
    QSurfaceFormat::setDefaultFormat(format);

    mWorkspace->SetProjectSettings(settings);

    GfxWindow::SetDefaultFilter(settings.default_min_filter);
    GfxWindow::SetDefaultFilter(settings.default_mag_filter);
}

void MainWindow::on_actionProjectPlay_triggered()
{
    if (!mWorkspace)
        return;

    if (!mPlayWindow)
    {
        mPlayWindow.reset(new PlayWindow(*mWorkspace));
        const auto xpos = mWorkspace->GetUserProperty("play_window_xpos", mPlayWindow->x());
        const auto ypos = mWorkspace->GetUserProperty("play_window_ypos", mPlayWindow->y());
        mPlayWindow->move(xpos, ypos);
        mPlayWindow->show();
        emit newAcceleratedWindowOpen();
        DEBUG("Playwindow position: %1x%2", xpos, ypos);
    }
    // bring to the top of the window stack.
    mPlayWindow->raise();
}

void MainWindow::timerRefreshUI()
{
    if (mPlayWindow)
    {
        if (mPlayWindow->IsClosed())
        {
            mWorkspace->SetUserProperty("play_window_xpos", mPlayWindow->x());
            mWorkspace->SetUserProperty("play_window_ypos", mPlayWindow->y());
            mPlayWindow.reset();
        }
    }

    // refresh the UI state, and update the tab widget icon/text
    // if needed.
    for (int i=0; i<GetCount(mUI.mainTab); ++i)
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

    if (mWorkspace)
        mWorkspace->Tick();
}

void MainWindow::showNote(const app::Event& event)
{
    if (event.type == app::Event::Type::Note)
    {
        mUI.statusbar->showMessage(event.message, 5000);
    }
}

void MainWindow::openExternalImage(const QString& file)
{
    if (mSettings.image_editor_executable.isEmpty())
    {
        QMessageBox msg;
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't configured any external application for opening images.\n"
                    "Would you like to set one now?");
        if (msg.exec() == QMessageBox::No)
            return;
        on_actionSettings_triggered();
        if (mSettings.image_editor_executable.isEmpty())
        {
            ERROR("No image editor has been configured.");
            return;
        }
    }
    if (MissingFile(file))
    {
        WARN("Could not find '%1'", file);
    }

    QStringList args;
    QStringList list = mSettings.image_editor_arguments.split(" ", QString::SkipEmptyParts);
    for (const auto& item : list)
    {
        if (item == "${file}")
            args << QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
        else args << item;
    }
    if (!QProcess::startDetached(mSettings.image_editor_executable, args))
    {
        ERROR("Failed to start application '%1'", mSettings.image_editor_executable);

    }
    DEBUG("Start application '%1'", mSettings.image_editor_executable);
}

void MainWindow::openExternalShader(const QString& file)
{
    if (mSettings.shader_editor_executable.isEmpty())
    {
        QMessageBox msg;
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't configured any external application for shader files.\n"
                    "Would you like to set one now?");
        if (msg.exec() == QMessageBox::No)
            return;
        on_actionSettings_triggered();
        if (mSettings.shader_editor_executable.isEmpty())
        {
            ERROR("No shader editor has been configured.");
            return;
        }
    }
    if (MissingFile(file))
    {
        WARN("Could not find '%1'", file);
    }

    QStringList args;
    QStringList list = mSettings.shader_editor_arguments.split(" ", QString::SkipEmptyParts);
    for (const auto& item : list)
    {
        if (item == "${file}")
            args << QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
        else args << item;
    }
    if (!QProcess::startDetached(mSettings.shader_editor_executable, args))
    {
        ERROR("Failed to start application '%1'", mSettings.shader_editor_executable);

    }
    DEBUG("Start application '%1'", mSettings.shader_editor_executable);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();

    // try to perform an orderly shutdown.
    // first save everything and only if that is succesful
    // (or the user don't care) we then close the workspace
    // and exit the application.
    if (!saveWorkspace() || !saveState())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("There was a problem saving the application state.\r\n"
            "Do you still want to quit the application?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    // close workspace (if any is open)
    closeWorkspace();

    // accept the event, will quit the application
    event->accept();

    mIsClosed = true;

    emit aboutToClose();
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
    // persist the properties of the mainwindow itself.
    Settings settings("Ensisoft", APP_TITLE);
    settings.setValue("MainWindow", "width", width());
    settings.setValue("MainWindow", "height", height());
    settings.setValue("MainWindow", "xpos", x());
    settings.setValue("MainWindow", "ypos", y());
    settings.setValue("MainWindow", "show_toolbar", mUI.mainToolBar->isVisible());
    settings.setValue("MainWindow", "show_statusbar", mUI.statusbar->isVisible());
    settings.setValue("MainWindow", "show_eventlog", mUI.eventlogDock->isVisible());
    settings.setValue("MainWindow", "show_workspace", mUI.workspaceDock->isVisible());
    settings.setValue("Settings", "image_editor_executable", mSettings.image_editor_executable);
    settings.setValue("Settings", "image_editor_arguments", mSettings.image_editor_arguments);
    settings.setValue("Settings", "shader_editor_executable", mSettings.shader_editor_executable);
    settings.setValue("Settings", "shader_editor_arguments", mSettings.shader_editor_arguments);
    settings.setValue("Settings", "default_open_win_or_tab", mSettings.default_open_win_or_tab);
    settings.setValue("MainWindow", "current_workspace",
        (mWorkspace ? mWorkspace->GetDir() : ""));

    // QMainWindow::saveState saves the current state of the mainwindow toolbars
    // and dockwidgets.
    settings.setValue("MainWindow", "toolbar_and_dock_state", QMainWindow::saveState());
    return settings.Save();
}

ChildWindow* MainWindow::showWidget(MainWidget* widget, bool new_window)
{
    ASSERT(widget->parent() == nullptr);

    // connect the important signals here.
    connect(widget, &MainWidget::openExternalImage,
            this,   &MainWindow::openExternalImage);
    connect(widget, &MainWidget::openExternalShader,
            this,   &MainWindow::openExternalShader);

    //widget->setTargetFps(mSettings.target_fps);

    if (new_window)
    {
        // create a new child window that will hold the widget.
        ChildWindow* child = new ChildWindow(widget);
        child->SetSharedWorkspaceMenu(mUI.menuWorkspace);
        child->show();
        mChildWindows.push_back(child);

        emit newAcceleratedWindowOpen();
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

    emit newAcceleratedWindowOpen();

    // no child window
    return nullptr;
}

void MainWindow::editResources(bool open_new_window)
{
    const auto& indices = mUI.workspace->selectionModel()->selectedRows();
    for (int i=0; i<indices.size(); ++i)
    {
        const auto& res = mWorkspace->GetResource(indices[i].row());
        switch (res.GetType())
        {
            case app::Resource::Type::Material:
                showWidget(new MaterialWidget(mWorkspace.get(), res), open_new_window);
                break;
            case app::Resource::Type::ParticleSystem:
                showWidget(new ParticleEditorWidget(mWorkspace.get(), res), open_new_window);
                break;
            case app::Resource::Type::Animation:
                showWidget(new AnimationWidget(mWorkspace.get(), res), open_new_window);
                break;
            case app::Resource::Type::CustomShape:
                showWidget(new PolygonWidget(mWorkspace.get(), res), open_new_window);
                break;
        }
    }
}

} // namespace
