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
    QFontMetrics metrics(mFont);

    int space = 3 + metrics.horizontalAdvance(QLatin1Char('9')) * digits;

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

    const auto ctrl = key->modifiers() & Qt::ControlModifier;
    const auto alt  = key->modifiers() & Qt::AltModifier;
    const auto code = key->key();

    if (ctrl && code == Qt::Key_F)
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::NextCharacter);
        setTextCursor(cursor);
    }
    else if (ctrl && code == Qt::Key_B)
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::PreviousCharacter);
        setTextCursor(cursor);
    }
    else if (ctrl && code == Qt::Key_N)
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::Down);
        setTextCursor(cursor);
    }
    else if (ctrl && code == Qt::Key_P)
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::Up);
        setTextCursor(cursor);
    }
    else if (ctrl && code == Qt::Key_A)
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::StartOfLine);
        setTextCursor(cursor);
    }
    else if (ctrl && code == Qt::Key_E)
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::EndOfLine);
        setTextCursor(cursor);
    }
    else if (ctrl && code == Qt::Key_K)
    {
        QTextCursor cursor = textCursor();
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::MoveOperation::StartOfLine, QTextCursor::MoveMode::MoveAnchor);
        cursor.movePosition(QTextCursor::MoveOperation::EndOfLine, QTextCursor::MoveMode::KeepAnchor);
        cursor.removeSelectedText();
        cursor.endEditBlock();
    }
    else if (alt && code == Qt::Key_V)
    {
        QTextCursor cursor = textCursor();
        for (int i=0; i<20; ++i)
        {
            cursor.movePosition(QTextCursor::MoveOperation::Up);
            setTextCursor(cursor);
        }
    }
    else if (ctrl && code == Qt::Key_V)
    {
        QTextCursor cursor = textCursor();
        for (int i=0; i<20; ++i)
        {
            cursor.movePosition(QTextCursor::MoveOperation::Down);
            setTextCursor(cursor);
        }
        cursor.movePosition(QTextCursor::MoveOperation::NextBlock);
        setTextCursor(cursor);

    } else  QPlainTextEdit::keyPressEvent(key);
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
    painter.setPen(Qt::black);
    painter.setFont(mFont);

    QFontMetrics metrics(mFont);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= rect.bottom())
    {
        if (block.isVisible() && bottom >= rect.top())
        {
            QString number = QString::number(blockNumber + 1);
            painter.drawText(0, top, mLineNumberArea->width(), metrics.height(),Qt::AlignRight, number);
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
    const auto font_name = mFontName.value_or(mSettings.font_description);
    const auto font_size = mFontSize.value_or(mSettings.font_size);
    if (font.fromString(font_name))
    {
        font.setPointSize(font_size);
        mDocument->setDefaultFont(font);
        mFont = font;
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
