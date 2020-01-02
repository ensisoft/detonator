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

#include "config.h"

#include "warnpush.h"
#  include <QIcon>
#include "warnpop.h"

#include "base/logging.h"
#include "eventlog.h"

namespace app
{

EventLog::EventLog() : mEvents(1000)
{}

EventLog::~EventLog()
{}

void EventLog::write(Event::Type type, const QString& msg, const QString& tag)
{
    const auto time  = QTime::currentTime();
    const auto event = Event {type, msg, tag, time };

    emit newEvent(event);
    if (type == Event::Type::Note)
        return;

    if (mEvents.full())
    {
        const auto first = index(0, 0);
        const auto last  = index((int)mEvents.size(), 1);
        mEvents.push_front(event);
        emit dataChanged(first, last);
    }
    else
    {
        beginInsertRows(QModelIndex(), 0, 0);
        mEvents.push_front(event);
        endInsertRows();
    }
}

void EventLog::clear()
{
    beginRemoveRows(QModelIndex(), 0, mEvents.size());
    mEvents.clear();
    endRemoveRows();
}

int EventLog::rowCount(const QModelIndex&) const
{
    return static_cast<int>(mEvents.size());
}

QVariant EventLog::data(const QModelIndex& index, int role) const
{
    const auto row = index.row();
    const auto& ev = mEvents[row];

    if (role == Qt::SizeHintRole)
        return QSize(0, 16);

    if (role == Qt::DecorationRole)
    {
        using type = Event::Type;
        switch (ev.type)
        {
            case type::Warning: return QIcon("icons:log_warning.png");
            case type::Info:    return QIcon("icons:log_info.png");
            case type::Error:   return QIcon("icons:log_error.png");
            case type::Note:    return QIcon("icons:log_note.png");
        }
    }

    if (role == Qt::DisplayRole)
    {
        return QString("[%1] [%2] %3")
            .arg(ev.time.toString("hh:mm:ss:zzz"))
            .arg(ev.logtag)
            .arg(ev.message);
    }
    return {};
}

EventLog& EventLog::get()
{
    static EventLog log;
    return log;
}

} // app
