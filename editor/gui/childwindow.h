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
#  include "ui_childwindow.h"
#include "warnpop.h"

namespace app {
    class Workspace;
} // namespace

namespace gui
{
    class MainWidget;

    // ChildWindow is a "container" window for MainWidgets that
    // are opened in their own windows. We need this to provide
    // for example a menu and a toolbar that are not part of the
    // MainWidget itself.
    class ChildWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        // takes ownership of the widget.
        ChildWindow(MainWidget* widget);
       ~ChildWindow();

        // Returns true if the user has closed the window.
        bool IsClosed() const
        { return mClosed; }
        // Get the contained widget.
        const MainWidget* GetWidget() const
        { return mWidget; }
        MainWidget* GetWidget()
        { return mWidget; }

        // Do the periodic UI refresh.
        void RefreshUI();
        // Animate/update the underlying widget and it's simulations
        // if any.
        void Animate(double secs);

        void SetTargetFps(unsigned target);

    private slots:
        void on_actionClose_triggered();
        void on_actionReloadShaders_triggered();
        void on_actionReloadTextures_triggered();
        void on_actionZoomIn_triggered();
        void on_actionZoomOut_triggered();

    private:
        virtual void closeEvent(QCloseEvent* event) override;
        virtual bool eventFilter(QObject* destination, QEvent* event) override;
    private:
        Ui::ChildWindow mUI;
    private:
        MainWidget* mWidget = nullptr;
        bool mClosed = false;
    };
} // namespace
