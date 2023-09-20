// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include "editor/app/platform.h"
#include "editor/app/process.h"
#include "editor/app/workspace.h"
#include "editor/gui/dlgcomplete.h"
#include "editor/gui/utility.h"

namespace gui
{

DlgComplete::DlgComplete(QWidget* parent,
                         const app::Workspace& workspace,
                         const app::Workspace::ContentPackingOptions& package)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mPackage(package)
{
    mUI.setupUi(this);

    QStringList browsers;
    browsers << "Default Browser";

#if defined(LINUX_OS)
    if (app::FileExists("/usr/bin/firefox"))
        browsers << "Firefox";
    if (app::FileExists("/usr/bin/chromium"))
        browsers << "Chromium";
    if (app::FileExists("/usr/bin/chrome"))
        browsers << "Chrome";
#elif defined(WINDOWS_OS)
    if (app::FileExists("C:\\Program Files\\Mozilla Firefox\\firefox.exe"))
        browsers << "Firefox";
    if (app::FileExists("C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe"))
        browsers << "Chrome";
    if (app::FileExists("C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe"))
        browsers << "Edge";
#endif
    SetList(mUI.cmbBrowser, browsers);
}

DlgComplete::~DlgComplete()
{
    if (mPython.IsRunning())
        mPython.Kill();
    if (mGame.IsRunning())
        mGame.Kill();
}

void DlgComplete::on_btnOpenFolder_clicked()
{
    app::OpenFolder(mPackage.directory);
}
void DlgComplete::on_btnPlayBrowser_clicked()
{
    // todo error checking and stuff

    if (!mPython.IsRunning())
    {
        const auto& script = app::JoinPath(mPackage.directory, "http-server.py");
        const auto& output = app::JoinPath(mPackage.directory, "http-server.log");
        QStringList args;
        args << script;
        mPython.Start(mPackage.python_executable, args, output, mPackage.directory);
    }

    const QString& browser = GetValue(mUI.cmbBrowser);
    if (browser == "Default Browser")
    {
        app::OpenWeb("http://localhost:8000/game.html");
        return;
    }

    app::ExternalApplicationArgs args;
    args.uri_arg = "http://localhost:8000/game.html";
    args.executable_args = "${uri}";

    if (browser == "Firefox")
    {
#if defined(LINUX_OS)
        args.executable_binary = "/usr/bin/firefox";
#elif defined(WINDOWS_OS)
        args.executable_binary = "C:\\Program Files\\Mozilla Firefox\\firefox.exe";
#endif
    }
    else if (browser == "Chromium")
    {
        args.executable_binary = "/usr/bin/chromium";
    }
    else if (browser == "Chrome")
    {
#if defined(LINUX_OS)
        args.executable_binary = "chrome";
#elif defined(WINDOWS_OS)
        args.executable_args = "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";
#endif
    }
    else if (browser == "Edge")
    {
        args.executable_binary = "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
    }
    app::LaunchExternalApplication(args);
}
void DlgComplete::on_btnPlayNative_clicked()
{
    if (mGame.IsRunning())
        return;

    // TODO: fix this name stuff here. it's duplicated in the workspace package

    const auto& settings = mWorkspace.GetProjectSettings();

    QString game_name = settings.application_name;
    if (game_name.isEmpty())
        game_name = "GameMain";
#if defined(WINDOWS_OS)
    game_name.append(".exe");
#endif

    const auto& game_exec = app::JoinPath(mPackage.directory, game_name);
    const auto game_out = app::JoinPath(mPackage.directory, "game.log");
    QStringList args;
    mGame.Start(game_exec, args, game_out, mPackage.directory);
}

void DlgComplete::on_btnClose_clicked()
{
    close();
}

} // namespace
