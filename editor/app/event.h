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
#  include <QMetaType>
#  include <QString>
#  include <QTime>
#include "warnpop.h"

namespace app
{
    // Application log event
    struct Event {
        enum class Type {
            // useful information about an event that occurred.
            Info,
            // like info except that it is transient and isnt logged.
            // usually indicated as message in the application status bar.
            Note,
            // warning means that things might not work quite as expected
            // but the particular processing can continue.
            // for example a downloaded file was damaged or some non-critical
            // file could not be opened.
            Warning,
            // error means that some processing has encountered an unrecoverable
            // problem and probably cannot continue. For example connection was lost
            // critical file could not be read/written or some required
            // resource could not be acquired.
            Error,
        };
        Type type = Type::Info;

        // event log message.
        QString message;

        // logtag identifies the component that generated the event.
        QString logtag;

        // recording time.
        QTime time;
    };

} // namespace

    Q_DECLARE_METATYPE(app::Event);