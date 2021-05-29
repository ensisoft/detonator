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

#pragma once

#include "config.h"

// note that wingdi.h also has ERROR macro. On Windows Qt headers incluce
// windows.h which includes wingdi.h which redefines the ERROR macro which
// causes macro collision. This is a hack to already include wingdi.h 
// (has to be done through windows.h) and then undefine ERROR which allows
// the code below to hijack the macro name. The problem obviously is that
// any code that would then try to use WinGDI with the ERROR macro would 
// produce garbage errors. 
#ifdef _WIN32
#  include <windows.h>
#  undef ERROR
#endif

#include "warnpush.h"
#  include <boost/circular_buffer.hpp>
#  include <QAbstractListModel>
#  include <QSortFilterProxyModel>
#  include <QEvent>
#  include <QTime>
#  include <QString>
#include "warnpop.h"

#include <functional>

#include "base/logging.h"
#include "base/bitflag.h"
#include "editor/app/format.h"
#include "editor/app/event.h"

// Careful with the macros here. base/logging.h has macros by the same name.
// we're going to hijack the names here and change the definition to better
// suit the application.
#undef WARN
#undef ERROR
#undef INFO
#undef DEBUG

namespace app
{
    // Application event log. events that occur on the background
    // are logged in here for later inspection.
    class EventLog : public QAbstractListModel
    {
        Q_OBJECT

    public:
        static EventLog& get();

        EventLog();
       ~EventLog();

        // record a new event in the log
        void write(Event::Type type, const QString& msg, const QString& tag);

        // clear the event log
        void clear();

        // abstraclistmodel data accessor
        virtual int rowCount(const QModelIndex&) const override;

        // abstractlistmodel data accessor
        virtual QVariant data(const QModelIndex& index, int role) const override;

        std::size_t numEvents() const
        { return mEvents.size(); }

        using NewEventCallback = std::function<void(const app::Event&)>;
        NewEventCallback OnNewEvent;

        bool isEmpty() const
        { return mEvents.empty(); }

        const Event& getEvent(size_t index) const
        {
            return mEvents[index];
        }

        void setShowTime(bool on_off)
        { mIncludeTime = on_off; }
        void setShowTag(bool on_off)
        { mIncludeTag = on_off; }

    signals:
        void newEvent(const app::Event& event);

    private:
        boost::circular_buffer<Event> mEvents;
        QString mLogTag;
        bool mIncludeTime = true;
        bool mIncludeTag  = true;
    };

    // Filtering model proxy for event log
    class EventLogProxy : public QSortFilterProxyModel
    {
    Q_OBJECT
    public:
        enum class Show {
            Info, Note, Warning, Error, Debug
        };
        EventLogProxy()
        {
            mBits.set(Show::Info, true);
            mBits.set(Show::Note, true);
            mBits.set(Show::Warning, true);
            mBits.set(Show::Error, true);
            mBits.set(Show::Debug, true);
        }

        void SetModel(const EventLog* log)
        { mLog = log; }
        void SetVisible(Show what, bool yes_no)
        { mBits.set(what, yes_no); }
        bool IsShown(Show what) const
        { return mBits.test(what); }
        unsigned GetShowBits() const
        { return mBits.value(); }
        void SetShowBits(unsigned value)
        { mBits.set_from_value(value); }
        void SetFilterStr(QString str, bool case_sensitive)
        {
            mFilterStr = str;
            mFilterCaseSensitive = case_sensitive;
        }
    protected:
        bool filterAcceptsRow(int row, const QModelIndex& parent) const override
        {
            const auto& event = mLog->getEvent(row);
            if (event.type == Event::Type::Info && !mBits.test(Show::Info))
                return false;
            else if (event.type == Event::Type::Note && !mBits.test(Show::Note))
                return false;
            else if (event.type == Event::Type::Warning && !mBits.test(Show::Warning))
                return false;
            else if (event.type == Event::Type::Error && !mBits.test(Show::Error))
                return false;
            else if (event.type == Event::Type::Debug && !mBits.test(Show::Debug))
                return false;

            if (mFilterStr.isEmpty())
                return true;
            return event.message.contains(mFilterStr,
                                          mFilterCaseSensitive
                                          ? Qt::CaseSensitive
                                          : Qt::CaseInsensitive);
        }
    private:
        base::bitflag<Show> mBits;
        const EventLog* mLog = nullptr;
        QString mFilterStr;
        bool mFilterCaseSensitive = true;
    };

} // namespace

// we want every log event to be traceable back to where it came from
// so thus every module should define it's own LOGTAG
#ifndef LOGTAG
#  if !defined(Q_MOC_OUTPUT_REVISION)
#    warning every module importing eventlog needs to define LOGTAG
#  endif
#endif

#undef WARN
#define WARN(msg, ...) \
    app::EventLog::get().write(app::Event::Type::Warning, app::toString(msg, ## __VA_ARGS__), LOGTAG)

#undef ERROR
#define ERROR(msg, ...) \
    app::EventLog::get().write(app::Event::Type::Error, app::toString(msg, ## __VA_ARGS__), LOGTAG)

#undef INFO
#define INFO(msg, ...) \
    app::EventLog::get().write(app::Event::Type::Info, app::toString(msg, ## __VA_ARGS__), LOGTAG)

#undef NOTE
#define NOTE(msg, ...) \
    app::EventLog::get().write(app::Event::Type::Note, app::toString(msg, ## __VA_ARGS__), LOGTAG)

#undef DEBUG
#define DEBUG(msg, ...) \
    base::WriteLog(base::LogEvent::Debug, __FILE__, __LINE__, app::ToUtf8(app::toString(msg, ## __VA_ARGS__)))
