// Copyright (c) 2016 Sami Väisänen, Ensisoft
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

#define LOGTAG "eventlog"

#include "config.h"

#include "app/event.h"
#include "app/eventlog.h"
#include "eventwidget.h"

namespace gui
{

EventWidget::EventWidget()
{
    auto& model = app::EventLog::get();

    mUI.setupUi(this);
    mUI.listView->setModel(&model);

    QObject::connect(&model, SIGNAL(newEvent(const app::Event&)),
        this, SLOT(newEvent(const app::Event&)));

    mNumWarnings = 0;
    mNumErrors   = 0;
}

EventWidget::~EventWidget()
{}

void EventWidget::activate()
{
    mNumErrors   = 0;
    mNumWarnings = 0;
    setWindowIcon(QIcon("icons:log_info.png"));
    setWindowTitle("Log");
}

void EventWidget::addActions(QToolBar& bar)
{
    bar.addAction(mUI.actionClearLog);
}
void EventWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionClearLog);
}

void EventWidget::newEvent(const app::Event& event)
{
    if (event.type == app::Event::Type::Note)
        return;

    mUI.actionClearLog->setEnabled(true);

    if (event.type == app::Event::Type::Error)
    {
        mNumErrors++;
        setWindowIcon(QIcon("icons:log_error.png"));
        setWindowTitle(QString("Log (%1)").arg(mNumErrors));
        return;
    }

    if (mNumErrors)
        return;

    if (event.type == app::Event::Type::Warning)
    {
        mNumWarnings++;
        setWindowIcon(QIcon("icons:log_warning.png"));
        setWindowTitle(QString("Log (%1)").arg(mNumWarnings));
    }
}

void EventWidget::on_actionClearLog_triggered()
{
    auto& model = app::EventLog::get();
    model.clear();

    mUI.actionClearLog->setEnabled(false);
}


} // namespace
