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

#define LOGTAG "app"

#include "config.h"

#include "warnpush.h"
#  include <QFile>
#  include <QTextStream>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/app/claude-script.h"

namespace app
{

bool ClaudeScript::ReadScript(const QString& file)
{
    mCommands.clear();
    mFileName = file;

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        ERROR("Failed to open claude-script. [file='%1', error='%2']", file, f.errorString());
        return false;
    }

    QTextStream stream(&f);
    int line_num = 0;

    while (!stream.atEnd())
    {
        const QString line = stream.readLine().trimmed();
        ++line_num;

        if (line.isEmpty() || line.startsWith('#'))
            continue;

        const int space = line.indexOf(' ');
        const QString verb = (space == -1) ? line : line.left(space);
        const QString rest = (space == -1) ? QString() : line.mid(space + 1).trimmed();

        if (verb == "import-resource")
        {
            if (rest.isEmpty())
            {
                ERROR("claude-script 'import-resource' requires a file argument. [line=%1]", line_num);
                return false;
            }
            mCommands.push_back(ImportResource { rest });
        }
        else if (verb == "edit-resource")
        {
            if (rest.isEmpty())
            {
                ERROR("claude-script 'edit-resource' requires an ID argument. [line=%1]", line_num);
                return false;
            }
            mCommands.push_back(EditResource { rest });
        }
        else if (verb == "focus-resource")
        {
            if (rest.isEmpty())
            {
                ERROR("claude-script 'focus-resource' requires an ID argument. [line=%1]", line_num);
                return false;
            }
            mCommands.push_back(FocusResource { rest });
        }
        else if (verb == "close-resource")
        {
            if (rest.isEmpty())
            {
                ERROR("claude-script 'close-resource' requires an ID argument. [line=%1]", line_num);
                return false;
            }
            mCommands.push_back(CloseResource { rest });
        }
        else if (verb == "close-all-resources")
        {
            mCommands.push_back(CloseAllResources{});
        }
        else if (verb == "play")
        {
            mCommands.push_back(Play {});
        }
        else if (verb == "pause")
        {
            mCommands.push_back(Pause {});
        }
        else if (verb == "stop")
        {
            mCommands.push_back(Stop {});
        }
        else if (verb == "delay")
        {
            if (rest.isEmpty())
            {
                ERROR("claude-script 'delay' requires a millisecond argument. [line=%1]", line_num);
                return false;
            }
            bool ok = false;
            const unsigned ms = rest.toUInt(&ok);
            if (!ok)
            {
                ERROR("claude-script 'delay' argument is not a valid unsigned integer. [line=%1, value='%2']", line_num, rest);
                return false;
            }
            mCommands.push_back(Delay { ms });
        }
        else if (verb == "take-screenshot")
        {
            const int sep = rest.indexOf(' ');
            if (sep == -1)
            {
                ERROR("claude-script 'take-screenshot' requires an ID and a file argument. [line=%1]", line_num);
                return false;
            }
            const QString id   = rest.left(sep).trimmed();
            const QString file = rest.mid(sep + 1).trimmed();
            if (id.isEmpty() || file.isEmpty())
            {
                ERROR("claude-script 'take-screenshot' requires an ID and a file argument. [line=%1]", line_num);
                return false;
            }
            mCommands.push_back(TakeScreenshot { id, file });
        }
        else if (verb == "export-resource")
        {
            const int sep = rest.indexOf(' ');
            if (sep == -1)
            {
                ERROR("claude-script 'export-resource' requires an ID and a file argument. [line=%1]", line_num);
                return false;
            }
            const QString id   = rest.left(sep).trimmed();
            const QString file = rest.mid(sep + 1).trimmed();
            if (id.isEmpty() || file.isEmpty())
            {
                ERROR("claude-script 'export-resource' requires an ID and a file argument. [line=%1]", line_num);
                return false;
            }
            mCommands.push_back(ExportResource { id, file });
        }
        else if (verb == "save-workspace")
        {
            mCommands.push_back(SaveWorkspace {});
        }
        else if (verb == "list-resources")
        {
            if (rest.isEmpty())
            {
                ERROR("claude-script 'list-resources' requires a file argument. [line=%1]", line_num);
                return false;
            }
            mCommands.push_back(ListResources { rest });
        }
        else if (verb == "zoom-in")
        {
            unsigned steps = 1;
            if (!rest.isEmpty())
                steps = rest.toUInt();
            mCommands.push_back(ZoomIn { steps ? steps : 1 });
        }
        else if (verb == "zoom-out")
        {
            unsigned steps = 1;
            if (!rest.isEmpty())
                steps = rest.toUInt();
            mCommands.push_back(ZoomOut { steps ? steps : 1 });
        }
        else
        {
            WARN("claude-script unknown action, skipping. [line=%1, action='%2']", line_num, verb);
        }
    }

    return true;
}

// static
const char* ClaudeScript::GetCommandName(const Command& cmd) noexcept
{
    if (std::get_if<ImportResource>(&cmd))        return "import-resource";
    else if (std::get_if<EditResource>(&cmd))     return "edit-resource";
    else if (std::get_if<FocusResource>(&cmd))    return "focus-resource";
    else if (std::get_if<CloseResource>(&cmd))    return "close-resource";
    else if (std::get_if<CloseAllResources>(&cmd))return "close-all-resources";
    else if (std::get_if<Play>(&cmd))             return "play";
    else if (std::get_if<Pause>(&cmd))            return "pause";
    else if (std::get_if<Stop>(&cmd))             return "stop";
    else if (std::get_if<Delay>(&cmd))            return "delay";
    else if (std::get_if<TakeScreenshot>(&cmd))   return "take-screenshot";
    else if (std::get_if<ExportResource>(&cmd))   return "export-resource";
    else if (std::get_if<ZoomIn>(&cmd))           return "zoom-in";
    else if (std::get_if<ZoomOut>(&cmd))          return "zoom-out";
    else if (std::get_if<SaveWorkspace>(&cmd))    return "save-workspace";
    else if (std::get_if<ListResources>(&cmd))    return "list-resources";
    else BUG("Unhandled claude script command name.");
}

void ClaudeScriptRunner::WriteResult(QIODevice& device) const
{
    QTextStream out(&device);

    for (const auto& command : mCommands)
    {
        const auto* name = ClaudeScript::GetCommandName(command.command);

        QString status;
        if (command.status == CommandStatus::Success)
            status = "SUCCESS";
        else if (command.status == CommandStatus::Warning)
            status = "WARNING";
        else if (command.status == CommandStatus::Error)
            status = "ERROR";
        else BUG("Unexpected command status");

        out << name << " -> " << status;
        if (!command.message.IsEmpty())
            out << ": " << command.message;
        out << "\n";
    }
}

} // namespace app
