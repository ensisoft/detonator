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
#  include <QPlainTextEdit>
#include "warnpop.h"

class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;
class QTextDocument;
class QSyntaxHighlighter;

namespace gui
{
    // simple text editor widget for simplistic editing functionality.
    // intended for out of the box support for Lua scripts and GLSL.
    class TextEditor : public QPlainTextEdit
    {
        Q_OBJECT

    public:
        struct Settings {
            QString theme = "Monokai";
            QString font_description;
            bool show_line_numbers = true;
            bool highlight_syntax = true;
            bool highlight_current_line = true;
            bool insert_spaces = true;
            unsigned tab_spaces = 4;
            unsigned font_size  = 10;
        };

        TextEditor(QWidget *parent = nullptr);
       ~TextEditor();

        bool CanCopy() const
        { return mCanCopy; }
        bool CanUndo() const
        { return mCanUndo;}

        void SetDocument(QTextDocument* document);

        void PaintLineNumbers(const QRect& rect);

        int ComputeLineNumberAreaWidth() const;

        static void SetDefaultSettings(const Settings& settings);
        static void GetDefaultSettings(Settings* settings);

    protected:
        virtual void resizeEvent(QResizeEvent *event) override;
        virtual void keyPressEvent(QKeyEvent* key) override;

    private slots:
        void UpdateLineNumberAreaWidth(int newBlockCount);
        void UpdateLineNumberArea(const QRect &rect, int dy);
        void HighlightCurrentLine();
        void CopyAvailable(bool yes_no);
        void UndoAvailable(bool yes_no);
    private:
        void ApplySettings();

    private:
        QWidget* mLineNumberArea = nullptr;
        QSyntaxHighlighter* mHighlighter = nullptr;
        QTextDocument* mDocument = nullptr;
        static Settings mSettings;
        bool mCanCopy = false;
        bool mCanUndo = false;
        QFont mFont;
    };

} // namespace