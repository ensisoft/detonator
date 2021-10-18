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

#include "config.h"

#include "warnpush.h"
#  include <QIcon>
#  include <QFontMetrics>
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
    Event event;
    event.type = type;
    event.message = msg;
    event.logtag = tag;
    event.time = QTime::currentTime();
    emit newEvent(event);

    if (OnNewEvent)
        OnNewEvent(event);

    if (type == Event::Type::Note)
        return;

    if (mEvents.full())
    {
        const auto first = index(0, 0);
        const auto last  = index((int)mEvents.size()-1, 0);
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

    QString text;
    if (mIncludeTime && mIncludeTag)
    {
        text = QString("[%1] [%2] %3")
            .arg(ev.time.toString("hh:mm:ss:zzz"))
            .arg(ev.logtag)
            .arg(ev.message);
    }
    else if (mIncludeTime)
    {
        text = QString("[%1] %2")
                .arg(ev.time.toString("hh:mm:ss:zzz"))
                .arg(ev.message);
    }
    else if (mIncludeTag)
    {
        text = QString("[%1] %2").arg(ev.logtag).arg(ev.message);
    }
    else
    {
        text = ev.message;
    }
    if (role == Qt::DisplayRole)
        return text;
    else if (role == Qt::SizeHintRole)
    {
        // The QListView in mainwindow doesn't display the horizontal
        // scroll bar properly without the size hint being implemented.
        // Howoever the question here is.. how to figure out the horizontal
        // size we should return ??
        // grabbing some default font and coming up with text with
        // through font metrics.
        QFont dunno_about_this_font;
        QFontMetrics fm(dunno_about_this_font);
        const auto text_width = fm.horizontalAdvance(text);
        return QSize(text_width, 16);
    }
    else if (role == Qt::DecorationRole)
    {
        using type = Event::Type;
        switch (ev.type)
        {
            case type::Warning: return QIcon("icons:log_warning.png");
            case type::Info:    return QIcon("icons:log_info.png");
            case type::Error:   return QIcon("icons:log_error.png");
            case type::Note:    return QIcon("icons:log_note.png");
            case type::Debug:   return QIcon("icons:log_debug.png");
        }
    }
    return {};
}

EventLog& EventLog::get()
{
    static EventLog log;
    return log;
}

} // app
