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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QtWidgets>
#  include <QTimer>
#  include <QElapsedTimer>
#  include "ui_mainwindow.h"
#include "warnpop.h"

#include <vector>
#include <memory>

#include "editor/app/workspace.h"
#include "editor/gui/appsettings.h"

namespace gui
{
    class MainWidget;
    class Settings;
    class ChildWindow;

    // Main application window. Composes several MainWidgets
    // into a single cohesive window object that the user can
    // interact with.
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        MainWindow();
       ~MainWindow();

        // Close the widget object and delete it.
        void closeWidget(MainWidget* widget);

        // Move focus of the application to this widget if the widget
        // is currently being shown. If the widget is not being shown
        // then does nothing.
        void focusWidget(const MainWidget* widget);

        // Load the MainWindow and all MainWidget states.
        void loadState();

        // Prepare the file menu to have actions such as "New X ..."
        // The actions available are loaded from the registered MainWidgets.
        void prepareFileMenu();

        // Prepare (enumerate) the currently open MainWidgets in the window menu
        // and prepare window swapping shortcuts.
        void prepareWindowMenu();

        // Prepare the main tab for first display.
        void prepareMainTab();

        // Show the application window.
        void showWindow();

    private slots:
        void on_mainTab_currentChanged(int index);
        void on_mainTab_tabCloseRequested(int index);
        void on_actionExit_triggered();
        void on_actionWindowClose_triggered();
        void on_actionWindowNext_triggered();
        void on_actionWindowPrev_triggered();
        void on_actionZoomIn_triggered();
        void on_actionZoomOut_triggered();
        void on_actionReloadShaders_triggered();
        void on_actionReloadTextures_triggered();
        void on_actionNewMaterial_triggered();
        void on_actionNewParticleSystem_triggered();
        void on_actionNewAnimation_triggered();
        void on_actionEditResource_triggered();
        void on_actionEditResourceNewWindow_triggered();
        void on_actionEditResourceNewTab_triggered();
        void on_actionDeleteResource_triggered();
        void on_actionSaveWorkspace_triggered();
        void on_actionLoadWorkspace_triggered();
        void on_actionNewWorkspace_triggered();
        void on_actionCloseWorkspace_triggered();
        void on_actionSettings_triggered();
        void on_actionImagePacker_triggered();
        void on_workspace_customContextMenuRequested(QPoint);
        void on_workspace_doubleClicked();
        void on_actionPackageResources_triggered();
        void on_actionSelectResourceForEditing_triggered();
        void on_actionNewResource_triggered();
        void on_actionProjectSettings_triggered();

        void actionWindowFocus_triggered();

        void timerLoRes();
        void timerHiRes();
        void showNote(const app::Event& event);
        void openExternalImage(const QString& file);
        void openExternalShader(const QString& file);

    private:
        bool saveState();
        bool loadWorkspace(const QString& dir);
        bool saveWorkspace();
        void closeWorkspace();
        ChildWindow* showWidget(MainWidget* widget, bool new_window);
        void editResources(bool open_new_window);

    private:
        void closeEvent(QCloseEvent* event) Q_DECL_OVERRIDE;
        bool eventFilter(QObject* destination, QEvent* event) override;

    private:
        Ui::MainWindow mUI;

    private:
        // the refesh timer to do low frequency UI updates.
        QTimer mTimerLoRes;
        // the high frequency time to run widget animations
        QTimer mTimerHiRes;
        QElapsedTimer mHiResInterval;

        // current application settings that are not part of any
        // other state. Loaded on startup and saved on exit.
        AppSettings mSettings;

        // currently focused (current) widget in the main tab.
        MainWidget* mCurrentWidget = nullptr;

        // workspace object.
        std::unique_ptr<app::Workspace> mWorkspace;

        // the list of child windows that are opened to show mainwidgets.
        // it's possible to open some resource in a separate window
        // in which case a new ChildWindow is opened to display/contain the MainWidget.
        // instead of the tab in this window.
        std::vector<ChildWindow*> mChildWindows;
    };

} // namespace

