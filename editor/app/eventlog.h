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
    protected:
        bool filterAcceptsRow(int row, const QModelIndex& parent) const override
        {
            const auto& event = mLog->getEvent(row);
            if (event.type == Event::Type::Info && mBits.test(Show::Info))
                return true;
            else if (event.type == Event::Type::Note && mBits.test(Show::Note))
                return true;
            else if (event.type == Event::Type::Warning && mBits.test(Show::Warning))
                return true;
            else if (event.type == Event::Type::Error && mBits.test(Show::Error))
                return true;
            else if (event.type == Event::Type::Debug && mBits.test(Show::Debug))
                return true;
            return false;
        }
    private:
        base::bitflag<Show> mBits;
        const EventLog* mLog = nullptr;
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
