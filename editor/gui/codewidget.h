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

#include "editor/app/lua-doc.h"

class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;
class QTextDocument;
class QSyntaxHighlighter;

namespace app {
    class LuaParser;
    class LuaTheme;
} // namespace

namespace gui
{
    class CodeCompleter : public QWidget
    {
        Q_OBJECT

    public:
        explicit CodeCompleter(QWidget* parent);

        void Open(const QRect& rect);

        bool IsOpen() const;

        void Close();

        void SetModel(app::LuaDocModelProxy* model);
    signals:
        void Complete(const QString& text, const QModelIndex& index);

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
        app::LuaDocModelProxy* mModel = nullptr;
    };

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

        void ApplySettings();
        void SetFontName(QString font)
        { mFontName = font; }
        void ResetFontName()
        { mFontName.reset(); }
        void SetFontSize(int size)
        { mFontSize = size; }
        void ResetFontSize()
        { mFontSize.reset(); }
        void Reparse();

        bool CancelCompletion();

        QString GetCurrentWord() const;

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
        void Complete(const QString& text, const QModelIndex& index);
    private:
        static Settings mSettings;
    private:
        app::LuaParser* mHighlighter = nullptr;
        app::LuaDocTableModel mDocModel;
        app::LuaDocModelProxy mDocProxy;
        std::unique_ptr<CodeCompleter> mCompleter;
        QWidget* mLineNumberArea = nullptr;
        QTextDocument* mDocument = nullptr;
        QFont mFont;
        std::optional<QString> mFontName;
        std::optional<int> mFontSize;
        bool mCanCopy = false;
        bool mCanUndo = false;
    };

} // namespace
