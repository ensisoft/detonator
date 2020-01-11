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
#  include "ui_mainwindow.h"
#include "warnpop.h"

#include <vector>
#include <memory>

namespace gui
{
    class MainWidget;
    class Settings;

    // Main application window. Composes several MainWidgets
    // into a single cohesive window object that the user can
    // interact with. 
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        // Construct with settings.
        MainWindow(Settings& settings);
       ~MainWindow();

        // Attach a new permanent MainWidget to the window and display it.
        // The ownership of the widget remains with the caller.
        // Permanent widgets get an entry in the view menu and can be 
        // toggled on/off but are never deleted by MainWindow.
        void attachPermanentWidget(MainWidget* widget);        

        // Attach a temporary session widget. Session widgets
        // are spawned dynamically during the lifetime of the application
        // and there can several widgets of the same type. 
        // When the user diposes of them they're deleted by the MainWindow.
        void attachSessionWidget(std::unique_ptr<MainWidget> widget);

        // Detach all current widgets both temporary and permanent.
        // You should call this before the application ends to make
        // sure that the any widgets that should not get deleted 
        // get deleted by Qt's object hierachy system.
        void detachAllWidgets();

        // Close the widget object. If the widget is permanent
        // it's hidden and the view menu item is toggled. Otherwise
        // it's deleted.
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

        // Startup the mainwindow and all the MainWidges. Should be called
        // as the last step of the application startup process  after all
        // the state has been loaded and application activities can start.
        void startup();

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
        void actionWindowToggleView_triggered();
        void actionWindowFocus_triggered();        
        void refreshUI();
        void showNote(const app::Event& event);

    private:
        void showWidget(const QString& name);
        void showWidget(MainWidget* widget);
        void showWidget(std::size_t index);
        void hideWidget(MainWidget* widget);
        void hideWidget(std::size_t index);
        bool saveState();
        

    private:
        void closeEvent(QCloseEvent* event) Q_DECL_OVERRIDE;
        bool eventFilter(QObject* destination, QEvent* event) override;

    private:
        Ui::MainWindow mUI;

    private:
        QTimer mTimer;
        Settings& mSettings;

    private:
        std::vector<MainWidget*> mWidgets;
        std::vector<QAction*>    mActions;
        MainWidget* mCurrentWidget = nullptr;
    };

} // namespace

