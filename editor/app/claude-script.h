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
#include <QIODevice>
#include "warnpop.h"

#include <variant>
#include <vector>
#include <algorithm>

#include "base/assert.h"
#include "editor/app/types.h"

namespace app
{
    // ClaudeScript parses a simple text file into a sequence of
    // editor actions that can be dispatched to automate editor
    // operations. Each line in the script file contains one action.
    // Blank lines and lines beginning with '#' are ignored.
    //
    // Supported actions:
    //   import-resource <file>   -- import a resource from a file
    //   edit-resource <id>       -- open a resource for editing
    //   focus-resource <id>      -- focus an already open resource
    //   close-resource <id>      -- close an open resource editor
    //   play                     -- start game play
    //   pause                    -- pause game play
    //   stop                     -- stop game play
    //   delay <ms>               -- wait for the given number of milliseconds
    //   take-screenshot <id> <file>  -- take a screenshot of the resource and save to file
    //   list-resources <file>    -- export a JSON list of all resources (id, name, type) to file
    class ClaudeScript
    {
    public:
        struct ImportResource {
            AnyString file;
        };
        struct EditResource {
            AnyString id;
        };
        struct FocusResource {
            AnyString id;
        };
        struct CloseResource {
            AnyString id;
        };
        struct CloseAllResources {};
        struct Play {};
        struct Pause {};
        struct Stop {};
        struct Delay {
            unsigned ms = 0;
        };
        struct TakeScreenshot {
            AnyString id;
            AnyString file;
        };
        struct ExportResource {
            AnyString id;
            AnyString file;
        };
        struct ZoomIn  { unsigned steps = 1; };
        struct ZoomOut { unsigned steps = 1; };
        struct SaveWorkspace {};
        struct ListResources {
            AnyString file;
        };

        using Command = std::variant<
            ImportResource,
            EditResource,
            FocusResource,
            CloseResource,
            CloseAllResources,
            Play,
            Pause,
            Stop,
            Delay,
            TakeScreenshot,
            ExportResource,
            ZoomIn,
            ZoomOut,
            SaveWorkspace,
            ListResources>;

        // Parse the script file at the given path. Returns true if
        // the file was read and parsed without errors. On error the
        // details are written to the application error log.
        // Any previously loaded actions are discarded on each call.
        bool ReadScript(const QString& file);

        // Returns the number of actions parsed from the script.
        std::size_t GetCommandCount() const noexcept
        { return mCommands.size(); }

        // Returns the action at the given index.
        const Command& GetCommand(std::size_t index) const noexcept
        { return base::SafeIndex(mCommands, index); }

        const AnyString& GetFileName() const noexcept
        { return mFileName; }

        static const char* GetCommandName(const Command& cmd) noexcept;
    private:
        AnyString mFileName;
        std::vector<Command> mCommands;
    };

    class ClaudeScriptRunner
    {
    public:
        enum class CommandStatus {
            None, Success, Warning, Error
        };
        struct Command {
            CommandStatus status = CommandStatus::None;
            ClaudeScript::Command command;
            AnyString message;
        };

        explicit ClaudeScriptRunner(const ClaudeScript& script)
        {
            mScriptFile = script.GetFileName();
            for (size_t i=0; i<script.GetCommandCount(); ++i)
            {
                Command cmd;
                cmd.status  = CommandStatus::None;
                cmd.command = script.GetCommand(i);
                mCommands.push_back(std::move(cmd));
            }
        }

        // step the script forward by dt delta seconds and consume things
        // such as time delays etc.
        void Update(float dt)
        {
            if (mDelay > 0.0)
                mDelay -= dt;

            mDelay = std::max(0.0, mDelay);
            mRuntime += dt;
        }

        bool IsDone() const noexcept
        {
            return mCommandIndex == mCommands.size();
        }

        auto GetExecutionTime() const noexcept
        {
            return mRuntime;
        }

        const AnyString& GetFileName() const noexcept
        {
            return mScriptFile;
        }

        // return next command or nullptr if there's no next command.
        // we have a next command unless we're at the end of commands
        // or the next command is gated by a delay that needs to be
        // consumed.
        Command* GetNextCommand() noexcept
        {
            if (mCommandIndex == mCommands.size())
                return nullptr;

            if (mDelay > 0.0f)
                return nullptr;

            ASSERT(mCommandIndex < mCommands.size());
            auto& cmd = mCommands[mCommandIndex++];

            // if its delay we stop here and start delaying execution
            if (const auto* delay = std::get_if<ClaudeScript::Delay>(&cmd.command))
            {
                mDelay += (delay->ms / 1000.0f);
                cmd.status = CommandStatus::Success;
                return nullptr;
            }
            return &cmd;
        }

        size_t GetCommandCount() const noexcept
        { return mCommands.size(); }

        const Command& GetCommand(size_t index) const
        {
            return base::SafeIndex(mCommands, index);
        }

        void WriteResult(QIODevice& device) const;

    private:
        double mRuntime = 0.0;
        double mDelay = 0.0;
        std::size_t mCommandIndex = 0;
        std::vector<Command> mCommands;
        AnyString mScriptFile;
    };

} // namespace app
