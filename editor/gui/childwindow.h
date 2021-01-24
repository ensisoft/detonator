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

        // Returns true if the widget requires accelerated
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
        // Animate/update the underlying widget and it's simulations
        // if any.
        void Update(double dt);

        void Render();

        void SetSharedWorkspaceMenu(QMenu* menu);

        void Shutdown();

        // Show a note in the status bar.
        void ShowNote(const QString& note) const;

        // Take the mainwidget inside this child window.
        // The ownership is transferred to the caller!
        MainWidget* TakeWidget();

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
        virtual void closeEvent(QCloseEvent* event) override;
    private:
        Ui::ChildWindow mUI;
    private:
        MainWidget* mWidget = nullptr;
        Clipboard* mClipboard = nullptr;
        bool mClosed = false;
        bool mPopInRequested = false;
    };
} // namespace
