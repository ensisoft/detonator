// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include <QString>

#include "warnpop.h"

class QAbstractItemModel;
class QModelIndex;
class QTextDocument;
class QTextCursor;
class QKeyEvent;

namespace app
{
    class CodeCompleter
    {
    public:
        struct ApiHelp {
            QString desc;
            QString args;
            QString name;
        };

        virtual ~CodeCompleter() = default;

        // Start code completion process on the key press given the document
        // and current text document cursor. Returns true if code completion
        // is possible or otherwise false to indicate that there are no completions
        // available at this point.
        virtual bool StartCompletion(const QKeyEvent* key,
                                     const QTextDocument& document,
                                     const QTextCursor& cursor)  = 0;

        // Filter possible completions based on some user input coming from the
        // completion UI system.
        virtual void FilterPossibleCompletions(const QString& input) = 0;
        // Perform the completion on the document at the given cursor with the
        // final user input and/or selected data index. It is possible that the
        // input string is empty or that the index is invalid, or both. If any
        // completion was done and text was added to the document then the cursor
        // can be modified/adjusted to point to a new convenient location and the
        // function will return true. If no changes were made false should be
        // returned and cursor should not be modified.
        virtual bool FinishCompletion(const QString& input,
                                      const QModelIndex& index,
                                      const QTextDocument& document,
                                      QTextCursor& cursor) = 0;

        // Get the human-readable help for the given completion index.
        virtual ApiHelp GetCompletionHelp(const QModelIndex& index) const = 0;

        // Get the table model for displaying the completion data in a table view.
        virtual QAbstractItemModel* GetCompletionModel() = 0;
    private:
    };

    class CodeHighlighter
    {
    public:
        // Apply code highlight on the given document.
        virtual void ApplyHighlight(QTextDocument& document) = 0;
        virtual void RemoveHighlight(QTextDocument& document) = 0;
    private:
    };

} // namespace
