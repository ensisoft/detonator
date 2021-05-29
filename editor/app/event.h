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
            // Debugging message.
            Debug,
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