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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QtWidgets>
#  include <QMenu>
#  include "ui_childwindow.h"
#include "warnpop.h"

namespace app {
    class Workspace;
} // namespace

namespace gui
{
    class MainWidget;
    class Clipboard;

    // ChildWindow is a "container" window for MainWidgets that
    // are opened in their own windows. We need this to provide
    // for example a menu and a toolbar that are not part of the
    // MainWidget itself.
    class ChildWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        // takes ownership of the widget.
        ChildWindow(MainWidget* widget, Clipboard* clipboard);
       ~ChildWindow();

        // Returns true if the widget requires an accelerated
        // update and render loop.
        bool IsAccelerated() const;

        // Returns true if the user has closed the window.
        bool IsClosed() const
        { return mClosed; }
        // Returns true if the contents should be popped back
        // into the main tab.
        bool ShouldPopIn() const
        { return mPopInRequested; }
        // Get the contained widget.
        const MainWidget* GetWidget() const
        { return mWidget; }
        MainWidget* GetWidget()
        { return mWidget; }

        // Do the periodic UI refresh.
        void RefreshUI();
        // Refresh the actions in main toolbar and widget menu.
        void RefreshActions();

        // Animate/update the underlying widget and it's simulations
        // if any.
        void Update(double dt);

        void Render();

        void RunGameLoopOnce();

        void SetSharedWorkspaceMenu(QMenu* menu);

        void Shutdown();

        // Show a note in the status bar.
        void ShowNote(const QString& note) const;

        bool LaunchScript(const QString& id);

        // Take the mainwidget inside this child window.
        // The ownership is transferred to the caller!
        MainWidget* TakeWidget();
    public slots:
        void ActivateWindow();
    private slots:
        void on_menuEdit_aboutToShow();
        void on_actionClose_triggered();
        void on_actionPopIn_triggered();
        void on_actionCut_triggered();
        void on_actionCopy_triggered();
        void on_actionPaste_triggered();
        void on_actionUndo_triggered();
        void on_actionReloadShaders_triggered();
        void on_actionReloadTextures_triggered();
        void on_actionZoomIn_triggered();
        void on_actionZoomOut_triggered();

    private:
        virtual void keyPressEvent(QKeyEvent* key) override;
        virtual void closeEvent(QCloseEvent* event) override;
        void UpdateStats();
    private:
        Ui::ChildWindow mUI;
    private:
        MainWidget* mWidget = nullptr;
        Clipboard* mClipboard = nullptr;
        bool mClosed = false;
        bool mPopInRequested = false;
    };
} // namespace
