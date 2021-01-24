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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QPainter>
#  include <QPalette>
#  include <QTextBlock>
#  include <QSyntaxHighlighter>
#  include <QRegularExpression>
#  include <QSourceHighlight/qsourcehighliter.h>
#include "warnpop.h"

#include <set>

#include "editor/app/eventlog.h"
#include "editor/gui/codewidget.h"

namespace gui
{
class LineNumberArea : public QWidget
{
public:
    LineNumberArea(TextEditor *editor) : QWidget(editor), mCodeWidget(editor)
    {}

    virtual QSize sizeHint() const override
    {
        return QSize(mCodeWidget->ComputeLineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        mCodeWidget->PaintLineNumbers(event->rect());
    }
private:
    TextEditor* mCodeWidget = nullptr;
};

// slightly ugly but okay, works for now
std::set<TextEditor*> open_editors;

// static
TextEditor::Settings TextEditor::mSettings;

// static
void TextEditor::SetDefaultSettings(const Settings& settings)
{
    mSettings = settings;
    for (auto* editor : open_editors)
    {
        editor->ApplySettings();
    }
}

// static
void TextEditor::GetDefaultSettings(Settings* settings)
{
    *settings = mSettings;
}

TextEditor::TextEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    connect(this, &TextEditor::blockCountChanged, this, &TextEditor::UpdateLineNumberAreaWidth);
    connect(this, &TextEditor::updateRequest, this, &TextEditor::UpdateLineNumberArea);
    connect(this, &TextEditor::cursorPositionChanged, this, &TextEditor::HighlightCurrentLine);
    connect(this, &TextEditor::copyAvailable, this, &TextEditor::CopyAvailable);
    connect(this, &TextEditor::undoAvailable, this, &TextEditor::UndoAvailable);

    open_editors.insert(this);
}

TextEditor::~TextEditor()
{
    open_editors.erase(this);
}

void TextEditor::SetDocument(QTextDocument* document)
{
    this->setDocument(document);
    mDocument = document;

    ApplySettings();

}

int TextEditor::ComputeLineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10)
    {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return space;
}

void TextEditor::UpdateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(ComputeLineNumberAreaWidth(), 0, 0, 0);
}

void TextEditor::UpdateLineNumberArea(const QRect &rect, int dy)
{
    if (!mLineNumberArea)
        return;

    if (dy)
        mLineNumberArea->scroll(0, dy);
    else
        mLineNumberArea->update(0, rect.y(), mLineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        UpdateLineNumberAreaWidth(0);
}

void TextEditor::CopyAvailable(bool yes_no)
{
    mCanCopy = yes_no;
}

void TextEditor::UndoAvailable(bool yes_no)
{
    mCanUndo = yes_no;
}

void TextEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    if (mLineNumberArea)
        mLineNumberArea->setGeometry(QRect(cr.left(), cr.top(), ComputeLineNumberAreaWidth(), cr.height()));
}

void TextEditor::keyPressEvent(QKeyEvent* key)
{
    if (key->key() == Qt::Key_Tab && mSettings.insert_spaces)
    {
        // convert tab to spaces.
        this->insertPlainText("    ");
        return;
    }
    // leave the emacs type keybindings for now.
    // they'd stomp over the default key handling in the QPlainTextEdit
    // and also collide with the top level keyboard shortcuts.
    /*
    if (key->key() == Qt::Key_N && key->modifiers() & Qt::ControlModifier)
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::Down);
        setTextCursor(cursor);
        return;
    }
    else if (key->key() == Qt::Key_P && key->modifiers() & Qt::ControlModifier)
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::Up);
        setTextCursor(cursor);
        return;
    }
    */
    QPlainTextEdit::keyPressEvent(key);
}

void TextEditor::HighlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    const auto& palette = this->palette();

    if (mSettings.highlight_current_line)
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(palette.color(QPalette::Midlight));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void TextEditor::PaintLineNumbers(const QRect& rect)
{
    QPainter painter(mLineNumberArea);
    painter.fillRect(rect, Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= rect.bottom())
    {
        if (block.isVisible() && bottom >= rect.top())
        {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            //painter.setFont(font());
            painter.drawText(0, top, mLineNumberArea->width(), fontMetrics().height(),Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void TextEditor::ApplySettings()
{
    QFont font;
    if (font.fromString(mSettings.font_description))
    {
        font.setPointSize(mSettings.font_size);
        mDocument->setDefaultFont(font);
    }

    if (mSettings.highlight_syntax && !mHighlighter)
    {
        auto highliter = new QSourceHighlite::QSourceHighliter(mDocument);
        highliter->setCurrentLanguage(QSourceHighlite::QSourceHighliter::CodeLua);
        mHighlighter = highliter;
        mHighlighter->setParent(this);
        mHighlighter->rehighlight();
    }
    else if (!mSettings.highlight_syntax && mHighlighter)
    {
        delete mHighlighter;
        mHighlighter = nullptr;
    }
    if (mSettings.show_line_numbers && !mLineNumberArea)
    {
        mLineNumberArea = new LineNumberArea(this);
        UpdateLineNumberAreaWidth(0);
    }
    else if (!mSettings.show_line_numbers && mLineNumberArea)
    {
        delete mLineNumberArea;
        mLineNumberArea = nullptr;
    }

    HighlightCurrentLine();
}

} // gui
