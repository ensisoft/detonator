// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include <string>
#include <vector>
#include <algorithm> // for swap

#include "base/logging.h"
#include "base/format.h"

namespace gfx
{
    class PaintContext
    {
    public:
        using LogEvent = base::LogEvent;

        struct LogMessage {
            LogEvent type = LogEvent::Debug;
            std::string file;
            std::string message;
            unsigned line = 0;
        };
        using MessageList = std::vector<LogMessage>;

        void WriteLogMessage(LogEvent type , const char* file, int line, std::string message)
        {
            if (type == LogEvent::Error)
                ++mErrorCount;
            else if (type == LogEvent::Warning)
                ++mWarningCount;

            LogMessage msg;
            msg.type = type;
            msg.file = file;
            msg.line = line;
            msg.message = std::move(message);
            mMessages.push_back(std::move(msg));
        }

        PaintContext();
       ~PaintContext();
        PaintContext(const PaintContext&) = delete;

        void EndScope() noexcept;

        void ClearMessages() noexcept;

        auto HasErrors() const noexcept
        { return mErrorCount != 0; }
        bool HasWarnings() const noexcept
        { return mWarningCount != 0; }

        auto GetMessages() const
        { return mMessages; }
        void TransferMessages(MessageList* messages)
        {
            std::swap(*messages, mMessages);
            mErrorCount   = 0;
            mWarningCount = 0;
        }

        static PaintContext* GetContext();

        PaintContext& operator=(const PaintContext&) = delete;
    private:
        std::vector<LogMessage> mMessages;
        std::size_t mErrorCount   = 0;
        std::size_t mWarningCount = 0;
        bool mActive = true;
    };

} // namespace

