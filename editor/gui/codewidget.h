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
    };

} // namespace