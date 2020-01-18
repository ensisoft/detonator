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

#include "warnpush.h"
#  include <boost/circular_buffer.hpp>
#  include <QAbstractListModel>
#  include <QEvent>
#  include <QTime>
#  include <QString>
#include "warnpop.h"

#include "base/logging.h"
#include "format.h"
#include "event.h"

namespace app
{
    // Application event log. events that occur on the background
    // are logged in here for later inspection.
    class EventLog : public QAbstractListModel
    {
        Q_OBJECT

        EventLog();
       ~EventLog();

    public:
        static EventLog& get();

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
    signals:
        void newEvent(const app::Event& event);

    private:
        boost::circular_buffer<Event> mEvents;
    };

// we want every log event to be tracable back to where it came from
// so thus every module should define it's own LOGTAG
#ifndef LOGTAG
#  if !defined(Q_MOC_OUTPUT_REVISION)
#    warning every module importing eventlog needs to define LOGTAG
#  endif
#endif

// Careful with the macros here. base/logging.h has macros by the same name.
// we're going to hijack the names here and change the definition to better
// suit the application.

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

} // namespace