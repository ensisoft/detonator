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
#  include <QAbstractEventDispatcher>
#include "warnpop.h"

#if defined(__GCC__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <algorithm>

#include "base/assert.h"
#include "data/json.h"
#include "graphics/resource.h"
#include "graphics/loader.h"
#include "editor/app/buffer.h"
#include "editor/app/format.h"
#include "editor/app/utility.h"
#include "editor/app/eventlog.h"
#include "editor/gui/mainwindow.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/childwindow.h"
#include "editor/gui/playwindow.h"
#include "editor/gui/settings.h"
#include "editor/gui/audiowidget.h"
#include "editor/gui/particlewidget.h"
#include "editor/gui/materialwidget.h"
#include "editor/gui/entitywidget.h"
#include "editor/gui/scenewidget.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/polygonwidget.h"
#include "editor/gui/tilemapwidget.h"
#include "editor/gui/dlgabout.h"
#include "editor/gui/dlgresdeps.h"
#include "editor/gui/dlgimport.h"
#include "editor/gui/dlgtileimport.h"
#include "editor/gui/dlgsettings.h"
#include "editor/gui/dlgimgpack.h"
#include "editor/gui/dlgpackage.h"
#include "editor/gui/dlgopen.h"
#include "editor/gui/dlgnew.h"
#include "editor/gui/dlgproject.h"
#include "editor/gui/dlgsave.h"
#include "editor/gui/dlgmigrationlog.h"
#include "editor/gui/dlgimgview.h"
#include "editor/gui/dlgfontmap.h"
#include "editor/gui/dlgtilemap.h"
#include "editor/gui/dlgvcs.h"
#include "editor/gui/dlgsvg.h"
#include "editor/gui/utility.h"
#include "editor/gui/gfxwidget.h"
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/codewidget.h"
#include "editor/gui/uiwidget.h"
#include "editor/gui/drawing.h"
#include "editor/gui/types.h"

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

gui::MainWidget* CreateWidget(app::Resource::Type type, app::Workspace* workspace, const app::Resource* resource = nullptr)
{
    switch (type) {
        case app::Resource::Type::Material:
            if (resource)
                return new gui::MaterialWidget(workspace, *resource);
            return new gui::MaterialWidget(workspace);
        case app::Resource::Type::ParticleSystem:
            if (resource)
                return new gui::ParticleEditorWidget(workspace, *resource);
            return new gui::ParticleEditorWidget(workspace);
        case app::Resource::Type::Shape:
            if (resource)
                return new gui::ShapeWidget(workspace, *resource);
            return new gui::ShapeWidget(workspace);
        case app::Resource::Type::Entity:
            if (resource)
                return new gui::EntityWidget(workspace, *resource);
            return new gui::EntityWidget(workspace);
        case app::Resource::Type::Scene:
            if (resource)
                return new gui::SceneWidget(workspace, *resource);
            return new gui::SceneWidget(workspace);
        case app::Resource::Type::Tilemap:
            if (resource)
                return new gui::TilemapWidget(workspace, *resource);
            return new gui::TilemapWidget(workspace);
        case app::Resource::Type::Script:
            if (resource)
                return new gui::ScriptWidget(workspace, *resource);
            return new gui::ScriptWidget(workspace);
        case app::Resource::Type::UI:
            if (resource)
                return new gui::UIWidget(workspace, *resource);
            return new gui::UIWidget(workspace);
        case app::Resource::Type::AudioGraph:
            if (resource)
                return new gui::AudioWidget(workspace, *resource);
            return new gui::AudioWidget(workspace);
    }
    BUG("Unhandled widget type.");
    return nullptr;
}

struct ExternalApplicationArgs {
    QString executable_binary;
    QString executable_args;
    QString file;
};

void LaunchExternalApplication(const ExternalApplicationArgs& args)
{
    if (gui::MissingFile(args.file))
        WARN("Could not find '%1'", args.file);

    QStringList arg_list;
    QStringList tokens = args.executable_args.split(" ", Qt::SkipEmptyParts);
    for (const auto& item : tokens)
    {
        if (item == "${file}")
            arg_list << args.file;
        else arg_list << item;
    }
    if (!QProcess::startDetached(args.executable_binary, arg_list))
    {
        ERROR("Failed to start application '%1'", args.executable_binary);
        return;
    }
    DEBUG("Start application '%1'", args.executable_binary);
}

} // namespace

namespace gui
{

class MainWindow::GfxResourceLoader : public gfx::Loader {
public:
    virtual gfx::ResourceHandle LoadResource(const std::string& URI) override
    {
        if (base::StartsWith(URI, "app://")) {
            return app::Workspace::LoadAppResource(URI);
        }
        return app::GraphicsBuffer::LoadFromFile(app::FromUtf8(URI));
    }
};

MainWindow::MainWindow(QApplication& app) : mApplication(app)
{
    mUI.setupUi(this);
    mUI.actionExit->setShortcut(QKeySequence::Quit);
    //mUI.actionWindowClose->setShortcut(QKeySequence::Close); // using ours now
    mUI.actionWindowNext->setShortcut(QKeySequence::Forward);
    mUI.actionWindowPrev->setShortcut(QKeySequence::Back);
    mUI.statusbar->insertPermanentWidget(0, mUI.statusBarFrame);
    mUI.statusBarFrame->setVisible(false);
    mUI.statusbar->setVisible(true);
    mUI.mainToolBar->setVisible(true);
    mUI.actionViewToolbar->setChecked(true);
    mUI.actionViewStatusbar->setChecked(true);
    mUI.actionCut->setShortcut(QKeySequence::Cut);
    mUI.actionCopy->setShortcut(QKeySequence::Copy);
    mUI.actionPaste->setShortcut(QKeySequence::Paste);
    mUI.actionUndo->setShortcut(QKeySequence::Undo);

    ShowHelpWidget();

    // start periodic refresh timer. this is low frequency timer that is used
    // to update the widget UI if needed, such as change the icon/window title
    // and tick the workspace for periodic cleanup and stuff.
    QObject::connect(&mRefreshTimer, &QTimer::timeout, this, &MainWindow::RefreshUI);
    mRefreshTimer.setInterval(500);
    mRefreshTimer.start();

    auto& events = app::EventLog::get();
    QObject::connect(&events, SIGNAL(newEvent(const app::Event&)),this, SLOT(ShowNote(const app::Event&)));
    mEventLog.SetModel(&events);
    mEventLog.setSourceModel(&events);
    mUI.eventlist->setModel(&mEventLog);

    SetValue(mUI.grpHelp, QString("Welcome to %1").arg(APP_TITLE));
    setWindowTitle(APP_TITLE);
    setAcceptDrops(true);

    // need this loader for the tool dialogs that use GFX based rendering
    // and use resources under application, i.e. with app:// resource URI
    // When a workspace is opened the resource loader is then replaced with
    // the "real" thing, i.e the workspace object.
    mLoader = std::make_unique<GfxResourceLoader>();
    gfx::SetResourceLoader(mLoader.get());
}

MainWindow::~MainWindow()
{}

void MainWindow::LoadSettings()
{
#if defined(POSIX_OS)
    mSettings.image_editor_executable  = "/usr/bin/gimp";
    mSettings.shader_editor_executable = "/usr/bin/gedit";
    mSettings.script_editor_executable = "/usr/bin/gedit";
    mSettings.audio_editor_executable  = "/usr/bin/audacity";
    mSettings.python_executable        = "/usr/bin/python";
    mSettings.vcs_executable           = "/usr/bin/git";
    // no emsdk selected, user has to do that :(
#elif defined(WINDOWS_OS)
    mSettings.image_editor_executable  = "mspaint.exe";
    mSettings.shader_editor_executable = "notepad.exe";
    mSettings.script_editor_executable = "notepad.exe";
    // todo: what python to use ?
    // use python from emssdk ? use python packaged with gamestudio ?
    // emsdk must be selected in any case.
    mSettings.python_executable = app::GetAppInstFilePath("python/python.exe");
    mSettings.vcs_executable           = "C:\\Program Files\\Git\\cmd\\git.exe";

#endif
    mSettings.vcs_cmd_commit_file = "add -f ${file}";
    mSettings.vcs_cmd_add_file    = "add -f ${file}";
    mSettings.vcs_cmd_del_file    = "rm -f --cached ${file}";
    mSettings.vcs_cmd_list_files  = "ls-files ${workspace}";
    mSettings.vcs_ignore_list     = "content.json,workspace.json,readme,license,screenshot.png";

    Settings settings("Ensisoft", "Gamestudio Editor");
    if (!settings.Load())
    {
        ERROR("Failed to load application settings.");
        return;
    }
    settings.GetValue("Settings", "image_editor_executable",    &mSettings.image_editor_executable);
    settings.GetValue("Settings", "image_editor_arguments",     &mSettings.image_editor_arguments);
    settings.GetValue("Settings", "shader_editor_executable",   &mSettings.shader_editor_executable);
    settings.GetValue("Settings", "shader_editor_arguments",    &mSettings.shader_editor_arguments);
    settings.GetValue("Settings", "script_editor_executable",   &mSettings.script_editor_executable);
    settings.GetValue("Settings", "script_editor_arguments",    &mSettings.script_editor_arguments);
    settings.GetValue("Settings", "audio_editor_executable",    &mSettings.audio_editor_executable);
    settings.GetValue("Settings", "audio_editor_arguments",     &mSettings.audio_editor_arguments);
    settings.GetValue("Settings", "default_open_win_or_tab",    &mSettings.default_open_win_or_tab);
    settings.GetValue("Settings", "style_name",                 &mSettings.style_name);
    settings.GetValue("Settings", "save_automatically_on_play", &mSettings.save_automatically_on_play);
    settings.GetValue("Settings", "python_executable",          &mSettings.python_executable);
    settings.GetValue("Settings", "emsdk",                      &mSettings.emsdk);
    settings.GetValue("Settings", "clear_color",                &mSettings.clear_color);
    settings.GetValue("Settings", "grid_color",                 &mSettings.grid_color);
    settings.GetValue("Settings", "default_grid",               &mUISettings.grid);
    settings.GetValue("Settings", "default_zoom",               &mUISettings.zoom);
    settings.GetValue("Settings", "snap_to_grid",               &mUISettings.snap_to_grid);
    settings.GetValue("Settings", "show_viewport",              &mUISettings.show_viewport);
    settings.GetValue("Settings", "show_origin",                &mUISettings.show_origin);
    settings.GetValue("Settings", "show_grid",                  &mUISettings.show_grid);
    settings.GetValue("Settings", "vsync",                      &mSettings.vsync);
    settings.GetValue("Settings", "frame_delay",                &mSettings.frame_delay);
    settings.GetValue("Settings", "mouse_cursor",               &mSettings.mouse_cursor);
    settings.GetValue("Settings", "viewer_geometry",            &mSettings.viewer_geometry);
    settings.GetValue("Settings", "vcs_executable",             &mSettings.vcs_executable);
    settings.GetValue("Settings", "vcs_cmd_list_files",         &mSettings.vcs_cmd_list_files);
    settings.GetValue("Settings", "vcs_cmd_add_file",           &mSettings.vcs_cmd_add_file);
    settings.GetValue("Settings", "vcs_cmd_del_file",           &mSettings.vcs_cmd_del_file);
    settings.GetValue("Settings", "vcs_cmd_commit_file",        &mSettings.vcs_cmd_commit_file);
    settings.GetValue("Settings", "vcs_ignore_list",            &mSettings.vcs_ignore_list);
    settings.GetValue("Settings", "main_tab_position",          &mSettings.main_tab_position);
    GfxWindow::SetDefaultClearColor(ToGfx(mSettings.clear_color));
    // disabling the VSYNC setting for now since there are just too many problems
    // making it scale nicely when having multiple windows.
    GfxWindow::SetVSYNC(false); // mSettings.vsync
    GfxWindow::SetMouseCursor(mSettings.mouse_cursor);
    gui::SetGridColor(ToGfx(mSettings.grid_color));

    ScriptWidget::Settings script_widget_settings;
    settings.GetValue("ScriptWidget", "color_theme",                    &script_widget_settings.theme);
    settings.GetValue("ScriptWidget", "lua_formatter_exec",             &script_widget_settings.lua_formatter_exec);
    settings.GetValue("ScriptWidget", "lua_formatter_args",             &script_widget_settings.lua_formatter_args);
    settings.GetValue("ScriptWidget", "lua_format_on_save",             &script_widget_settings.lua_format_on_save);
    settings.GetValue("ScriptWidget", "editor_font_name",               &script_widget_settings.editor_settings.font_description);
    settings.GetValue("ScriptWidget", "editor_font_size",               &script_widget_settings.editor_settings.font_size);
    settings.GetValue("ScriptWidget", "editor_show_line_numbers",       &script_widget_settings.editor_settings.show_line_numbers);
    settings.GetValue("ScriptWidget", "editor_highlight_syntax",        &script_widget_settings.editor_settings.highlight_syntax);
    settings.GetValue("ScriptWidget", "editor_highlight_current_line",  &script_widget_settings.editor_settings.highlight_current_line);
    settings.GetValue("ScriptWidget", "editor_replace_tab_with_spaces", &script_widget_settings.editor_settings.replace_tabs_with_spaces);
    settings.GetValue("ScriptWidget", "editor_num_tab_spaces",          &script_widget_settings.editor_settings.tab_spaces);
    ScriptWidget::SetDefaultSettings(script_widget_settings);

    mUI.mainTab->setTabPosition(mSettings.main_tab_position);

    if (!mSettings.style_name.isEmpty())
    {
        app::SetStyle(mSettings.style_name);
    }
    DEBUG("Loaded application settings.");
}


void MainWindow::LoadLastState()
{
    const auto& file = app::GetAppHomeFilePath("state.json");
    Settings settings(file);
    if (!settings.Load())
    {
        ERROR("Failed to load application state.");
        return;
    }
    const auto log_bits       = settings.GetValue("MainWindow", "log_bits", mEventLog.GetShowBits());
    const auto window_xdim    = settings.GetValue("MainWindow", "width", width());
    const auto window_ydim    = settings.GetValue("MainWindow", "height", height());
    const auto window_xpos    = settings.GetValue("MainWindow", "xpos", x());
    const auto window_ypos    = settings.GetValue("MainWindow", "ypos", y());
    const auto show_statbar   = settings.GetValue("MainWindow", "show_statusbar", true);
    const auto show_toolbar   = settings.GetValue("MainWindow", "show_toolbar", true);
    const auto show_eventlog  = settings.GetValue("MainWindow", "show_event_log", true);
    const auto show_workspace = settings.GetValue("MainWindow", "show_workspace", true);
    const auto& dock_state    = settings.GetValue("MainWindow", "toolbar_and_dock_state", QMainWindow::saveState());
    settings.GetValue("MainWindow", "recent_workspaces", &mRecentWorkspaces);

    mEventLog.SetShowBits(log_bits);
    mEventLog.invalidate();
    mUI.actionLogShowInfo->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Info));
    mUI.actionLogShowWarning->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Warning));
    mUI.actionLogShowError->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Error));

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
    mUI.actionSelectResourceForEditing->setEnabled(false);
    mUI.menuWorkspace->setEnabled(false);
    mUI.menuEdit->setEnabled(false);
    mUI.menuTemp->setEnabled(false);
    mUI.workspace->setModel(nullptr);
    mWorkspaceProxy.SetModel(nullptr);
    mWorkspaceProxy.setSourceModel(nullptr);
    BuildRecentWorkspacesMenu();
}

void MainWindow::LoadLastWorkspace()
{
    const auto& file = app::GetAppHomeFilePath("state.json");
    Settings settings(file);
    if (!settings.Load())
    {
        ERROR("Failed to load application state.");
        return;
    }
    const auto& workspace = settings.GetValue("MainWindow", "current_workspace", QString(""));
    if (workspace.isEmpty())
        return;

    if (!LoadWorkspace(workspace))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("There was a problem loading the previous workspace.\n\n%1"
                       "See the application log for more details.").arg(workspace));
        msg.exec();
    }
}

void MainWindow::UpdateWindowMenu()
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

        QObject::connect(action, SIGNAL(triggered()),this, SLOT(actionWindowFocus_triggered()));
    }
    // and this is in the window menu
    mUI.menuWindow->addSeparator();
    mUI.menuWindow->addAction(mUI.actionWindowPopOut);
    mUI.menuWindow->addAction(mUI.actionWindowClose);
    mUI.menuWindow->addAction(mUI.actionWindowNext);
    mUI.menuWindow->addAction(mUI.actionWindowPrev);
    mUI.menuWindow->setEnabled(count != 0);
}

void MainWindow::LoadDemoWorkspace(const QString& which)
{
    QString where = qApp->applicationDirPath();

    LoadWorkspace(app::JoinPath(where, which));
}

bool MainWindow::LoadWorkspace(const QString& dir)
{
    ASSERT(!mWorkspace);

    app::MigrationLog migration_log;

    auto workspace = std::make_unique<app::Workspace>(dir);
    if (!workspace->LoadWorkspace(&migration_log))
        return false;

    mWorkspace = std::move(workspace);
    connect(mWorkspace.get(), &app::Workspace::ResourceUpdated, this, &MainWindow::ResourceUpdated);
    connect(mWorkspace.get(), &app::Workspace::ResourceAdded,   this, &MainWindow::ResourceAdded);

    gfx::SetResourceLoader(mWorkspace.get());

    const auto& settings = mWorkspace->GetProjectSettings();
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();

    format.setSamples(settings.multisample_sample_count);
    format.setColorSpace(settings.config_srgb
        ? QSurfaceFormat::ColorSpace::sRGBColorSpace
        : QSurfaceFormat::ColorSpace::DefaultColorSpace);
    QSurfaceFormat::setDefaultFormat(format);

    GfxWindow::SetDefaultFilter(settings.default_min_filter);
    GfxWindow::SetDefaultFilter(settings.default_mag_filter);

    // desktop dimensions
    const QList<QScreen*>& screens = QGuiApplication::screens();
    const QScreen* screen0 = screens[0];
    const QSize& size = screen0->availableVirtualSize();

    // block main tab signals
    QSignalBlocker blocker(mUI.mainTab);

    // Load workspace windows and their content.
    bool success = true;
    bool load_session = true;

    const QStringList& args = QCoreApplication::arguments();
    for (const QString& arg : args)
    {
        if (arg == "--no-session")
            load_session = false;
    }

    unsigned show_resource_bits = ~0u;
    QStringList session;
    QString filter_string;
    mWorkspace->GetUserProperty("session_files", &session);
    mWorkspace->GetUserProperty("resource_show_bits", &show_resource_bits);
    mWorkspace->GetUserProperty("resource_filter_string", &filter_string);

    if (load_session)
    {
        for (const auto& file: session)
        {
            Settings settings(file);
            if (!settings.Load())
            {
                WARN("Failed to load session file. [file='%1']", file);
                success = false;
                continue;
            }
            const auto& klass = settings.GetValue("MainWindow", "class_name", QString(""));
            const auto& id = settings.GetValue("MainWindow", "widget_id", QString(""));
            const auto& title = settings.GetValue("MainWindow", "widget_title", QString(""));
            MainWidget* widget = nullptr;
            if (klass == MaterialWidget::staticMetaObject.className())
                widget = new MaterialWidget(mWorkspace.get());
            else if (klass == ParticleEditorWidget::staticMetaObject.className())
                widget = new ParticleEditorWidget(mWorkspace.get());
            else if (klass == ShapeWidget::staticMetaObject.className())
                widget = new ShapeWidget(mWorkspace.get());
            else if (klass == AnimationTrackWidget::staticMetaObject.className())
                widget = new AnimationTrackWidget(mWorkspace.get());
            else if (klass == EntityWidget::staticMetaObject.className())
                widget = new EntityWidget(mWorkspace.get());
            else if (klass == SceneWidget::staticMetaObject.className())
                widget = new SceneWidget(mWorkspace.get());
            else if (klass == TilemapWidget::staticMetaObject.className())
                widget = new TilemapWidget(mWorkspace.get());
            else if (klass == ScriptWidget::staticMetaObject.className())
                widget = new ScriptWidget(mWorkspace.get());
            else if (klass == UIWidget::staticMetaObject.className())
                widget = new UIWidget(mWorkspace.get());
            else if (klass == AudioWidget::staticMetaObject.className())
                widget = new AudioWidget(mWorkspace.get());
            else BUG("Unhandled widget type.");

            widget->setWindowTitle(title);

            if (!widget->LoadState(settings))
            {
                WARN("Failed to load main widget state. [name='%1']", title);
                success = false;
            }
            const bool has_own_window = settings.GetValue("MainWindow", "has_own_window", false);
            if (has_own_window)
            {
                ChildWindow* window = ShowWidget(widget, true);
                const auto xpos = settings.GetValue("MainWindow", "window_xpos", window->x());
                const auto ypos = settings.GetValue("MainWindow", "window_ypos", window->y());
                const auto width = settings.GetValue("MainWindow", "window_width", window->width());
                const auto height = settings.GetValue("MainWindow", "window_height", window->height());
                if (xpos < size.width() && ypos < size.height())
                    window->move(xpos, ypos);

                window->resize(width, height);
                window->setWindowTitle(title);
            } else
            {
                ShowWidget(widget, false);
            }
            // remove the file, no longer needed.
            QFile::remove(file);
            DEBUG("Loaded main widget. [name='%1']", title);
        }
    }

    // like magic setting the window title fails when  the workspace load
    // happens as part of the app startup, i.e. when loading the last workspace.
    // Smells like some kind of Qt bug...
    // WAR using a timer with one shot to set window title.
    QTimer::singleShot(10, this, [this] {
        if (mWorkspace)
            setWindowTitle(QString("%1 - %2").arg(APP_TITLE).arg(mWorkspace->GetName()));
    });

    SetEnabled(mUI.actionSaveWorkspace, true);
    SetEnabled(mUI.actionCloseWorkspace, true);
    SetEnabled(mUI.actionSelectResourceForEditing, true);
    SetEnabled(mUI.menuWorkspace, true);
    SetEnabled(mUI.workspace, true);
    SetEnabled(mUI.workspaceFilter, true);
    SetValue(mUI.workspaceFilter, filter_string);
    SetValue(mUI.grpHelp, mWorkspace->GetName());

    mUI.workspace->setModel(&mWorkspaceProxy);
    mWorkspaceProxy.SetModel(mWorkspace.get());
    mWorkspaceProxy.setSourceModel(mWorkspace->GetResourceModel());
    mWorkspaceProxy.SetFilterString(filter_string);
    mWorkspaceProxy.SetShowBits(show_resource_bits);
    mWorkspaceProxy.invalidate();

    const auto current_index = mWorkspace->GetUserProperty("focused_widget_index", 0);
    if (current_index < GetCount(mUI.mainTab))
    {
        mUI.mainTab->setCurrentIndex(current_index);
        on_mainTab_currentChanged(current_index);
    }
    else
    {
        on_mainTab_currentChanged(-1);
    }

    // todo: regarding migration, an unresolved issue is that
    // if there's some session/window state that is to be restored
    // there's no migration path for that state.
    // either a) figure out how to that migration or b) discard that state
    // with option b) some work might unfortunately then be lost but
    // maybe the solution is simply "don't do it".

    if (!migration_log.IsEmpty())
    {
        DlgMigrationLog dlg(this, migration_log);
        dlg.exec();
    }

    return success;
}

bool MainWindow::SaveWorkspace()
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
    for (int i=0; i<GetCount(mUI.mainTab); ++i)
    {
        const auto& temp = app::RandomString();
        const auto& path = app::GetAppHomeFilePath("temp");
        const auto& file = app::GetAppHomeFilePath("temp/" + temp + ".json");
        QDir dir;
        if (!dir.mkpath(path))
        {
            ERROR("Failed to create folder: '%1'", path);
            success = false;
            continue;
        }
        const auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));

        Settings settings(file);
        settings.SetValue("MainWindow", "class_name",   widget->metaObject()->className());
        settings.SetValue("MainWindow", "widget_id",    widget->GetId());
        settings.SetValue("MainWindow", "widget_title", widget->windowTitle());
        if (!widget->SaveState(settings))
        {
            ERROR("Failed to save main widget state. [name='%1']", widget->windowTitle());
            success = false;
            continue;
        }
        if (!settings.Save())
        {
            ERROR("Failed to save main widget settings. [name='%1']",  widget->windowTitle());
            success = false;
            continue;
        }
        session_file_list << file;
        DEBUG("Saved main widget. [name='%1']", widget->windowTitle());
    }

    // for each widget that is contained inside a window (instead of being in the main tab)
    // we (also) generate a temporary JSON file in which we save the widget's UI state.
    // When the application is relaunched we use the data in the JSON to recover
    // the widget and it's contents and also to recreate a new containing ChildWindow
    // with same dimensions and desktop position.
    for (const auto* child : mChildWindows)
    {
        const auto& temp = app::RandomString();
        const auto& path = app::GetAppHomeFilePath("temp");
        const auto& file = app::GetAppHomeFilePath("temp/" + temp + ".json");
        QDir dir;
        if (!dir.mkpath(path))
        {
            ERROR("Failed to create folder. [path='%1']", path);
            success = false;
            continue;
        }
        const MainWidget* widget = child->GetWidget();

        Settings settings(file);
        settings.SetValue("MainWindow", "class_name", widget->metaObject()->className());
        settings.SetValue("MainWindow", "widget_title", widget->windowTitle());
        settings.SetValue("MainWindow", "has_own_window", true);
        settings.SetValue("MainWindow", "window_xpos", child->x());
        settings.SetValue("MainWindow", "window_ypos", child->y());
        settings.SetValue("MainWindow", "window_width", child->width());
        settings.SetValue("MainWindow", "window_height", child->height());
        if (!widget->SaveState(settings))
        {
            ERROR("Failed to save main widget state. [name='%1']", widget->windowTitle());
            success = false;
            continue;
        }
        if (!settings.Save())
        {
            ERROR("Failed to save main widget settings. [name='%1']", widget->windowTitle());
            success = false;
        }
        session_file_list << file;
        DEBUG("Saved main widget. [name='%1']", widget->windowTitle());
    }
    mWorkspace->SetUserProperty("session_files", session_file_list);
    mWorkspace->SetUserProperty("resource_show_bits", mWorkspaceProxy.GetShowBits());
    mWorkspace->SetUserProperty("resource_filter_string", mWorkspaceProxy.GetFilterString());
    if (mCurrentWidget)
    {
        const auto index_of = mUI.mainTab->indexOf(mCurrentWidget);
        mWorkspace->SetUserProperty("focused_widget_index", index_of);
    }
    if (mPlayWindow)
    {
        mPlayWindow->SaveState("play_window");
    }

    if (!mWorkspace->SaveWorkspace())
        return false;

    return true;
}

void MainWindow::CloseWorkspace()
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
    QSignalBlocker blocker(mUI.mainTab);

    // delete widget objects in the main tab.
    while (mUI.mainTab->count())
    {
        auto* widget = qobject_cast<MainWidget*>(mUI.mainTab->widget(0));
        widget->Shutdown();
        //               !!!!! WARNING !!!!!
        // setParent(nullptr) will cause an OpenGL memory leak
        //
        // https://forum.qt.io/topic/92179/xorg-vram-leak-because-of-qt-opengl-application/12
        // https://community.khronos.org/t/xorg-vram-leak-because-of-qt-opengl-application/76910/2
        // https://bugreports.qt.io/browse/QTBUG-69429
        //
        // widget->setParent(nullptr);

        // cleverly enough this will remove the tab. so the loop
        // here must be carefully done to access the tab at index 0
        delete widget;
    }
    mUI.mainTab->clear();

    // delete child windows
    for (auto* child : mChildWindows)
    {
        child->Shutdown();
        child->close();
        delete child;
    }
    mChildWindows.clear();

    mCurrentWidget = nullptr;

    if (mGameProcess.IsRunning())
    {
        mGameProcess.Kill();
    }
    if  (mViewerProcess.IsRunning())
    {
        mViewerProcess.Kill();
    }

    if (mIPCHost)
    {
        mIPCHost->Close();
        mIPCHost.reset();
    }

    if (mIPCHost2)
    {
        mIPCHost2->Close();
        mIPCHost2.reset();
    }

    if (mPlayWindow)
    {
        mPlayWindow->Shutdown();
        mPlayWindow->close();
        mPlayWindow.reset();
    }

    // update window menu.
    UpdateWindowMenu();

    SetEnabled(mUI.actionSaveWorkspace, false);
    SetEnabled(mUI.actionCloseWorkspace, false);
    SetEnabled(mUI.actionSelectResourceForEditing, false);
    SetEnabled(mUI.menuWorkspace, false);
    SetEnabled(mUI.menuEdit, false);
    SetEnabled(mUI.menuTemp, false);
    SetEnabled(mUI.workspace, false);
    SetEnabled(mUI.workspaceFilter, false);
    SetValue(mUI.workspaceFilter, QString(""));
    SetValue(mUI.grpHelp, QString("Welcome to %1").arg(APP_TITLE));
    setWindowTitle(APP_TITLE);

    if (mDlgImgView && mDlgImgView->HasWorkspace())
    {
        mDlgImgView->SaveState();
        mDlgImgView->close();
        mDlgImgView.reset();
    }

    mWorkspaceProxy.SetModel(nullptr);
    mWorkspaceProxy.setSourceModel(nullptr);
    mWorkspace.reset();

    gfx::SetResourceLoader(mLoader.get());

    ShowHelpWidget();

    mFocusStack = FocusStack();
}

void MainWindow::showWindow()
{
    show();

    if (!mSettings.style_name.isEmpty())
    {
        app::SetTheme(mSettings.style_name);
    }
}

void MainWindow::RunGameLoopOnce()
{
    if (!mWorkspace)
        return;

    const auto elapsed_since = ElapsedSeconds();
    const auto& settings = mWorkspace->GetProjectSettings();
    const auto time_step = 1.0 / settings.updates_per_second;

    mTimeAccum += elapsed_since;

    while (mTimeAccum >= time_step)
    {
        if (mCurrentWidget)
        {
            mCurrentWidget->Update(time_step);
        }
        for (auto* child : mChildWindows)
        {
            child->Update(time_step);
        }
        mTimeTotal += time_step;
        mTimeAccum -= time_step;
    }

    GfxWindow::BeginFrame();

    for (int i=0; i<GetCount(mUI.mainTab); ++i)
    {
        auto* widget = qobject_cast<MainWidget*>(mUI.mainTab->widget(i));
        widget->RunGameLoopOnce();
    }

    for (auto* child : mChildWindows)
    {
        child->RunGameLoopOnce();
    }

    if (mCurrentWidget)
    {
        mCurrentWidget->Render();
    }
    for (auto* child : mChildWindows)
    {
        child->Render();
    }

    if (mPlayWindow)
    {
        mPlayWindow->RunGameLoopOnce();
    }

    UpdateStats();

    GfxWindow::EndFrame();

    // could be changed through the widget's hotkey handler.
    //mSettings.vsync = GfxWindow::GetVSYNC();
}

void MainWindow::on_menuEdit_aboutToShow()
{
    DEBUG("Edit menu about to show.");

    if (!mCurrentWidget)
    {
        mUI.actionCut->setEnabled(false);
        mUI.actionCopy->setEnabled(false);
        mUI.actionPaste->setEnabled(false);
        mUI.actionUndo->setEnabled(false);
        return;
    }
    // Paste won't work correctly when invoked through the menu.
    // The problem is that we're using QWindow inside GfxWidget and that
    // means that when the menu opens the widget loses keyboard focus.
    // If the widget checks inside MainWidget::Paste whether the GfxWidget actually
    // is focused or not this won't work correctly. And it kinda needs to do this
    // inorder to implement the paste only when the window is actually in focus.
    // As in it'd be weird if something was pasted into the gfx widget while some
    // other widget actually had the focus.

    // instead of having some complicated signal system on window activation to indicate
    // whether something can be copy/pasted we enable/disable these on last minute
    // menu activation. Then if the user wants to invoke the actions throuhg the keyboard
    // shortcuts the widget implementations will actually need to check for the
    // right state before implementing the action.
    mUI.actionCut->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanCut, &mClipboard));
    mUI.actionCopy->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanCopy, &mClipboard));
    mUI.actionPaste->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanPaste, &mClipboard));
    mUI.actionUndo->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanUndo));
}

void MainWindow::on_mainTab_currentChanged(int index)
{
    DEBUG("Main tab current changed %1", index);

    if (mCurrentWidget)
    {
        mCurrentWidget->Deactivate();
        mUI.statusBarFrame->setVisible(false);
        mFocusStack.push(mCurrentWidget->GetId());
    }

    mCurrentWidget = nullptr;
    mUI.mainToolBar->clear();
    mUI.menuTemp->clear();

    SetValue(mUI.statTime, QString(""));
    SetValue(mUI.statFps,  QString(""));
    SetValue(mUI.statVsync, QString(""));
    SetValue(mUI.statVBO, QString(""));

    if (index != -1)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(index));

        widget->Activate();
        widget->AddActions(*mUI.mainToolBar);
        widget->AddActions(*mUI.menuTemp);

        QString name = widget->metaObject()->className();
        name.remove("gui::");
        name.remove("Widget");
        mUI.menuEdit->setEnabled(true);
        mUI.menuTemp->setEnabled(true);
        mUI.menuTemp->setTitle(name);
        mCurrentWidget = widget;
        mUI.actionZoomIn->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanZoomIn));
        mUI.actionZoomOut->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanZoomOut));
        mUI.actionReloadShaders->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanReloadShaders));
        mUI.actionReloadTextures->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanReloadTextures));
        mUI.statusBarFrame->setVisible(widget->HasStats());
    }
    else
    {
        mUI.menuTemp->setEnabled(false);
        mUI.menuEdit->setEnabled(false);
        mUI.actionZoomIn->setEnabled(false);
        mUI.actionZoomOut->setEnabled(false);
        mFocusStack = FocusStack();
    }
    UpdateWindowMenu();
    ShowHelpWidget();
}

void MainWindow::on_mainTab_tabCloseRequested(int index)
{
    auto* widget = qobject_cast<MainWidget*>(mUI.mainTab->widget(index));
    if (widget->HasUnsavedChanges())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msg.setIcon(QMessageBox::Question);
        msg.setText(tr("Looks like you have unsaved changes. Would you like to save them?"));
        const auto ret = msg.exec();
        if (ret == QMessageBox::Cancel)
            return;
        else if (ret == QMessageBox::Yes)
            widget->Save();
    }
    if (widget == mCurrentWidget)
        mCurrentWidget = nullptr;

    // does not delete the widget.
    mUI.mainTab->removeTab(index);

    widget->Shutdown();
    //               !!!!! WARNING !!!!!
    // setParent(nullptr) will cause an OpenGL memory leak
    //
    // https://forum.qt.io/topic/92179/xorg-vram-leak-because-of-qt-opengl-application/12
    // https://community.khronos.org/t/xorg-vram-leak-because-of-qt-opengl-application/76910/2
    // https://bugreports.qt.io/browse/QTBUG-69429
    //
    //widget->setParent(nullptr);
    delete widget;

    // rebuild window menu.
    UpdateWindowMenu();
    FocusPreviousTab();

    QTimer::singleShot(1000, this, &MainWindow::CleanGarbage);
}

void MainWindow::on_actionExit_triggered()
{
    this->close();
}

void MainWindow::on_actionHelp_triggered()
{
    QStringList help_args;
    help_args << "-collectionFile";
    help_args << "help/help.qhc";
    QString help_name = "assistant-qt5";
#if defined(WINDOWS_OS)
    help_name.append(".exe");
#endif
    const auto& help = app::JoinPath(QCoreApplication::applicationDirPath(), help_name);
    if (!QProcess::startDetached(help, help_args))
    {
        ERROR("Failed to start assistant-qt5.");
    }
}

void MainWindow::on_actionAbout_triggered()
{
    DlgAbout dlg(this);
    dlg.exec();
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

void MainWindow::on_actionWindowPopOut_triggered()
{
    const auto index = mUI.mainTab->currentIndex();
    if (index == -1)
        return;

    auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(index));

    // does not delete the widget. should trigger currentChanged.
    mUI.mainTab->removeTab(index);
    widget->setParent(nullptr);

    ChildWindow* window = ShowWidget(widget, true);
    widget->show();
    widget->updateGeometry();
    window->updateGeometry();

    QByteArray geometry;
    if (mWorkspace->GetUserProperty(widget->GetId(), &geometry)) {
        window->restoreGeometry(geometry);
    }

    // seems that we need some delay (presumably to allow some
    // event processing to take place) on Windows before
    // calling the update geometry. Without this the window is  
    // somewhat fucked up in its appearance. (Layout is off)
    QTimer::singleShot(10, window, &QWidget::updateGeometry);
    QTimer::singleShot(10, widget, &QWidget::updateGeometry);
}

void MainWindow::on_actionCut_triggered()
{
    if (!mCurrentWidget)
        return;

    mCurrentWidget->Cut(mClipboard);
}
void MainWindow::on_actionCopy_triggered()
{
    if (!mCurrentWidget)
        return;

    mCurrentWidget->Copy(mClipboard);
}
void MainWindow::on_actionPaste_triggered()
{
    if (!mCurrentWidget)
        return;

    mCurrentWidget->Paste(mClipboard);
}

void MainWindow::on_actionUndo_triggered()
{
    if (!mCurrentWidget)
        return;

    mCurrentWidget->Undo();
}

void MainWindow::on_actionZoomIn_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->ZoomIn();
    mUI.actionZoomIn->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanZoomIn));
}
void MainWindow::on_actionZoomOut_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->ZoomOut();
    mUI.actionZoomOut->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanZoomOut));
}

void MainWindow::on_actionReloadShaders_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->ReloadShaders();
    INFO("'%1' shaders reloaded.", mCurrentWidget->windowTitle());
}

void MainWindow::on_actionReloadTextures_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->ReloadTextures();
    INFO("'%1' textures reloaded.", mCurrentWidget->windowTitle());
}

void MainWindow::on_actionNewMaterial_triggered()
{
    OpenNewWidget(new MaterialWidget(mWorkspace.get()));
}

void MainWindow::on_actionNewParticleSystem_triggered()
{
    OpenNewWidget(new ParticleEditorWidget(mWorkspace.get()));
}

void MainWindow::on_actionNewCustomShape_triggered()
{
    OpenNewWidget(new ShapeWidget(mWorkspace.get()));
}
void MainWindow::on_actionNewEntity_triggered()
{
    OpenNewWidget(new EntityWidget(mWorkspace.get()));
}
void MainWindow::on_actionNewScene_triggered()
{
    OpenNewWidget(new SceneWidget(mWorkspace.get()));
}

void MainWindow::on_actionNewScript_triggered()
{
    OpenNewWidget(new ScriptWidget(mWorkspace.get()));
}

void MainWindow::on_actionNewBlankScript_triggered()
{
    OpenNewWidget(new ScriptWidget(mWorkspace.get()));
}
void MainWindow::on_actionNewEntityScript_triggered()
{
    GenerateNewScript("Entity Script", "entity", GenerateEntityScriptSource);
}
void MainWindow::on_actionNewSceneScript_triggered()
{
    GenerateNewScript("Scene Script", "scene", GenerateSceneScriptSource);
}

void MainWindow::on_actionNewUIScript_triggered()
{
    if (!mWorkspace)
        return;

    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    const auto& filename = app::FromUtf8(script.GetId());
    const auto& fileuri  = QString("ws://lua/%1.lua").arg(filename);
    const auto& filepath = mWorkspace->MapFileToFilesystem(fileuri);
    const QFileInfo info(filepath);
    if (info.exists())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(tr("File already exists"));
        msg.setText(tr("Overwrite existing script file?\n%1").arg(filepath));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }

    QFile io;
    io.setFileName(filepath);
    if (!io.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        ERROR("Failed to open file for writing. [file='%1, error='%2']", filepath, io.error());
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle(tr("Error Occurred"));
        msg.setText(tr("There was a problem creating the script file.\n%1").arg(io.errorString()));
        msg.setStandardButtons(QMessageBox::Ok);
        return;
    }

    // TODO: refactor this and a dupe from MainWindow into a single place.
    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    stream << "-- UI script.\n\n";
    stream << "-- This script will be called for every assigned UI instance.\n";
    stream << "-- You're free to delete functions you don't need.\n\n";
    stream << "-- Called whenever the UI has been opened.\n";
    stream << "function OnUIOpen(ui)\nend\n\n";
    stream << " -- Called whenever the UI is about to be closed.\n";
    stream << "-- result is the value passed in the exit_code in the call to CloseUI.\n";
    stream << "function OnUIClose(ui, result)\nend\n\n";
    stream << "-- Called whenever some UI action such as button click etc. occurs\n";
    stream << "function OnUIAction(ui, action)\nend\n\n";
    stream << "-- Optionally called on mouse events when the flag is set.\n";
    stream << "function OnMouseMove(ui, mouse)\nend\n\n";
    stream << "-- Optionally called on mouse events when the flag is set.\n";
    stream << "function OnMousePress(ui, mouse)\nend\n\n";
    stream << "-- Optionally called on mouse events when the flag is set.\n";
    stream << "function OnMouseRelease(ui, mouse)\nend\n\n";
    stream << "-- Optionally called on keyboard events when the flag is set.\n";
    stream << "function OnKeyDown(ui, symbol, modifier_bits)\nend\n\n";
    stream << "-- Optionally called on keyboard events when the flag is set.\n";
    stream << "function OnKeyUp(ui, symbol, modifier_bits)\nend\n\n";

    io.flush();
    io.close();

    script.SetFileURI(app::ToUtf8(fileuri));
    app::ScriptResource resource(script, "UI Script");
    mWorkspace->SaveResource(resource);

    auto* widget = new ScriptWidget(mWorkspace.get(), resource);

    OpenNewWidget(widget);
}

void MainWindow::on_actionNewAnimatorScript_triggered()
{
    if (!mWorkspace)
        return;

    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    const auto& uri  = app::toString("ws://lua/%1.lua", script.GetId());
    const auto& file = mWorkspace->MapFileToFilesystem(uri);
    if (app::FileExists(file))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(tr("File Exists"));
        msg.setText(tr("Overwrite existing script file?\n%1").arg(file));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }

    QString source = GenerateAnimatorScriptSource();

    QFile::FileError err_val = QFile::FileError::NoError;
    QString err_str;
    if (!app::WriteTextFile(file, source, &err_val, &err_str))
    {
        ERROR("Failed to write file. [file='%1', err_val=%2, err_str='%3']", file, err_val, err_str);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Error Occurred");
        msg.setText(tr("Failed to write the script file. [%1]").arg(err_str));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }

    script.SetFileURI(uri);
    app::ScriptResource resource(script, "Entity/Animator Script");
    mWorkspace->SaveResource(resource);
    auto widget = new ScriptWidget(mWorkspace.get(), resource);

    OpenNewWidget(widget);
}

void MainWindow::on_actionNewTilemap_triggered()
{
    OpenNewWidget(new TilemapWidget(mWorkspace.get()));
}

void MainWindow::on_actionNewUI_triggered()
{
    OpenNewWidget(new UIWidget(mWorkspace.get()));
}
void MainWindow::on_actionNewAudioGraph_triggered()
{
    OpenNewWidget(new AudioWidget(mWorkspace.get()));
}

void MainWindow::on_actionImportFiles_triggered()
{
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Files"));
    if (files.isEmpty())
        return;
    ImportFiles(files);
}

void MainWindow::on_actionImportTiles_triggered()
{
    if (!mWorkspace)
        return;

    DlgTileImport dlg(this, mWorkspace.get());
    dlg.LoadGeometry();
    dlg.show();
    dlg.LoadState();
    dlg.exec();
}

void MainWindow::on_actionExportJSON_triggered()
{
    if (!mWorkspace)
        return;
    const auto& indices = GetSelection(mUI.workspace);
    if (indices.isEmpty())
        return;
    const auto& filename = QFileDialog::getSaveFileName(this,
        tr("Export Resource Json"),
        tr("resource.json"),
        tr("JSON (*.json)"));
    if (filename.isEmpty())
        return;

    if (!mWorkspace->ExportResourceJson(indices, filename))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to export the JSON to a file.\n"
                       "Please see the log for details."));
        msg.exec();
        return;
    }
    NOTE("Exported %1 resource(s) to '%2'", indices.size(), filename);
    INFO("Exported %1 resource(s) to '%2'", indices.size(), filename);
    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Information);
    msg.setStandardButtons(QMessageBox::Ok);
    msg.setText(tr("Exported %1 resource(s) to '%2'").arg(indices.size()).arg(filename));
    msg.exec();
}

void MainWindow::on_actionImportJSON_triggered()
{
    if (!mWorkspace)
        return;
    const auto& filename = QFileDialog::getOpenFileName(this,
        tr("Import Resources from Json"),
        QString(), tr("JSON (*.json)"));
    if (filename.isEmpty())
        return;

    std::vector<std::unique_ptr<app::Resource>> resources;
    if (!app::Workspace::ImportResourcesFromJson(filename, resources))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to import the resources from JSON.\n"
                       "Please see the log for details."));
        msg.exec();
        return;
    }

    std::size_t import_count = 0;

    for (const auto& resource : resources)
    {
        if (const auto* previous = mWorkspace->FindResourceById(resource->GetId()))
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Question);
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msg.setText(tr("A resource with this ID (%1, '%2') already exists in the workspace.\n"
                           "Overwrite resource?").arg(previous->GetId()).arg(previous->GetName()));
            if (msg.exec() == QMessageBox::No)
                continue;
        }
        mWorkspace->SaveResource(*resource);
        ++import_count;
    }
    if (import_count)
    {
        NOTE("Imported %1 resources into workspace.", import_count);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Information);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Imported %1 resources into workspace.").arg(import_count));
        msg.exec();
    }
}

void MainWindow::on_actionImportZIP_triggered()
{
    if (!mWorkspace)
        return;

    DlgImport dlg(this, mWorkspace.get());
    dlg.exec();
}

void MainWindow::on_actionExportZIP_triggered()
{
    if (!mWorkspace)
        return;

    const auto& selection = GetSelection(mUI.workspace);
    if (selection.isEmpty())
        return;
    const auto& filename = QFileDialog::getSaveFileName(this,
        tr("Export resource(s) to Zip"),
        tr("export.zip"),
        tr("ZIP (*.zip)"));
    if (filename.isEmpty())
        return;

    std::vector<const app::Resource*> resources;

    for (int i=0; i<selection.size(); ++i)
        resources.push_back(&mWorkspace->GetUserDefinedResource(selection[i].row()));

    const auto& list = mWorkspace->ListDependencies(selection);
    for (const auto& item : list)
        resources.push_back(item.resource);

    app::Workspace::ExportOptions options;
    options.zip_file = filename;

    if (!mWorkspace->ExportResourceArchive(resources, options))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to export the resource(s) to a zip file.\n"
                       "Please see the application log for more details."));
        msg.exec();
        return;
    }
    NOTE("Exported %1 resource(s) to '%2'.", resources.size(), filename);
    INFO("Exported %1 resource(s) to '%2'.", resources.size(), filename);
    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Information);
    msg.setStandardButtons(QMessageBox::Ok);
    msg.setText(tr("Exported %1 resources to '%2'.").arg(resources.size()).arg(filename));
    msg.exec();
}

void MainWindow::on_actionEditResource_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    EditResources(open_new_window);
}

void MainWindow::on_actionEditResourceNewWindow_triggered()
{
    const auto open_new_window = true;

    EditResources(open_new_window);
}

void MainWindow::on_actionEditResourceNewTab_triggered()
{
    const auto open_new_window = false;

    EditResources(open_new_window);
}


void MainWindow::on_actionDeleteResource_triggered()
{
    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Question);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setText(tr("Are you sure you want to delete the selected resources ?"));
    if (msg.exec() == QMessageBox::No)
        return;
    const auto& selected = GetSelection(mUI.workspace);

    std::vector<QString> dead_files;
    mWorkspace->DeleteResources(selected, &dead_files);

    bool confirm_delete = true;

    for (const auto& dead_file : dead_files)
    {
        if (confirm_delete)
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel | QMessageBox::No | QMessageBox::YesAll);
            msg.setWindowTitle("Delete File?");
            msg.setText(tr("Do you want to delete the ile? [%1]").arg(dead_file));
            msg.setIcon(QMessageBox::Icon::Warning);
            const auto ret = msg.exec();
            if (ret == QMessageBox::Cancel)
                return;
            else if (ret == QMessageBox::No)
                continue;
            else if (ret == QMessageBox::YesAll)
                confirm_delete = false;
        }
        if (!QFile::remove(dead_file))
            ERROR("Failed to delete file. [file='%1']", dead_file);
        else INFO("Deleted file '%1.'", dead_file);
    }
}

void MainWindow::on_actionRenameResource_triggered()
{
    const auto& selected = GetSelection(mUI.workspace);
    for (int i=0; i<selected.size(); ++i)
    {
        auto& resource = mWorkspace->GetResource(selected[i].row());

        bool accepted = false;
        const auto& name = QInputDialog::getText(this, tr("Rename Resource"),
            tr("Resource Name:"), QLineEdit::Normal, resource.GetName(), &accepted);
        if (!accepted) continue;

        resource.SetName(name);
        mWorkspace->UpdateResource(&resource);
    }
}

void MainWindow::on_actionDuplicateResource_triggered()
{
    const auto& selected = GetSelection(mUI.workspace);
    mWorkspace->DuplicateResources(selected);
}

void MainWindow::on_actionDependencies_triggered()
{
    if (!mWorkspace)
        return;

    DlgResourceDeps dlg(this, *mWorkspace);

    const auto& selected = GetSelection(mUI.workspace);
    if (!selected.isEmpty())
        dlg.SelectItem(mWorkspace->GetResource(selected[0]));

    dlg.exec();
}

void MainWindow::on_actionSaveWorkspace_triggered()
{
    if (!mWorkspace->SaveWorkspace())
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
    const auto& file = QFileDialog::getOpenFileName(this, tr("Select Workspace"),
        QString(), QString("workspace.json"));
    if (file.isEmpty())
        return;

    const QFileInfo info(file);
    const QString dir = info.path();

    // check here whether the files actually exist.
    // todo: maybe move into workspace to validate folder
    if (MissingFile(app::JoinPath(dir, "content.json")) ||
        MissingFile(app::JoinPath(dir, "workspace.json")))
    {
        // todo: could ask if the user would like to create a new workspace instead.
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("The selected folder doesn't seem to contain a valid workspace."));
        msg.exec();
        return;
    }

    // todo: should/could ask about saving. the current workspace if we have any.

    if (!SaveWorkspace())
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
    CloseWorkspace();

    // load new workspace.
    if (!LoadWorkspace(dir))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to load workspace.\n"
                    "Please See the application log for more information.").arg(dir));
        msg.exec();
        return;
    }

    if (!mRecentWorkspaces.contains(dir))
        mRecentWorkspaces.insert(0, dir);
    if (mRecentWorkspaces.size() > 10)
        mRecentWorkspaces.pop_back();

    BuildRecentWorkspacesMenu();
    ShowHelpWidget();
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
    const auto& workspace_dst_dir = QFileDialog::getExistingDirectory(this,
        tr("Select New Workspace Directory"));
    if (workspace_dst_dir.isEmpty())
        return;

    // todo: should/could ask about saving. the current workspace if we have any.
    if (!SaveWorkspace())
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
    CloseWorkspace();

    if (!MissingFile(app::JoinPath(workspace_dst_dir, "content.json")) ||
        !MissingFile(app::JoinPath(workspace_dst_dir, "workspace.json")))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("The selected folder seems to already contain a workspace.\n"
            "Are you sure you want to overwrite this ?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Question);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setText(tr("Would you like to initialize the new workspace with some starter content?"));
    if (msg.exec() == QMessageBox::Yes)
    {
        const auto& starter_src_dir = app::JoinPath(qApp->applicationDirPath(), "starter/derp/");
        const auto [success, error] = app::CopyRecursively(starter_src_dir, workspace_dst_dir);
        if (!success)
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setText(tr("Failed to copy starter content.\n%1").arg(error));
            msg.exec();
            return;
        }
    }
    else
    {
        const auto& starter_src_dir = app::JoinPath(qApp->applicationDirPath(), "starter/init/");
        const auto [success, error] = app::CopyRecursively(starter_src_dir, workspace_dst_dir);
        if (!success)
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setText(tr("Failed to initialize new workspace.\n%1").arg(error));
            msg.exec();
            return;
        }
    }

    LoadWorkspace(workspace_dst_dir);
    if (mWorkspace)
        mWorkspace->SetProjectId(app::RandomString());

    if (!mRecentWorkspaces.contains(workspace_dst_dir))
        mRecentWorkspaces.insert(0, workspace_dst_dir);
    if (mRecentWorkspaces.size() > 10)
        mRecentWorkspaces.pop_back();

    BuildRecentWorkspacesMenu();
    ShowHelpWidget();
    NOTE("New workspace created.");
}

void MainWindow::on_actionCloseWorkspace_triggered()
{
    // todo: should/could ask about saving. the current workspace if we have any.
    if (!SaveWorkspace())
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
    CloseWorkspace();
}

void MainWindow::on_actionSettings_triggered()
{
    const QString current_style = mSettings.style_name;

    ScriptWidget::Settings script_widget_settings;
    ScriptWidget::GetDefaultSettings(&script_widget_settings);

    DlgSettings dlg(this, mSettings, script_widget_settings, mUISettings);
    if (dlg.exec() == QDialog::Rejected)
        return;

    SaveSettings();

    ScriptWidget::SetDefaultSettings(script_widget_settings);
    GfxWindow::SetDefaultClearColor(ToGfx(mSettings.clear_color));
    // disabling this setting for now.
    //GfxWindow::SetVSYNC(mSettings.vsync);
    GfxWindow::SetMouseCursor(mSettings.mouse_cursor);
    gui::SetGridColor(ToGfx(mSettings.grid_color));

    mUI.mainTab->setTabPosition(mSettings.main_tab_position);

    if (current_style == mSettings.style_name)
        return;

    app::SetStyle(mSettings.style_name);
    app::SetTheme(mSettings.style_name);

    // restyle the wigets.
    const QWidgetList topLevels = QApplication::topLevelWidgets();
    for (QWidget* widget : topLevels)
    {
        // this is needed with Qt >= 5.13.1 but is harmless otherwise
        widget->setAttribute(Qt::WA_NoSystemBackground, false);
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
    }

    // Qt5 has QEvent::ThemeChange
    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget* widget : widgets)
    {
        QEvent event(QEvent::ThemeChange);
        QApplication::sendEvent(widget, &event);
    }
}

void MainWindow::on_actionImagePacker_triggered()
{
    if (!mDlgImgPack)
        mDlgImgPack = std::make_unique<DlgImgPack>(nullptr);

    mDlgImgPack->show();
    mDlgImgPack->activateWindow();
}

void MainWindow::on_actionImageViewer_triggered()
{
    if (!mDlgImgView)
    {
        mDlgImgView = std::make_unique<DlgImgView>(nullptr);
        mDlgImgView->SetWorkspace(mWorkspace.get());
        mDlgImgView->LoadGeometry();
        mDlgImgView->show();
        mDlgImgView->LoadState();
    }
    else
    {
        mDlgImgView->show();
        mDlgImgView->activateWindow();
    }
}

void MainWindow::on_actionSvgViewer_triggered()
{
    if (!mDlgSvgView)
        mDlgSvgView = std::make_unique<DlgSvgView>(nullptr);

    mDlgSvgView->show();
    mDlgSvgView->activateWindow();
}

void MainWindow::on_actionFontMap_triggered()
{
    if (!mDlgFontMap)
        mDlgFontMap = std::make_unique<DlgFontMap>(nullptr);

    mDlgFontMap->show();
    mDlgFontMap->activateWindow();
}

void MainWindow::on_actionTilemap_triggered()
{
    if (!mDlgTilemap)
        mDlgTilemap = std::make_unique<DlgTilemap>(nullptr);

    mDlgTilemap->show();
    mDlgTilemap->activateWindow();
}

void MainWindow::on_actionLaunchViewer_triggered()
{
    if (!mWorkspace)
        return;

    app::IPCHost::CleanupSocket("gamestudio-local-socket-2");
    auto ipc = std::make_unique<app::IPCHost>();
    if (!ipc->Open("gamestudio-local-socket-2"))
        return;

    DEBUG("Local socket is open!");

    QStringList viewer_args;
    viewer_args << "--viewer";
    viewer_args << "--socket-name";
    viewer_args << "gamestudio-local-socket-2";
    viewer_args << "--app-style";
    viewer_args << mSettings.style_name;

    QString executable = "Detonator";
#if defined(WINDOWS_OS)
    executable.append(".exe");
#endif
    const auto& exec_file   = app::JoinPath(QCoreApplication::applicationDirPath(), executable);
    const auto& log_file    = app::JoinPath(QCoreApplication::applicationDirPath(), "viewer.log");
    const auto& viewer_cwd  = QCoreApplication::applicationDirPath();
    mViewerProcess.EnableTimeout(false);
    mViewerProcess.onFinished = [this]() {
        DEBUG("Viewer process finished.");
        if (mViewerProcess.GetError() != app::Process::Error::None)
            ERROR("Viewer process error. [error='%1']", mViewerProcess.GetError());

        mIPCHost2->Close();
        mIPCHost2.reset();
        SetEnabled(mUI.actionLaunchViewer, true);
    };
    mViewerProcess.Start(exec_file, viewer_args, log_file, viewer_cwd);
    mIPCHost2 = std::move(ipc);

    QObject::connect(mIPCHost2.get(), &app::IPCHost::ClientConnected,
         [this]() {
             QJsonObject json;
             app::JsonWrite(json, "message",      QString("settings"));
             app::JsonWrite(json, "clear_color",  mSettings.clear_color);
             app::JsonWrite(json, "grid_color",   mSettings.grid_color);
             app::JsonWrite(json, "mouse_cursor", mSettings.mouse_cursor);
             app::JsonWrite(json, "vsync",        mSettings.vsync);
             app::JsonWrite(json, "geometry",     mSettings.viewer_geometry);
             mIPCHost2->SendJsonMessage(json);
        });
    QObject::connect(mIPCHost2.get(), &app::IPCHost::JsonMessageReceived, this, &MainWindow::ViewerJsonMessageReceived);

    SetEnabled(mUI.actionLaunchViewer, false);
}

void MainWindow::on_actionClearLog_triggered()
{
    app::EventLog::get().clear();
}

void MainWindow::on_actionLogShowInfo_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Info, val);
    mEventLog.invalidate();
}
void MainWindow::on_actionLogShowWarning_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Warning, val);
    mEventLog.invalidate();
}
void MainWindow::on_actionLogShowError_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Error, val);
    mEventLog.invalidate();
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

void MainWindow::on_eventlist_customContextMenuRequested(QPoint point)
{
    QMenu menu(this);
    mUI.actionClearLog->setEnabled(!app::EventLog::get().isEmpty());
    menu.addAction(mUI.actionClearLog);
    menu.addSeparator();
    menu.addAction(mUI.actionLogShowInfo);
    menu.addAction(mUI.actionLogShowWarning);
    menu.addAction(mUI.actionLogShowError);
    menu.exec(QCursor::pos());
}

void MainWindow::on_workspace_customContextMenuRequested(QPoint)
{
    if (!mWorkspace)
        return;

    const auto& indices = GetSelection(mUI.workspace);
    mUI.actionDeleteResource->setEnabled(!indices.isEmpty());
    mUI.actionDuplicateResource->setEnabled(!indices.isEmpty());
    mUI.actionEditResource->setEnabled(!indices.isEmpty());
    mUI.actionEditResourceNewWindow->setEnabled(!indices.isEmpty());
    mUI.actionEditResourceNewTab->setEnabled(!indices.isEmpty());
    mUI.actionExportJSON->setEnabled(!indices.isEmpty());
    mUI.actionImportJSON->setEnabled(mWorkspace != nullptr);
    mUI.actionExportZIP->setEnabled(!indices.isEmpty());
    mUI.actionImportZIP->setEnabled(mWorkspace != nullptr);
    mUI.actionRenameResource->setEnabled(!indices.empty());

    for (int i=0; i<indices.size(); ++i)
    {
        const auto& resource = mWorkspace->GetResource(indices[i].row());
        if (resource.IsDataFile())
        {
            // disable edit actions if a non-native resources have been
            // selected. these need to be opened through an external editor.
            SetEnabled(mUI.actionEditResource, false);
            SetEnabled(mUI.actionEditResourceNewTab, false);
            SetEnabled(mUI.actionEditResourceNewWindow, false);
            // disable duplicate, don't know how to dupe external data files.
            SetEnabled(mUI.actionDuplicateResource, false);
        }
        else if (resource.IsScript())
        {
            // this doesn't currently do what is expected, since the script file
            // is *not* copied.
            SetEnabled(mUI.actionDuplicateResource, false);
        }
    }

    QMenu show;
    show.setTitle("Show");
    for (const auto val : magic_enum::enum_values<app::Resource::Type>())
    {
        // skip drawable it's a superclass type and not directly relevant
        // to the user.
        if (val == app::Resource::Type::Drawable)
            continue;

        const std::string name(magic_enum::enum_name(val));
        QAction* action = show.addAction(app::FromUtf8(name));
        connect(action, &QAction::toggled, this, &MainWindow::ToggleShowResource);
        action->setData(magic_enum::enum_integer(val));
        action->setCheckable(true);
        action->setChecked(mWorkspaceProxy.IsShow(val));
        action->setIcon(app::Resource::GetIcon(val));
    }

    QMenu script;
    script.setTitle("New Script");
    script.setIcon(QIcon("icons:add.png"));
    script.addAction(mUI.actionNewBlankScript);
    script.addAction(mUI.actionNewEntityScript);
    script.addAction(mUI.actionNewSceneScript);
    script.addAction(mUI.actionNewUIScript);
    script.addAction(mUI.actionNewAnimatorScript);

    QMenu import;
    import.setIcon(QIcon("icons:import.png"));
    import.setTitle("Import Resource...");
    import.addAction(mUI.actionImportTiles);
    import.addAction(mUI.actionImportFiles);
    import.addAction(mUI.actionImportJSON);
    import.addAction(mUI.actionImportZIP);

    QMenu export_;
    export_.setIcon(QIcon("icons:export.png"));
    export_.setTitle("Export...");
    export_.addAction(mUI.actionExportJSON);
    export_.addAction(mUI.actionExportZIP);
    export_.setEnabled(!indices.isEmpty());

    QMenu menu(this);
    menu.addAction(mUI.actionNewMaterial);
    menu.addAction(mUI.actionNewParticleSystem);
    menu.addAction(mUI.actionNewCustomShape);
    menu.addAction(mUI.actionNewEntity);
    menu.addAction(mUI.actionNewScene);
    menu.addAction(mUI.actionNewUI);
    menu.addAction(mUI.actionNewTilemap);
    menu.addAction(mUI.actionNewAudioGraph);
    menu.addMenu(&script);
    menu.addSeparator();
    //menu.addMenu(&import);
    //menu.addSeparator();
    menu.addAction(mUI.actionEditResource);
    menu.addAction(mUI.actionEditResourceNewWindow);
    menu.addAction(mUI.actionEditResourceNewTab);
    menu.addSeparator();
    menu.addAction(mUI.actionRenameResource);
    menu.addAction(mUI.actionDuplicateResource);
    menu.addMenu(&export_);
    menu.addSeparator();
    menu.addAction(mUI.actionDependencies);
    menu.addSeparator();
    menu.addAction(mUI.actionDeleteResource);
    menu.addSeparator();
    menu.addMenu(&show);
    menu.exec(QCursor::pos());
}

void MainWindow::on_workspace_doubleClicked()
{
    on_actionEditResource_triggered();
}

void MainWindow::on_workspaceFilter_textChanged(const QString&)
{
    if (!mWorkspace)
        return;
    mWorkspaceProxy.SetFilterString(GetValue(mUI.workspaceFilter));
    mWorkspaceProxy.invalidate();
}

void MainWindow::on_actionPackageResources_triggered()
{
    DlgPackage dlg(QApplication::activeWindow(), mSettings, *mWorkspace);
    dlg.exec();
}

void MainWindow::on_actionSelectResourceForEditing_triggered()
{
    if (!mWorkspace)
        return;

    DlgOpen dlg(QApplication::activeWindow(), *mWorkspace);
    dlg.SetOpenMode(mSettings.default_open_win_or_tab);
    if (dlg.exec() == QDialog::Rejected)
        return;

    app::Resource* resource = dlg.GetSelected();
    if (resource == nullptr)
        return;
    else if (resource->IsDataFile())
    {
        WARN("Can't edit '%1' since it's not a %2 resource.", resource->GetName(), APP_TITLE);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("Can't edit '%1' since it's not a %2 resource.").arg(resource->GetName(), APP_TITLE));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }
    if (!FocusWidget(resource->GetId()))
    {
        const auto new_window = dlg.GetOpenMode() == "Window";
        ShowWidget(MakeWidget(resource->GetType(), resource), new_window);
    }
}

void MainWindow::on_actionNewResource_triggered()
{
    if (!mWorkspace)
        return;

    DlgNew dlg(QApplication::activeWindow());
    dlg.SetOpenMode(mSettings.default_open_win_or_tab);
    if (dlg.exec() == QDialog::Rejected)
        return;

    const auto new_window = dlg.GetOpenMode()  == "Window";
    ShowWidget(MakeWidget(dlg.GetType() ), new_window);
}

void MainWindow::on_actionProjectSettings_triggered()
{
    if (!mWorkspace)
        return;

    auto settings = mWorkspace->GetProjectSettings();

    DlgProject dlg(QApplication::activeWindow(), *mWorkspace, settings);
    if (dlg.exec() == QDialog::Rejected)
        return;

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(settings.multisample_sample_count);
    format.setColorSpace(settings.config_srgb
        ? QSurfaceFormat::ColorSpace::sRGBColorSpace
        : QSurfaceFormat::ColorSpace::DefaultColorSpace);
    QSurfaceFormat::setDefaultFormat(format);

    mWorkspace->SetProjectSettings(settings);

    GfxWindow::SetDefaultFilter(settings.default_min_filter);
    GfxWindow::SetDefaultFilter(settings.default_mag_filter);
}

void MainWindow::on_actionProjectPlay_triggered()
{
    if (!mWorkspace)
        return;
    else if (mPlayWindow)
        return;
    else if (mGameProcess.IsRunning())
        return;

    const auto& settings = mWorkspace->GetProjectSettings();
    if (settings.GetApplicationLibrary().isEmpty())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't set the application library for the project.\n"
                    "The game should be built into a library (a .dll or .so file).\n"
                    "You can change the application library in the workspace settings.");
        msg.exec();
        return;
    }

    std::vector<MainWidget*> unsaved;
    for (int i=0; i<mUI.mainTab->count(); ++i)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        if (widget->HasUnsavedChanges())
        {
            if (mSettings.save_automatically_on_play)
                widget->Save();
            else unsaved.push_back(widget);
        }
    }
    for (auto* wnd : mChildWindows)
    {
        auto* widget = wnd->GetWidget();
        if (widget->HasUnsavedChanges())
        {
            if (mSettings.save_automatically_on_play)
                widget->Save();
            else unsaved.push_back(widget);
        }
    }

    // The actual saving of resources is in the dlgsave !
    if (!unsaved.empty())
    {
        DlgSave dlg(this, unsaved);
        if (dlg.exec() == QDialog::Rejected)
            return;
        const bool save_automatically = dlg.SaveAutomatically();
        mSettings.save_automatically_on_play = save_automatically;
    }


    if (settings.use_gamehost_process)
    {
        // todo: maybe save to some temporary location ?
        // Save workspace for the loading the initial content
        // in the game host
        if (!mWorkspace->SaveWorkspace())
            return;

        ASSERT(!mIPCHost);
        app::IPCHost::CleanupSocket("gamestudio-local-socket");
        auto ipc = std::make_unique<app::IPCHost>();
        if (!ipc->Open("gamestudio-local-socket"))
            return;

        connect(mWorkspace.get(), &app::Workspace::ResourceUpdated, ipc.get(),
                &app::IPCHost::ResourceUpdated);
        connect(ipc.get(), &app::IPCHost::UserPropertyUpdated, mWorkspace.get(),
                &app::Workspace::UpdateUserProperty);
        DEBUG("Local socket is open.");

        QStringList game_host_args;
        game_host_args << "--no-term-colors";
        game_host_args << "--workspace";
        game_host_args << mWorkspace->GetDir();
        game_host_args << "--app-style";
        game_host_args << mSettings.style_name;

        QString game_host_name = "EditorGameHost";
#if defined(WINDOWS_OS)
        game_host_name.append(".exe");
#endif
        const auto& game_host_file = app::JoinPath(QCoreApplication::applicationDirPath(), game_host_name);
        const auto& game_host_log  = app::JoinPath(QCoreApplication::applicationDirPath(), "game_host.log");
        const auto& game_host_cwd  = QDir::currentPath();
        mGameProcess.EnableTimeout(false);
        mGameProcess.onFinished = [this](){
            DEBUG("Game process finished.");
            if (mGameProcess.GetError() != app::Process::Error::None)
                ERROR("Game process error: '%1'", mGameProcess.GetError());

            mIPCHost->Close();
            mIPCHost.reset();
        };
        mGameProcess.onStdOut = [](const QString& msg) {
            if (msg.isEmpty())
                return;
            // read an encoded log message from the game host process.
            // todo: for the debug message we might want to figure out the
            // source file and line from the message itself by parsing the message.
            // for the time being this is skipped.
            if (msg[0] == "E")
                app::EventLog::get().write(app::Event::Type::Error, msg, "game-host");
            else if (msg[0] == "W")
                app::EventLog::get().write(app::Event::Type::Warning, msg, "game-host");
            else if (msg[0] == "I")
                app::EventLog::get().write(app::Event::Type::Info, msg, "game-host");
            else base::WriteLog(base::LogEvent::Debug, "game-host", 0, app::ToUtf8(msg));
        };
        mGameProcess.onStdErr = [](const QString& msg) {
            app::EventLog::get().write(app::Event::Type::Error, msg, "game-host");
        };
        mGameProcess.Start(game_host_file, game_host_args, game_host_log, game_host_cwd);
        mIPCHost = std::move(ipc);

    }
    else
    {
        if (!mPlayWindow)
        {
            auto window = std::make_unique<PlayWindow>(*mWorkspace);
            window->LoadState("play_window");
            window->ShowWithWAR();
            window->LoadGame();

            mPlayWindow = std::move(window);
        }
        else
        {
            // bring to the top of the window stack.
            mPlayWindow->ActivateWindow();
        }
    }
}

void MainWindow::on_actionProjectSync_triggered()
{
    if (!mWorkspace)
        return;

    DlgVCS dlg(this, mWorkspace.get(), mSettings);
    dlg.show();
    dlg.BeginScan();

    if (dlg.exec() == QDialog::Rejected)
        return;
}

void MainWindow::on_btnDemoBandit_clicked()
{
    LoadDemoWorkspace("demos/bandit");
}

void MainWindow::on_btnDemoBlast_clicked()
{
    LoadDemoWorkspace("demos/blast");
}
void MainWindow::on_btnDemoBreak_clicked()
{
    LoadDemoWorkspace("demos/breakout");
}
void MainWindow::on_btnDemoParticles_clicked()
{
    LoadDemoWorkspace("demos/particles");
}

void MainWindow::on_btnDemoPlayground_clicked()
{
    LoadDemoWorkspace("demos/playground");
}
void MainWindow::on_btnDemoUI_clicked()
{
    LoadDemoWorkspace("demos/ui");
}
void MainWindow::on_btnDemoDerp_clicked()
{
    LoadDemoWorkspace("starter/derp");
}
void MainWindow::on_btnDemoCharacter_clicked()
{
    LoadDemoWorkspace("demos/character");
}

void MainWindow::on_btnMaterial_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::Material));
}
void MainWindow::on_btnParticle_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::ParticleSystem));
}
void MainWindow::on_btnShape_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::Shape));
}
void MainWindow::on_btnEntity_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::Entity));
}
void MainWindow::on_btnScene_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::Scene));
}
void MainWindow::on_btnScript_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::Script));
}
void MainWindow::on_btnUI_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::UI));
}

void MainWindow::on_btnAudio_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::AudioGraph));
}

void MainWindow::on_btnTilemap_clicked()
{
    OpenNewWidget(MakeWidget(app::Resource::Type::Tilemap));
}

void MainWindow::RefreshUI()
{
    if (mPlayWindow)
    {
        if (mPlayWindow->IsClosed())
        {
            mPlayWindow->SaveState("play_window");
            mPlayWindow->Shutdown();
            mPlayWindow->close();
            mPlayWindow.reset();
        }
    }

    bool did_close_tab = false;
    // refresh the UI state, and update the tab widget icon/text if needed.
    for (int i=0; i<GetCount(mUI.mainTab);)
    {
        auto* widget = qobject_cast<MainWidget*>(mUI.mainTab->widget(i));
        widget->Refresh();
        const auto& icon = widget->windowIcon();
        const auto& text = widget->windowTitle();
        mUI.mainTab->setTabText(i, text);
        mUI.mainTab->setTabIcon(i, icon);
        if (widget->ShouldClose())
        {
            // does not delete the widget.
            mUI.mainTab->removeTab(i);
            // shut the widget down, release graphics resources etc.
            widget->Shutdown();
            //               !!!!! WARNING !!!!!
            // setParent(nullptr) will cause an OpenGL memory leak
            //
            // https://forum.qt.io/topic/92179/xorg-vram-leak-because-of-qt-opengl-application/12
            // https://community.khronos.org/t/xorg-vram-leak-because-of-qt-opengl-application/76910/2
            // https://bugreports.qt.io/browse/QTBUG-69429
            //
            delete widget;
            QTimer::singleShot(1000, this, &MainWindow::CleanGarbage);
            did_close_tab = true;
        }
        else
        {
            ++i;
        }
    }
    if (did_close_tab)
        FocusPreviousTab();

    // cull child windows that have been closed.
    // note that we do it this way to avoid having problems with callbacks and recursions.
    for (size_t i=0; i<mChildWindows.size(); )
    {
        ChildWindow* child = mChildWindows[i];
        if (child->ShouldPopIn())
        {
            MainWidget* widget = child->TakeWidget();
            // save the child window geometry for later "pop out"
            mWorkspace->SetUserProperty("_child_window_geometry_" + widget->GetId(), child->saveGeometry());

            child->close();
            // careful about not fucking up the iteration of this loop
            // however we're going to add as a tab so the widget will
            // go into the main tab not into mChildWindows.
            ShowWidget(widget, false /* new window */);
            // seems that we need some delay (presumably to allow some
            // event processing to take place) on Windows before
            // calling the update geometry. Without this the window is  
            // somewhat fucked up in its appearance. (Layout is off)
            QTimer::singleShot(10, widget, &QWidget::updateGeometry);
            QTimer::singleShot(10, mUI.mainTab, &QWidget::updateGeometry);
        }
        if (child->IsClosed())
        {
            QTimer::singleShot(1000, this, &MainWindow::CleanGarbage);
        }

        if (child->IsClosed() || child->ShouldPopIn())
        {
            const auto last = mChildWindows.size() - 1;
            std::swap(mChildWindows[i], mChildWindows[last]);
            mChildWindows.pop_back();
            child->close();
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

    if (mPlayWindow)
        mPlayWindow->NonGameTick();

    if (mCurrentWidget)
    {
        mUI.actionZoomIn->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanZoomIn));
        mUI.actionZoomOut->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanZoomOut));
        UpdateStats();
    }

    if (mDlgImgPack && mDlgImgPack->IsClosed())
        mDlgImgPack.reset();

    if (mDlgImgView && mDlgImgView->IsClosed())
        mDlgImgView.reset();

    if (mDlgFontMap && mDlgFontMap->IsClosed())
        mDlgFontMap.reset();

    if (mDlgSvgView && mDlgSvgView->IsClosed())
        mDlgSvgView.reset();

    if (mDlgTilemap && mDlgTilemap->IsClosed())
        mDlgTilemap.reset();
}

void MainWindow::ShowNote(const app::Event& event)
{
    if (event.type == app::Event::Type::Note)
    {
        mUI.statusbar->showMessage(event.message, 5000);
        for (const auto* child : mChildWindows)
        {
            child->ShowNote(event.message);
        }
    }
}

void MainWindow::OpenExternalImage(const QString& file)
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
    ExternalApplicationArgs args;
    args.executable_args   = mSettings.image_editor_arguments;
    args.executable_binary = mSettings.image_editor_executable;
    args.file = QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
    LaunchExternalApplication(args);
}

void MainWindow::OpenExternalShader(const QString& file)
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
    ExternalApplicationArgs args;
    args.executable_args   = mSettings.shader_editor_arguments;
    args.executable_binary = mSettings.shader_editor_executable;
    args.file = QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
    LaunchExternalApplication(args);
}

void MainWindow::OpenExternalScript(const QString& file)
{
    if (mSettings.script_editor_executable.isEmpty())
    {
        QMessageBox msg;
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't configured any external application for script files.\n"
                    "Would you like to set one now?");
        if (msg.exec() == QMessageBox::No)
            return;
        on_actionSettings_triggered();
        if (mSettings.script_editor_executable.isEmpty())
        {
            ERROR("No shader editor has been configured.");
            return;
        }
    }
    ExternalApplicationArgs args;
    args.executable_args   = mSettings.script_editor_arguments;
    args.executable_binary = mSettings.script_editor_executable;
    args.file = QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
    LaunchExternalApplication(args);
}

void MainWindow::OpenExternalAudio(const QString& file)
{
    if (mSettings.audio_editor_executable.isEmpty())
    {
        QMessageBox msg;
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't configured any external application for audio files.\n"
                    "Would you like to set one now?");
        if (msg.exec() == QMessageBox::No)
            return;
        on_actionSettings_triggered();
        if (mSettings.audio_editor_executable.isEmpty())
        {
            ERROR("No audio editor has been configured.");
            return;
        }
    }
    ExternalApplicationArgs args;
    args.executable_args   = mSettings.audio_editor_arguments;
    args.executable_binary = mSettings.audio_editor_executable;
    args.file = QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
    LaunchExternalApplication(args);
}

void MainWindow::OpenNewWidget(MainWidget* widget)
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(widget, open_new_window);
}

void MainWindow::RefreshWidget()
{
    auto* widget = dynamic_cast<MainWidget*>(sender());
    widget->Refresh();

    if (widget != mCurrentWidget)
        return;

    UpdateStats();
}

void MainWindow::RefreshWidgetActions()
{
    auto* widget = dynamic_cast<MainWidget*>(sender());
    if (widget == mCurrentWidget)
    {
        mUI.mainToolBar->clear();
        mUI.menuTemp->clear();
        widget->AddActions(*mUI.mainToolBar);
        widget->AddActions(*mUI.menuTemp);
    }
    else
    {
        for (auto* child : mChildWindows)
        {
            if (child->GetWidget() == widget)
                child->RefreshActions();
        }
    }
}

void MainWindow::LaunchScript(const QString& id)
{
    if (id.isEmpty())
        return;

    bool did_launch = false;

    for (auto* child : mChildWindows)
    {
        if (child->LaunchScript(id))
            did_launch = true;
    }
    for (int i=0; i<GetCount(mUI.mainTab); ++i)
    {
        auto* widget = qobject_cast<MainWidget*>(mUI.mainTab->widget(i));

        if (widget->LaunchScript(id))
            did_launch = true;
    }
    if (did_launch)
        return;

    // Currently we need a widget that owns the preview window
    // for the scene/entity in question for which the script applies.
    // If no such widget was open there's no way to launch the preview.
    // This would require either  restructuring the preview mechanism 
    // Or possibly then opening the widget to open the resource. (seems stupid)
    NOTE("No current preview available for this script.");
}

void MainWindow::OpenResource(const QString& id)
{
    if (id.isEmpty())
        return;
    if (auto* resource = mWorkspace->FindResourceById(id))
    {
        if (resource->IsPrimitive())
            return;
        const auto open_new_window = mSettings.default_open_win_or_tab == "Window";

        if (!FocusWidget(id))
        {
            if (auto* child = ShowWidget(MakeWidget(resource->GetType(), resource), open_new_window))
            {
                child->ActivateWindow();
            }
            else
            {
                this->activateWindow();
            }
        }
    }
}

void MainWindow::OpenRecentWorkspace()
{
    const QAction* action = qobject_cast<const QAction*>(sender());

    const QString& dir = action->data().toString();

    // check here whether the files actually exist.
    // todo: maybe move into workspace to validate folder
    if (MissingFile(app::JoinPath(dir, "content.json")) ||
        MissingFile(app::JoinPath(dir, "workspace.json")))
    {
        // todo: could ask if the user would like to create a new workspace instead.
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("'%1'\n\ndoesn't seem contain workspace files.\n"
                       "Would you like to remove it from the recent workspaces list?").arg(dir));
        if (msg.exec() == QMessageBox::Yes)
        {
            mRecentWorkspaces.removeAll(dir);
            BuildRecentWorkspacesMenu();
        }
        return;
    }

    // todo: should/could ask about saving. the current workspace if we have any changes.
    if (!SaveWorkspace())
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
    CloseWorkspace();

    // load new workspace.
    if (!LoadWorkspace(dir))
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
    NOTE("Loaded workspace.");
}

void MainWindow::ToggleShowResource()
{
    QAction* action = qobject_cast<QAction*>(sender());

    const auto payload = action->data().toInt();
    const auto type = magic_enum::enum_cast<app::Resource::Type>(payload);
    ASSERT(type.has_value());
    mWorkspaceProxy.SetVisible(type.value(), action->isChecked());
    mWorkspaceProxy.invalidate();
}

void MainWindow::CleanGarbage()
{
    GfxWindow::CleanGarbage();
}

void MainWindow::ResourceUpdated(const app::Resource* resource)
{
    // forward to one function for now
    ResourceAdded(resource);
}
void MainWindow::ResourceAdded(const app::Resource* resource)
{
    for (int i=0; i< GetCount(mUI.mainTab); ++i)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        if (widget->GetId() == resource->GetId())
        {
            widget->setWindowTitle(resource->GetName());
            mUI.mainTab->setTabText(i, resource->GetName());
            return;
        }
    }
    for (auto* child : mChildWindows)
    {
        auto* widget = child->GetWidget();
        if (widget->GetId() == resource->GetId())
        {
            widget->setWindowTitle(resource->GetName());
            child->setWindowTitle(resource->GetName());
            return;
        }
    }
}

void MainWindow::ViewerJsonMessageReceived(const QJsonObject& json)
{
    QString message;
    app::JsonReadSafe(json, "message", &message);
    DEBUG("New IPC message from viewer. [message='%1']", message);

    if (message == "viewer-geometry")
    {
        QString geometry;
        app::JsonReadSafe(json, "geometry", &geometry);
        mSettings.viewer_geometry = geometry;
    }
    else if (message == "viewer-export")
    {
        // try to bring this window to the top
        activateWindow();

        QString zip_file;
        QString folder_suggestion;
        QString prefix_suggestion;
        app::JsonReadSafe(json, "zip_file", &zip_file);
        app::JsonReadSafe(json, "folder_suggestion", &folder_suggestion);
        app::JsonReadSafe(json, "prefix_suggestion", &prefix_suggestion);
        DlgImport dlg(this, mWorkspace.get());
        if (!dlg.OpenArchive(zip_file, folder_suggestion, prefix_suggestion))
            return;
        dlg.exec();
    }
    else
    {
        WARN("Ignoring unknown JSON IPC message. [message='%1']", message);
    }
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        auto* key = static_cast<QKeyEvent*>(event);
        if (mCurrentWidget)
        {
            if (key->key() == Qt::Key_Escape)
            {
                if (mCurrentWidget->OnEscape())
                    return true;
            }
            else
            {
                if (mCurrentWidget->OnKeyDown(key))
                    return true;
            }
        }
    }
    else if (event->type() == GameLoopEvent::GetIdentity())
    {
        RunGameLoopOnce();
        return true;
    }
    return QMainWindow::event(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();

    SaveSettings();

    // try to perform an orderly shutdown.
    // first save everything and only if that is successful
    // (or the user don't care) we then close the workspace
    // and exit the application.
    if (!SaveWorkspace() || !SaveState())
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
    CloseWorkspace();

    // accept the event, will quit the application
    event->accept();

    mIsClosed = true;

    if (mDlgImgPack)
    {
        mDlgImgPack->close();
        mDlgImgPack.reset();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* drag)
{
    if (!mWorkspace)
        return;

    DEBUG("dragEnterEvent");
    drag->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!mWorkspace)
        return;

    DEBUG("dropEvent");

    const auto* mime = event->mimeData();
    if (!mime->hasUrls())
        return;

    QStringList files;

    const auto& urls = mime->urls();
    for (int i=0; i<urls.size(); ++i)
    {
        const auto& name = urls[i].toLocalFile();
        DEBUG("Local file path: %1", name);
        files.append(name);
    }
    ImportFiles(files);
}

void MainWindow::BuildRecentWorkspacesMenu()
{
    mUI.menuRecentWorkspaces->clear();
    for (const auto& recent : mRecentWorkspaces)
    {
        QAction* action = mUI.menuRecentWorkspaces->addAction(QDir::toNativeSeparators(recent));
        action->setData(recent);
        connect(action, &QAction::triggered, this, &MainWindow::OpenRecentWorkspace);
    }
}

void MainWindow::SaveSettings()
{
    Settings settings("Ensisoft", "Gamestudio Editor");
    settings.SetValue("Settings", "image_editor_executable",    mSettings.image_editor_executable);
    settings.SetValue("Settings", "image_editor_arguments",     mSettings.image_editor_arguments);
    settings.SetValue("Settings", "shader_editor_executable",   mSettings.shader_editor_executable);
    settings.SetValue("Settings", "shader_editor_arguments",    mSettings.shader_editor_arguments);
    settings.SetValue("Settings", "default_open_win_or_tab",    mSettings.default_open_win_or_tab);
    settings.SetValue("Settings", "script_editor_executable",   mSettings.script_editor_executable);
    settings.SetValue("Settings", "script_editor_arguments",    mSettings.script_editor_arguments);
    settings.SetValue("Settings", "audio_editor_executable",    mSettings.audio_editor_executable);
    settings.SetValue("Settings", "audio_editor_arguments",     mSettings.audio_editor_arguments);
    settings.SetValue("Settings", "style_name",                 mSettings.style_name);
    settings.SetValue("Settings", "save_automatically_on_play", mSettings.save_automatically_on_play);
    settings.SetValue("Settings", "python_executable",          mSettings.python_executable);
    settings.SetValue("Settings", "emsdk",                      mSettings.emsdk);
    settings.SetValue("Settings", "clear_color",                mSettings.clear_color);
    settings.SetValue("Settings", "grid_color",                 mSettings.grid_color);
    settings.SetValue("Settings", "default_grid",               mUISettings.grid);
    settings.SetValue("Settings", "default_zoom",               mUISettings.zoom);
    settings.SetValue("Settings", "snap_to_grid",               mUISettings.snap_to_grid);
    settings.SetValue("Settings", "show_viewport",              mUISettings.show_viewport);
    settings.SetValue("Settings", "show_origin",                mUISettings.show_origin);
    settings.SetValue("Settings", "show_grid",                  mUISettings.show_grid);
    settings.SetValue("Settings", "vsync",                      mSettings.vsync);
    settings.SetValue("Settings", "frame_delay",                mSettings.frame_delay);
    settings.SetValue("Settings", "mouse_cursor",               mSettings.mouse_cursor);
    settings.SetValue("Settings", "viewer_geometry",            mSettings.viewer_geometry);
    settings.SetValue("Settings", "vcs_executable",             mSettings.vcs_executable);
    settings.SetValue("Settings", "vcs_cmd_list_files",         mSettings.vcs_cmd_list_files);
    settings.SetValue("Settings", "vcs_cmd_add_file",           mSettings.vcs_cmd_add_file);
    settings.SetValue("Settings", "vcs_cmd_del_file",           mSettings.vcs_cmd_del_file);
    settings.SetValue("Settings", "vcs_cmd_commit_file",        mSettings.vcs_cmd_commit_file);
    settings.SetValue("Settings", "vcs_ignore_list",            mSettings.vcs_ignore_list);
    settings.SetValue("Settings", "main_tab_position",          mSettings.main_tab_position);

    ScriptWidget::Settings script_widget_settings;
    ScriptWidget::GetDefaultSettings(&script_widget_settings);
    settings.SetValue("ScriptWidget", "color_theme",                    script_widget_settings.theme);
    settings.SetValue("ScriptWidget", "lua_formatter_exec",             script_widget_settings.lua_formatter_exec);
    settings.SetValue("ScriptWidget", "lua_formatter_args",             script_widget_settings.lua_formatter_args);
    settings.SetValue("ScriptWidget", "lua_format_on_save",             script_widget_settings.lua_format_on_save);
    settings.SetValue("ScriptWidget", "editor_font_name",               script_widget_settings.editor_settings.font_description);
    settings.SetValue("ScriptWidget", "editor_font_size",               script_widget_settings.editor_settings.font_size);
    settings.SetValue("ScriptWidget", "editor_show_line_numbers",       script_widget_settings.editor_settings.show_line_numbers);
    settings.SetValue("ScriptWidget", "editor_highlight_syntax",        script_widget_settings.editor_settings.highlight_syntax);
    settings.SetValue("ScriptWidget", "editor_highlight_current_line",  script_widget_settings.editor_settings.highlight_current_line);
    settings.SetValue("ScriptWidget", "editor_replace_tab_with_spaces", script_widget_settings.editor_settings.replace_tabs_with_spaces);
    settings.SetValue("ScriptWidget", "editor_num_tab_spaces",          script_widget_settings.editor_settings.tab_spaces);


    if (settings.Save())
        INFO("Saved application settings.");
    else WARN("Failed to save application settings.");
}

bool MainWindow::SaveState()
{
    const auto& file  = app::GetAppHomeFilePath("state.json");
    Settings settings(file);
    settings.SetValue("MainWindow", "log_bits", mEventLog.GetShowBits());
    settings.SetValue("MainWindow", "width", width());
    settings.SetValue("MainWindow", "height", height());
    settings.SetValue("MainWindow", "xpos", x());
    settings.SetValue("MainWindow", "ypos", y());
    settings.SetValue("MainWindow", "show_toolbar", mUI.mainToolBar->isVisible());
    settings.SetValue("MainWindow", "show_statusbar", mUI.statusbar->isVisible());
    settings.SetValue("MainWindow", "show_eventlog", mUI.eventlogDock->isVisible());
    settings.SetValue("MainWindow", "show_workspace", mUI.workspaceDock->isVisible());
    settings.SetValue("MainWindow", "current_workspace", (mWorkspace ? mWorkspace->GetDir() : ""));
    settings.SetValue("MainWindow", "toolbar_and_dock_state", QMainWindow::saveState());
    settings.SetValue("MainWindow", "recent_workspaces", mRecentWorkspaces);
    return settings.Save();
}

ChildWindow* MainWindow::ShowWidget(MainWidget* widget, bool new_window)
{
    ASSERT(widget->parent() == nullptr);

    if (!widget->property("_main_window_connected_").toBool())
    {
        connect(widget, &MainWidget::OpenExternalImage,   this, &MainWindow::OpenExternalImage);
        connect(widget, &MainWidget::OpenExternalShader,  this, &MainWindow::OpenExternalShader);
        connect(widget, &MainWidget::OpenExternalScript,  this, &MainWindow::OpenExternalScript);
        connect(widget, &MainWidget::OpenExternalAudio,   this, &MainWindow::OpenExternalAudio);
        connect(widget, &MainWidget::OpenNewWidget,       this, &MainWindow::OpenNewWidget);
        connect(widget, &MainWidget::RefreshRequest,      this, &MainWindow::RefreshWidget);
        connect(widget, &MainWidget::OpenResource,        this, &MainWindow::OpenResource);
        connect(widget, &MainWidget::RequestScriptLaunch, this, &MainWindow::LaunchScript);
        connect(widget, &MainWidget::RefreshActions,      this, &MainWindow::RefreshWidgetActions);
        connect(widget, &MainWidget::FocusWidget,         this, &MainWindow::FocusWidget);
        widget->setProperty("_main_window_connected_", true);
    }

    if (new_window)
    {
        // create a new child window that will hold the widget.
        ChildWindow* child = new ChildWindow(widget, &mClipboard);
        child->SetSharedWorkspaceMenu(mUI.menuWorkspace);

        QByteArray geometry;
        if (mWorkspace->GetUserProperty("_child_window_geometry_" + widget->GetId(), &geometry))
        {
            child->restoreGeometry(geometry);
        }
        else
        {
            // resize and relocate on the desktop, by default the window seems
            // to be at a position that requires it to be immediately used and
            // resize by the user. ugh
            const auto width = std::max((int) (this->width() * 0.8), child->width());
            const auto height = std::max((int) (this->height() * 0.8), child->height());
            const auto xpos = x() + (this->width() - width) / 2;
            const auto ypos = y() + (this->height() - height) / 2;
            child->resize(width, height);
            child->move(xpos, ypos);
        }
        // showing the widget *after* resize/move might produce incorrect
        // results since apparently the window's dimensions are not fully
        // know until it has been show (presumably some layout is done)
        // however doing the show first and then move/resize is visually
        // not very pleasing.
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
    UpdateWindowMenu();
    // no child window
    return nullptr;
}

MainWidget* MainWindow::MakeWidget(app::Resource::Type type, const app::Resource* resource)
{
    auto* widget = CreateWidget(type, mWorkspace.get(), resource);
    if (resource)
    {
        widget->setWindowTitle(resource->GetName());
    }
    else
    {
        widget->InitializeSettings(mUISettings);
        widget->InitializeContent();
    }
    return widget;
}

void MainWindow::GenerateNewScript(const QString& script_name, const QString& arg_name, ScriptGen generator)
{
    if (!mWorkspace)
        return;

    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    const auto& uri = app::toString("ws://lua/%1.lua", script.GetId());
    const auto& file = mWorkspace->MapFileToFilesystem(uri);
    if (app::FileExists(file))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(tr("File Exists"));
        msg.setText(tr("Overwrite existing script file?\n%1").arg(file));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }

    QString source = generator(arg_name);

    QFile::FileError err_val = QFile::FileError::NoError;
    QString err_str;
    if (!app::WriteTextFile(file, source, &err_val, &err_str))
    {
        ERROR("Failed to write file. [file='%1', err_val=%2, err_str='%3']", file, err_val, err_str);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Error Occurred");
        msg.setText(tr("Failed to write the script file. [%1]").arg(err_str));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }

    script.SetFileURI(uri);
    app::ScriptResource resource(script, script_name);
    mWorkspace->SaveResource(resource);

    auto widget = new ScriptWidget(mWorkspace.get(), resource);

    OpenNewWidget(widget);
}

void MainWindow::ShowHelpWidget()
{
    // todo: could build the demo setup here dynamically.

    //mUI.lblBanditDir->setText(app::JoinPath(qApp->applicationDirPath(), "demos/bandit"));
    //mUI.lblBlastDir->setText(app::JoinPath(qApp->applicationDirPath(), "demos/blast"));
    //mUI.lblBreakDir->setText(app::JoinPath(qApp->applicationDirPath(), "demos/breakout"));
    //mUI.lblPlaygroundDir->setText(app::JoinPath(qApp->applicationDirPath(), "demos/playground"));

    if (mWorkspace && mCurrentWidget)
    {
        mUI.helpWidget->setVisible(false);
        mUI.mainTab->setVisible(true);
    }
    else if (mWorkspace && !mCurrentWidget)
    {
        mUI.helpWidget->setVisible(true);
        mUI.helpWidget->setCurrentIndex(0);
        mUI.mainTab->setVisible(false);
        mUI.mainToolBar->clear();
        mUI.mainToolBar->addAction(mUI.actionNewMaterial);
        mUI.mainToolBar->addAction(mUI.actionNewParticleSystem);
        mUI.mainToolBar->addAction(mUI.actionNewCustomShape);
        mUI.mainToolBar->addAction(mUI.actionNewEntity);
        mUI.mainToolBar->addAction(mUI.actionNewScene);
        mUI.mainToolBar->addAction(mUI.actionNewScript);
        mUI.mainToolBar->addAction(mUI.actionNewUI);
        mUI.mainToolBar->addAction(mUI.actionNewTilemap);
        mUI.mainToolBar->addAction(mUI.actionNewAudioGraph);
        mUI.mainToolBar->addSeparator();
        mUI.mainToolBar->addAction(mUI.actionImportFiles);
        mUI.mainToolBar->addAction(mUI.actionImportTiles);
    }
    else
    {
        mUI.mainToolBar->clear();
        mUI.mainToolBar->addAction(mUI.actionNewWorkspace);
        mUI.mainToolBar->addAction(mUI.actionLoadWorkspace);
        mUI.helpWidget->setCurrentIndex(1);
        mUI.helpWidget->setVisible(true);
        mUI.mainTab->setVisible(false);
    }
}

void MainWindow::EditResources(bool open_new_window)
{
    const auto& indices = GetSelection(mUI.workspace);
    for (int i=0; i<indices.size(); ++i)
    {
        const auto& resource = mWorkspace->GetResource(indices[i].row());
        // we don't know how to open these.
        if (resource.GetType() == app::Resource::Type::DataFile)
        {
            WARN("Can't edit '%1' since it's not a %2 resource.", resource.GetName(), APP_TITLE);
            continue;
        }

        if (!FocusWidget(resource.GetId()))
            ShowWidget(MakeWidget(resource.GetType(), &resource) , open_new_window);
    }
}

bool MainWindow::FocusWidget(const QString& id)
{
    for (int i=0; i<GetCount(mUI.mainTab); ++i)
    {
        const auto* widget = qobject_cast<const MainWidget*>(mUI.mainTab->widget(i));
        if (widget->GetId() == id)
        {
            mUI.mainTab->setCurrentIndex(i);
            this->activateWindow();
            return true;
        }
    }
    for (auto* child : mChildWindows)
    {
        const auto* widget = child->GetWidget();
        // if the window is being closed but has not yet been removed
        // the widget can be nullptr, in which case skip the check.
        if (!widget)
            continue;
        if (widget->GetId() == id)
        {
            QTimer::singleShot(10, child, &ChildWindow::ActivateWindow);
            return true;
        }
    }
    return false;
}

void MainWindow::ImportFiles(const QStringList& files)
{
    if (!mWorkspace)
        return;

    mWorkspace->ImportFilesAsResource(files);
}

void MainWindow::UpdateStats()
{
    if (!mCurrentWidget)
        return;

    MainWidget::Stats stats;
    mCurrentWidget->GetStats(&stats);
    SetValue(mUI.statTime, QString::number(stats.time));
    SetVisible(mUI.lblFps,    stats.graphics.valid);
    //SetVisible(mUI.lblVsync,  stats.graphics.valid);
    SetVisible(mUI.statFps,   stats.graphics.valid);
    //SetVisible(mUI.statVsync, stats.graphics.valid);
    SetVisible(mUI.statVBO,   stats.graphics.valid);
    SetVisible(mUI.lblVBO,    stats.graphics.valid);
    if (!stats.graphics.valid)
        return;
    const auto kb = 1024.0; // * 1024.0;
    const auto vbo_use = stats.device.static_vbo_mem_use +
                         stats.device.streaming_vbo_mem_use +
                         stats.device.dynamic_vbo_mem_use;
    const auto vbo_alloc = stats.device.static_vbo_mem_alloc +
                           stats.device.streaming_vbo_mem_alloc +
                           stats.device.dynamic_vbo_mem_alloc;
    SetValue(mUI.statVBO, app::toString("%1/%2", app::Bytes{vbo_use}, app::Bytes{vbo_alloc}));
    SetValue(mUI.statFps, QString::number((int) stats.graphics.fps));
    SetValue(mUI.statVsync, mSettings.vsync  ? QString("ON") : QString("OFF"));
}

void MainWindow::FocusPreviousTab()
{
    // pop widget IDs off of the stack until we find a  widget that
    // is still valid. I.e. haven't been closed or popped into a child window
    while (!mFocusStack.empty())
    {
        const auto& widget_id = mFocusStack.top();
        for (int i=0; i<GetCount(mUI.mainTab); ++i)
        {
            const auto* widget = qobject_cast<const MainWidget*>(mUI.mainTab->widget(i));
            if (widget->GetId() == widget_id) {
                mUI.mainTab->setCurrentIndex(i);
                mFocusStack.pop();
                return;
            }
        }
        mFocusStack.pop();
    }
}

} // namespace
