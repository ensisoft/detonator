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
    mUI.statusBarFrame->setVisible(widget->HasStats());
    mUI.statusBar->insertPermanentWidget(0, mUI.statusBarFrame);
    SetVisible(mUI.lblFps,    false);
    SetVisible(mUI.lblVsync,  false);
    SetVisible(mUI.statFps,   false);
    SetVisible(mUI.statVsync, false);
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
    mUI.actionUndo->setShortcut(QKeySequence::Undo);

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

    UpdateStats();

    if (mWidget->ShouldClose())
    {
        Shutdown();

        hide();

        mClosed = true;
    }
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

    UpdateStats();
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

void ChildWindow::ActivateWindow()
{
    this->activateWindow();
    this->raise();
}

void ChildWindow::on_menuEdit_aboutToShow()
{
    DEBUG("Edit menu about to show.");

    if (!mWidget)
    {
        mUI.actionCut->setEnabled(false);
        mUI.actionCopy->setEnabled(false);
        mUI.actionPaste->setEnabled(false);
        mUI.actionUndo->setEnabled(false);
        return;
    }

    // see comments in MainWindow::on_menuEdit about this
    // problems related to paste functionality
    // and why we enable/disable the actions here.

    mUI.actionCut->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanCut, mClipboard));
    mUI.actionCopy->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanCopy, mClipboard));
    mUI.actionPaste->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanPaste, mClipboard));
    mUI.actionUndo->setEnabled(mWidget->CanTakeAction(MainWidget::Actions::CanUndo));
}

void ChildWindow::on_actionClose_triggered()
{
    if (mWidget->HasUnsavedChanges())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msg.setIcon(QMessageBox::Question);
        msg.setText(tr("Looks like you have unsaved changes. Would you like to save them?"));
        const auto ret = msg.exec();
        if (ret == QMessageBox::Cancel)
            return;
        else if (ret == QMessageBox::Yes)
            mWidget->Save();
    }
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

void ChildWindow::on_actionUndo_triggered()
{
    if (!mWidget)
        return;
    mWidget->Undo();
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

void ChildWindow::keyPressEvent(QKeyEvent* key)
{
    if (key->key() != Qt::Key_Escape)
        return QMainWindow::keyPressEvent(key);
    mWidget->OnEscape();
}

void ChildWindow::closeEvent(QCloseEvent* event)
{
    DEBUG("Child window close event.");

    if (!mWidget)
        return;

    event->ignore();

    if (mWidget->HasUnsavedChanges())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msg.setIcon(QMessageBox::Question);
        msg.setText(tr("Looks like you have unsaved changes. Would you like to save them?"));
        const auto ret = msg.exec();
        if (ret == QMessageBox::Cancel)
            return;
        else if (ret == QMessageBox::Yes)
            mWidget->Save();
    }
    
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

void ChildWindow::UpdateStats()
{
    if (!mWidget)
        return;

    MainWidget::Stats stats;
    mWidget->GetStats(&stats);
    SetValue(mUI.statTime, QString::number(stats.time));
    SetVisible(mUI.lblFps,    stats.graphics.valid);
    SetVisible(mUI.lblVsync,  stats.graphics.valid);
    SetVisible(mUI.statFps,   stats.graphics.valid);
    SetVisible(mUI.statVsync, stats.graphics.valid);
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
    SetValue(mUI.statVBO, QString("%1/%2 kB")
            .arg(vbo_use / kb, 0, 'f', 1, ' ').arg(vbo_alloc / kb, 0, 'f', 1, ' '));

    SetValue(mUI.statFps, QString::number((int) stats.graphics.fps));
    SetValue(mUI.statVsync, stats.graphics.vsync ? QString("ON") : QString("OFF"));
}

} // namespace
