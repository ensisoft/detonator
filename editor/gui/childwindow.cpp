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

#define LOGTAG "childwindow"

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include "base/assert.h"
#include "editor/app/eventlog.h"
#include "editor/gui/childwindow.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/clipboard.h"

namespace gui
{

ChildWindow::ChildWindow(MainWidget* widget, Clipboard* clipboard)
  : mWidget(widget)
  , mClipboard(clipboard)
{
    const auto& icon = mWidget->windowIcon();
    const auto& text = mWidget->windowTitle();
    const auto* klass = mWidget->metaObject()->className();
    DEBUG("Create new child window (widget=%1) '%2'", klass, text);

    mUI.setupUi(this);
    setWindowTitle(text);
    setWindowIcon(icon);

    mUI.verticalLayout->addWidget(widget);
    mUI.statusBarFrame->setVisible(widget->IsAccelerated());
    mUI.statusBar->insertPermanentWidget(0, mUI.statusBarFrame);
    QString menu_name = klass;
    menu_name.remove("gui::");
    menu_name.remove("Widget");
    mUI.menuTemp->setTitle(menu_name);
    mUI.actionZoomIn->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanZoomIn));
    mUI.actionZoomOut->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanZoomOut));
    mUI.actionReloadTextures->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanReloadTextures));
    mUI.actionReloadShaders->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanReloadShaders));
    mUI.actionCut->setShortcut(QKeySequence::Cut);
    mUI.actionCopy->setShortcut(QKeySequence::Copy);
    mUI.actionPaste->setShortcut(QKeySequence::Paste);

    mWidget->Activate();
    mWidget->AddActions(*mUI.toolBar);
    mWidget->AddActions(*mUI.menuTemp);
}

ChildWindow::~ChildWindow()
{
    Shutdown();

    DEBUG("Destroy ChildWindow");
}

bool ChildWindow::IsAccelerated() const
{
    if (!mWidget || mClosed)
        return false;
    return mWidget->IsAccelerated();
}

void ChildWindow::ShowNote(const QString& note) const
{
    mUI.statusBar->showMessage(note, 5000);
}

void ChildWindow::RefreshUI()
{
    if (mPopInRequested || mClosed)
        return;
    else if (!mWidget)
        return;

    mWidget->Refresh();
    // update the window icon and title if they have changed.
    const auto& icon = mWidget->windowIcon();
    const auto& text = mWidget->windowTitle();
    setWindowTitle(text);
    setWindowIcon(icon);

    mUI.actionZoomIn->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanZoomIn));
    mUI.actionZoomOut->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanZoomOut));
}

void ChildWindow::Update(double secs)
{
    if (mPopInRequested || mClosed)
        return;
    else if (!mWidget)
        return;
    mWidget->Update(secs);
}

void ChildWindow::Render()
{
    if (mPopInRequested || mClosed)
        return;
    else if (!mWidget)
        return;
    mWidget->Render();

    MainWidget::Stats stats;
    if (mWidget->GetStats(&stats))
    {
        SetValue(mUI.statTime, QString::number(stats.time));
        SetValue(mUI.statFps,  QString::number((int)stats.fps));
        SetValue(mUI.statVsync, stats.vsync ? QString("ON") : QString("OFF"));
    }
}

void ChildWindow::SetSharedWorkspaceMenu(QMenu* menu)
{
    mUI.menubar->insertMenu(mUI.menuEdit->menuAction(), menu);
}

void ChildWindow::Shutdown()
{
    if (mWidget)
    {
        const auto &text = mWidget->windowTitle();
        const auto &klass = mWidget->metaObject()->className();
        DEBUG("Shutdown child window (widget=%1) '%2'", klass, text);

        mUI.verticalLayout->removeWidget(mWidget);
        //               !!!!! WARNING !!!!!
        // setParent(nullptr) will cause an OpenGL memory leak
        //
        // https://forum.qt.io/topic/92179/xorg-vram-leak-because-of-qt-opengl-application/12
        // https://community.khronos.org/t/xorg-vram-leak-because-of-qt-opengl-application/76910/2
        // https://bugreports.qt.io/browse/QTBUG-69429
        //
        //mWidget->setParent(nullptr);

        // make sure we cleanup properly while all the resources
        // such as the OpenGL contexts (and the window) are still
        // valid.
        mWidget->Shutdown();

        delete mWidget;
        mWidget = nullptr;
    }
}

MainWidget* ChildWindow::TakeWidget()
{
    auto* ret = mWidget;
    mUI.verticalLayout->removeWidget(mWidget);
    mWidget->setParent(nullptr);
    mWidget = nullptr;
    return ret;
}

void ChildWindow::on_menuEdit_aboutToShow()
{
    DEBUG("Edit menu about to show.");

    if (!mWidget)
    {
        mUI.actionCut->setEnabled(false);
        mUI.actionCopy->setEnabled(false);
        mUI.actionPaste->setEnabled(false);
        return;
    }

    // see comments in MainWindow::on_menuEdit about this
    // problems related to paste functionality
    // and why we enable/disable the actions here.

    mUI.actionCut->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanCut, mClipboard));
    mUI.actionCopy->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanCopy, mClipboard));
    mUI.actionPaste->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanPaste, mClipboard));
}

void ChildWindow::on_actionClose_triggered()
{
    if (!mWidget->ConfirmClose())
        return;

    // Make sure to cleanup first while the window
    // (and the X11 surface) still exists
    Shutdown();

    mClosed = true;
    hide();
}
void ChildWindow::on_actionPopIn_triggered()
{
    mPopInRequested = true;
}

void ChildWindow::on_actionCut_triggered()
{
    if (!mWidget)
        return;
    mWidget->Cut(*mClipboard);
}
void ChildWindow::on_actionCopy_triggered()
{
    if (!mWidget)
        return;
    mWidget->Copy(*mClipboard);
}
void ChildWindow::on_actionPaste_triggered()
{
    if (!mWidget)
        return;
    mWidget->Paste(*mClipboard);
}

void ChildWindow::on_actionReloadShaders_triggered()
{
    mWidget->ReloadShaders();
    INFO("'%1' shaders reloaded.", mWidget->windowTitle());
}

void ChildWindow::on_actionReloadTextures_triggered()
{
    mWidget->ReloadTextures();
    INFO("'%1' textures reloaded.", mWidget->windowTitle());
}

void ChildWindow::on_actionZoomIn_triggered()
{
    mWidget->ZoomIn();
    mUI.actionZoomIn->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanZoomIn));
}
void ChildWindow::on_actionZoomOut_triggered()
{
    mWidget->ZoomOut();
    mUI.actionZoomOut->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanZoomOut));
}

void ChildWindow::closeEvent(QCloseEvent* event)
{
    DEBUG("Child window close event.");

    if (!mWidget)
        return;

    event->ignore();

    if (!mWidget->ConfirmClose())
        return;

    event->accept();

    const auto& text  = mWidget->windowTitle();
    const auto& klass = mWidget->metaObject()->className();
    DEBUG("Close child window (widget=%1) '%2'", klass, text);

    // Make sure to cleanup while the window and the rendering
    // surface still exist!
    Shutdown();
    
    // we could emit an event here to indicate that the window
    // is getting closed but that's a sure-fire way of getting
    // unwanted recursion that will fuck shit up.  (i.e this window
    // getting deleted which will run the destructor which will
    // make this function have an invalided *this* pointer. bad.
    // so instead of doing that we just set a flag and the
    // mainwindow will check from time to if the window object
    // should be deleted.
    mClosed = true;
}

} // namespace
