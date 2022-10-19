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

#define LOGTAG "viewwindow"

#include "config.h"

#include "warnpush.h"
#  include <QCloseEvent>
#  include <QCoreApplication>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QTimer>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/gui/viewwindow.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/materialwidget.h"
#include "editor/gui/particlewidget.h"
#include "editor/gui/polygonwidget.h"
#include "editor/gui/entitywidget.h"
#include "editor/gui/scenewidget.h"
#include "editor/gui/tilemapwidget.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/uiwidget.h"
#include "editor/gui/audiowidget.h"
#include "editor/gui/utility.h"

namespace {
class IterateGameLoopEvent : public QEvent
{
public:
    IterateGameLoopEvent() : QEvent(GetIdentity())
    {}
    static QEvent::Type GetIdentity()
    {
        static auto id = QEvent::registerEventType();
        return (QEvent::Type)id;
    }
private:
};
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
} // namespace

namespace gui
{

ViewWindow::ViewWindow(QApplication& app) : mApp(app)
{
    mUI.setupUi(this);

    QObject::connect(&mClientSocket, &app::IPCClient::JsonMessageReceived, this, &ViewWindow::JsonMessageReceived);
    // start periodic refresh timer. this is low frequency timer that is used
    // to update the widget UI if needed, such as change the icon/window title
    // and tick the workspace for periodic cleanup and stuff.
    QObject::connect(&mRefreshTimer, &QTimer::timeout, this, &ViewWindow::RefreshUI);
    mRefreshTimer.setInterval(500);
    mRefreshTimer.start();

    // set defaults
    GfxWindow::SetDefaultClearColor(ToGfx(mSettings.clear_color));
    GfxWindow::SetVSYNC(mSettings.vsync);
    GfxWindow::SetMouseCursor(mSettings.mouse_cursor);
    gui::SetGridColor(ToGfx(mSettings.grid_color));

    SetEnabled(mUI.btnExport, false);
}

ViewWindow::~ViewWindow()
{

}

bool ViewWindow::haveAcceleratedWindows() const
{
    return mCurrentWidget != nullptr;
}

void ViewWindow::Connect(const QString& local_ipc_socket)
{
    DEBUG("Connecting to local socket. [socket='%1']", local_ipc_socket);
    if (mClientSocket.Open(local_ipc_socket))
    {
        SetValue(mUI.lblStatus, QString("Connected!"));
        SetEnabled(mUI.btnExport, true);
    }
    else
    {
        SetValue(mUI.lblStatus, QString("Connection failed"));
        SetEnabled(mUI.btnExport, false);
    }
}

void ViewWindow::on_btnSelectFile_clicked()
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

    auto workspace = std::make_unique<app::Workspace>(dir);
    if (!workspace->LoadWorkspace())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to load workspace.\n"
                       "Please See the application log for more information.").arg(dir));
        msg.exec();
        return;
    }
    ShutdownWidget();

    mWorkspace = std::move(workspace);
    mUI.workspace->setModel(mWorkspace.get());
    gfx::SetResourceLoader(mWorkspace.get());
    connect(mUI.workspace->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ViewWindow::SelectResource);

    SetValue(mUI.fileSource, file);
}

void ViewWindow::on_btnExport_clicked()
{
    if (!mWorkspace || !mClientSocket.IsOpen())
        return;

    const auto& selection = GetSelection(mUI.workspace);
    if (selection.isEmpty())
        return;

    std::vector<const app::Resource*> resources;
    for (int i=0; i<selection.size(); ++i)
        resources.push_back(&mWorkspace->GetUserDefinedResource(selection[i].row()));

    const auto& deps = mWorkspace->ListDependencies(selection);
    for (const auto& item : deps)
        resources.push_back(item.resource);

    const auto& name = app::RandomString();
    const auto& temp = QDir::tempPath();
    const auto& file = app::JoinPath(temp, name + ".zip");

    app::Workspace::ExportOptions options;
    options.zip_file = file;
    if (!mWorkspace->ExportResourceArchive(resources, options))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to export the resource(s) to a zip file.\n"
                       "Please see the application log file for more details."));
        msg.exec();
        return;
    }
    const auto& settings = mWorkspace->GetProjectSettings();

    QJsonObject json;
    app::JsonWrite(json, "message", QString("viewer-export"));
    app::JsonWrite(json, "zip_file", file);
    app::JsonWrite(json, "folder_suggestion", settings.application_name);
    app::JsonWrite(json, "prefix_suggestion", settings.application_name);
    mClientSocket.SendJsonMessage(json);
}

void ViewWindow::on_btnClose_clicked()
{
    mClosed = true;

    ShutdownWidget();
    SendWindowState();

    emit aboutToClose();
}

void ViewWindow::SelectResource(const QItemSelection&, const QItemSelection&)
{
    ShutdownWidget();

    const auto row = GetCurrentRow(mUI.workspace);
    if (row == -1)
        return;

    const auto& resource = mWorkspace->GetResource(row);
    // we don't know how to open these.
    if (resource.GetType() == app::Resource::Type::DataFile ||
        resource.GetType() == app::Resource::Type::AudioGraph ||
        resource.GetType() == app::Resource::Type::Script ||
        resource.GetType() == app::Resource::Type::Scene ||
        resource.GetType() == app::Resource::Type::Tilemap)
    {
        SetVisible(mUI.lblPreview, true);
        return;
    }
    SetVisible(mUI.lblPreview, false);

    mCurrentWidget = CreateWidget(resource.GetType(), mWorkspace.get(), &resource);
    mCurrentWidget->SetViewerMode();
    mUI.layout->addWidget(mCurrentWidget);

    emit newAcceleratedWindowOpen();
    QCoreApplication::postEvent(this, new IterateGameLoopEvent());
}

void ViewWindow::JsonMessageReceived(const QJsonObject& json)
{
    QString message;
    app::JsonReadSafe(json, "message", &message);
    DEBUG("New IPC message from editor. [message='%1']", message);

    if (message == "settings")
    {
        app::JsonReadSafe(json, "clear_color",  &mSettings.clear_color);
        app::JsonReadSafe(json, "grid_color",   &mSettings.grid_color);
        app::JsonReadSafe(json, "mouse_cursor", &mSettings.mouse_cursor);
        app::JsonReadSafe(json, "vsync",        &mSettings.vsync);
        app::JsonReadSafe(json, "geometry",     &mSettings.viewer_geometry);
        // set defaults
        GfxWindow::SetVSYNC(mSettings.vsync);
        GfxWindow::SetDefaultClearColor(ToGfx(mSettings.clear_color));
        GfxWindow::SetMouseCursor(mSettings.mouse_cursor);
        gui::SetGridColor(ToGfx(mSettings.grid_color));

        if (!mSettings.viewer_geometry.isEmpty())
        {
            const auto& geometry = QByteArray::fromBase64(mSettings.viewer_geometry.toLatin1());
            this->restoreGeometry(geometry);
        }

        DEBUG("Received IPC settings JSON message.");
    }
    else
    {
        WARN("Ignoring unknown JSON IPC message. [message='%1']", message);
    }
}

bool ViewWindow::event(QEvent* event)
{
    if (event->type() == IterateGameLoopEvent::GetIdentity())
    {
        IterateGameLoop();

        if (haveAcceleratedWindows())
            QCoreApplication::postEvent(this, new IterateGameLoopEvent);

        return true;
    }
    return QMainWindow::event(event);
}


void ViewWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();

    mClosed = true;

    ShutdownWidget();
    SendWindowState();

    emit aboutToClose();
}

void ViewWindow::IterateGameLoop()
{
    if (!mWorkspace || !mCurrentWidget)
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
        mTimeTotal += time_step;
        mTimeAccum -= time_step;
    }

    GfxWindow::BeginFrame();
    mCurrentWidget->Render();
    GfxWindow::EndFrame(0 /* frame delay */);
}

void ViewWindow::ShutdownWidget()
{
    if (mCurrentWidget)
    {
        mCurrentWidget->Shutdown();
        mUI.layout->removeWidget(mCurrentWidget);
        delete mCurrentWidget;
        mCurrentWidget = nullptr;
    }
}

void ViewWindow::RefreshUI()
{
    if (mCurrentWidget)
        mCurrentWidget->Refresh();
}

void ViewWindow::SendWindowState()
{
    if (!mClientSocket.IsOpen())
        return;

    const auto& geometry = saveGeometry();
    QJsonObject json;
    app::JsonWrite(json, "message", QString("viewer-geometry"));
    app::JsonWrite(json, "geometry", QString::fromLatin1(geometry.toBase64()));
    mClientSocket.SendJsonMessage(json);
}

} // gui
