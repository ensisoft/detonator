// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <QTimer>
#  include <QEventLoop>
#  include <QCloseEvent>
#  include <QKeyEvent>
#include "warnpop.h"

#include "editor/app/workspace.h"
#include "editor/gui/main.h"
#include "editor/gui/fudialog.h"

namespace gui
{

// <RANT>
// And this here, children, is how you "sometimes" have to fight
// with Qt to make it do what you want it to.
//
// If I got a penny for every fucking time I've had to fight
// with Qt I would already be retired in some nice, warm, exotic
// island with a bunch of attractive local girls and cold beer
// in my hand and never fucking as much as look at C++ or Qt!
//
// </RANT>
//
// Archlinux + qt 5.15.14 + (X11 whatever) and whatever
//
// QDialog::restoreGeometry is no longer working but it
// fails to restore the dialog position. (The size works)
//
// Checking the QWidget::restoreGeometry documentation it says
//
// <snip>
// Restores the geometry and state of top-level widgets stored
// in the byte array geometry. Returns true on success;
// otherwise returns false.
// ...
// </snip>
//
// So the "jokementation" says "top-level", maybe that means
// that it in fact doesn't work for QDialogs? This would imply
// that it working previously was accidental. (The other obvious
// question is of course why would design such a retarded API
// that doesn't work half the time unless of course the designer
// was a mental midget...)
//
// So a possible workaround is to use a QTimer single shot
// to move the dialog to the right position *after* it has
// been shown. But this has the problem that the window appears
// at the default location and then jumps (when the timer
// triggers) to another location producing super ugly
// janky garbage UX.


FUDialog::FUDialog(QWidget* parent) : mParent(parent)
{
    mWindowEventFilter = new FUWindowEventFilter(this);
    mWindow = new QMainWindow;

    mWindow->setWindowFlags(Qt::Dialog);
    mWindow->installEventFilter(mWindowEventFilter);
}

FUDialog::~FUDialog()
{
    auto* centralWidget = mWindow->centralWidget();
    // if this blows up you have' missed to call CleanupFU
    ASSERT(centralWidget == nullptr);

    delete mWindowEventFilter;
    delete mWindow;
    if (Editor::DebugEditor())
        VERBOSE("Destroy FU dialog.");
}

void FUDialog::LoadGeometry(const app::Workspace* workspace, const QString& key)
{
    QByteArray geometry;
    if (workspace->GetUserProperty(key, &mGeometry))
    {
        if (Editor::DebugEditor())
        {
            VERBOSE("Previous FU dialog geometry. [key=%1]", key);
        }
        mWindow->restoreGeometry(mGeometry);
        mDidLoadGeometry = true;
    }
}
void FUDialog::SaveGeometry(app::Workspace* workspace, const QString& key) const
{
    workspace->SetUserProperty(key, mGeometry);
}

void FUDialog::showFU()
{
    InvokeFU(false);
}

int FUDialog::execFU()
{
    return InvokeFU(true);
}

void FUDialog::closeFU()
{
    mWindow->close();
}

bool FUDialog::HandleEvent(QEvent* event)
{
    if (event->type() == QEvent::Close)
    {
        auto* close = static_cast<QCloseEvent*>(event);

        if (Editor::DebugEditor())
            VERBOSE("FU Dialog close event.");

        if (OnCloseEvent())
        {
            // yes sir, we accept your request to close this FU window
            close->accept();

            // but we reject any changes made
            reject();
        }
        else
        {
            close->ignore();
        }
        return true;
    }
    else if (event->type() == QEvent::KeyPress)
    {
        const auto* key = static_cast<QKeyEvent*>(event);

        if (Editor::DebugEditor())
            VERBOSE("FU Dialog key press event. [key'%1']", key->key());

        // anything other than user clicking on whatever button that
        // calls "accept" equals *REJECTING* the dialog (and any changes)
        if (key->key() == Qt::Key_Escape)
        {
            if (OnCloseEvent())
            {
                reject();
                return true;
            }
        }
        return false;
    }
    return false;
}

void FUDialog::accept()
{
    if (Editor::DebugEditor())
        VERBOSE("FU Dialog accept");

    if (mBlocking)
    {
        mSatan.exit(QDialog::Accepted);
    }
    else
    {
        CloseFU(QDialog::Accepted);
    }

}
void FUDialog::reject()
{
    if (Editor::DebugEditor())
        VERBOSE("FU dialog reject");

    if (mBlocking)
    {
        mSatan.exit(QDialog::Rejected);
    }
    else
    {
        CloseFU(QDialog::Rejected);
    }
}

void FUDialog::SetupFU(QWidget* widget)
{
    mWindow->setCentralWidget(widget);
}

void FUDialog::CleanupFU()
{
    mWindow->centralWidget()->setParent(nullptr);
    mWindow->setCentralWidget(nullptr);
}

int FUDialog::InvokeFU(bool block)
{
    if (mParent && !mDidLoadGeometry)
    {
        const auto* widget = mWindow->centralWidget();
        auto parent_pos = mParent->mapToGlobal(mParent->pos());
        auto parent_size = mParent->size();
        auto widget_size = widget->size();

        const auto size = (parent_size  - widget_size) / 2;
        QPoint widget_pos;
        widget_pos.setX(parent_pos.x() + size.width());
        widget_pos.setY(parent_pos.y() + size.height());

        mWindow->resize(widget_size);
        mWindow->move(widget_pos);
    }

    if (block)
        mWindow->setWindowModality(Qt::WindowModality::ApplicationModal);

    mWindow->show();
    if (!block)
        return -1;

    mBlocking = true;
    auto ret = mSatan.exec();

    CloseFU(ret);
    return ret;
}

void FUDialog::CloseFU(int result)
{
    mGeometry = mWindow->saveGeometry();

    if (finished)
        finished(result);

    mWindow->removeEventFilter(mWindowEventFilter);
    mWindow->close();
}

bool FUDialog::OnCloseEvent()
{
    return true;
}

} // namespace

