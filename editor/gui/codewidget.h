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
#  include <QCompleter>
#  include "ui_completer.h"
#include "warnpop.h"

#include <optional>

class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;
class QTextDocument;

namespace app {
    class CodeCompleter;
    class CodeHighlighter;
} // namespace

namespace gui
{
    class CodeCompleter : public QWidget
    {
        Q_OBJECT

    public:
        explicit CodeCompleter(QWidget* parent);

        void Open(const QPoint& pos);

        bool IsOpen() const;

        void Close();

        void SetCompleter(app::CodeCompleter* completer);
    signals:
        void Complete(const QString& text, const QModelIndex& index);
        void Filter(const QString& input);

    private slots:
        void on_lineEdit_textChanged(const QString& text);
        void TableSelectionChanged(const QItemSelection&, const QItemSelection&);
    private:
        bool eventFilter(QObject* destination, QEvent* event);
        void UpdateHelp();
    private:
        Ui::Completer mUI;
        bool mOpen = false;
    private:
        app::CodeCompleter* mCompleter;
    };

    // simple text editor widget for simplistic editing functionality.
    // intended for out of the box support for Lua scripts and GLSL.
    class TextEditor : public QPlainTextEdit
    {
        Q_OBJECT

    public:
        struct Settings {
            QString font_description;
            bool show_line_numbers        = true;
            bool highlight_syntax         = true;
            bool highlight_current_line   = true;
            bool replace_tabs_with_spaces = true;
            bool use_code_completer       = true;
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
        void SetCompleter(app::CodeCompleter* completer);

        void PaintLineNumbers(const QRect& rect);

        int ComputeLineNumberAreaWidth() const;

        void SetSettings(const Settings& settings)
        { mSettings = settings; }
        void SetFontName(const QString& font)
        { mFontName = font; }
        void ResetFontName()
        { mFontName.reset(); }
        void SetFontSize(int size)
        { mFontSize = size; }
        void ResetFontSize()
        { mFontSize.reset(); }

        void ApplySettings();

        bool CancelCompletion();

        QString GetCurrentWord() const;


        // find the cursor position in the document while ignoring
        // whitespace, (tabs, spaces, newline)
        size_t GetCursorPositionHashIgnoreWS() const;

        void SetCursorPositionFromHashIgnoreWS(size_t cursor_hash);

    protected:
        virtual void resizeEvent(QResizeEvent *event) override;
        virtual void keyPressEvent(QKeyEvent* key) override;

    private slots:
        void UpdateLineNumberAreaWidth(int newBlockCount);
        void UpdateLineNumberArea(const QRect &rect, int dy);
        void HighlightCurrentLine();
        void CopyAvailable(bool yes_no);
        void UndoAvailable(bool yes_no);
        void Complete(const QString& text, const QModelIndex& index);
        void Filter(const QString& input);

    private:
        Settings mSettings;
        std::unique_ptr<CodeCompleter> mCompleterUI;
        app::CodeCompleter* mCompleter = nullptr;
        QWidget* mLineNumberArea = nullptr;
        QTextDocument* mDocument = nullptr;
        QFont mFont;
        std::optional<QString> mFontName;
        std::optional<int> mFontSize;
        bool mCanCopy = false;
        bool mCanUndo = false;
    };

} // namespace
