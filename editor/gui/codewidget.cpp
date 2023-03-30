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
#  include <QRegularExpression>
#include "warnpop.h"

#include <set>

#include "base/math.h"
#include "editor/app/code-tools.h"
#include "editor/app/eventlog.h"
#include "editor/gui/codewidget.h"
#include "editor/gui/utility.h"

namespace gui
{

CodeCompleter::CodeCompleter(QWidget* parent)
  : QWidget(parent, Qt::Popup)
{
    mUI.setupUi(this);
    mUI.lineEdit->installEventFilter(this);
}

void CodeCompleter::Open(const QPoint& point)
{
    move(point);
    show();
    mUI.lineEdit->setFocus();

    SetValue(mUI.lineEdit, QString(""));
    SelectRow(mUI.tableView, -1);
    UpdateHelp();

    mOpen = true;
}

bool CodeCompleter::IsOpen() const
{
    return mOpen;
}

void CodeCompleter::Close()
{
    close();
    mOpen = false;
}

void CodeCompleter::SetCompleter(app::CodeCompleter* completer)
{
    if (mCompleter)
    {
        mUI.tableView->setModel(nullptr);
        mUI.tableView->disconnect(this);
    }
    mCompleter = completer;

    if (mCompleter)
    {
        mUI.tableView->setModel(completer->GetCompletionModel());
        mUI.tableView->setColumnWidth(0, 150);
        mUI.tableView->setColumnWidth(1, 220);
        Connect(mUI.tableView, this, &CodeCompleter::TableSelectionChanged);
    }

}

void CodeCompleter::on_lineEdit_textChanged(const QString& text)
{
    mCompleter->FilterPossibleCompletions(text);

    SelectRow(mUI.tableView, 0);
    UpdateHelp();
}

void CodeCompleter::TableSelectionChanged(const QItemSelection&, const QItemSelection&)
{
    UpdateHelp();
}

bool CodeCompleter::eventFilter(QObject* destination, QEvent* event)
{
    if (destination != mUI.lineEdit)
        return false;
    else if (event->type() != QEvent::KeyPress)
        return false;

    const auto* key  = static_cast<QKeyEvent*>(event);
    const bool ctrl  = key->modifiers() & Qt::ControlModifier;
    const bool shift = key->modifiers() & Qt::ShiftModifier;

    if (ctrl && (key->key() == Qt::Key_G ))
    {
        Close();
        return true;
    }

    if (key->key() == Qt::Key_Return)
    {
        emit Complete(GetValue(mUI.lineEdit),
                      GetSelectedIndex(mUI.tableView));
        Close();
        return true;
    }

    auto* model = mUI.tableView->model();
    if (model->rowCount() == 0)
        return false;

    int current = GetSelectedRow(mUI.tableView);

    const auto max = GetCount(mUI.tableView);

    if (ctrl && key->key() == Qt::Key_N)
        current = math::wrap(0, max-1, current+1);
    else if (ctrl && key->key() == Qt::Key_P)
        current = math::wrap(0, max-1, current-1);
    else if (key->key() == Qt::Key_Up)
        current = math::wrap(0, max-1, current-1);
    else if (key->key() == Qt::Key_Down)
        current = math::wrap(0, max-1, current+1);
    else return false;

    SelectRow(mUI.tableView, current);
    UpdateHelp();
    return true;
}

void CodeCompleter::UpdateHelp()
{
    SetValue(mUI.help, QString(""));
    SetValue(mUI.args, QString(""));
    const auto index = GetSelectedIndex(mUI.tableView);
    if (!index.isValid())
        return;

    const auto& help = mCompleter->GetCompletionHelp(index);
    SetValue(mUI.help, help.desc);
    SetValue(mUI.args, help.args);
}

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

void TextEditor::Reparse()
{
    if (mHighlighter)
    {
        mHighlighter->ApplyHighlight(*mDocument);
    }
}

bool TextEditor::CancelCompletion()
{
    if (mCompleterUI->IsOpen())
    {
        mCompleterUI->Close();
        return true;
    }
    return false;
}

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

TextEditor::TextEditor(QWidget *parent)
  : QPlainTextEdit(parent)
{
    connect(this, &TextEditor::blockCountChanged,     this, &TextEditor::UpdateLineNumberAreaWidth);
    connect(this, &TextEditor::updateRequest,         this, &TextEditor::UpdateLineNumberArea);
    connect(this, &TextEditor::cursorPositionChanged, this, &TextEditor::HighlightCurrentLine);
    connect(this, &TextEditor::copyAvailable,         this, &TextEditor::CopyAvailable);
    connect(this, &TextEditor::undoAvailable,         this, &TextEditor::UndoAvailable);

    open_editors.insert(this);

    setLineWrapMode(LineWrapMode::NoWrap);

    mCompleterUI.reset(new CodeCompleter(this));
    connect(mCompleterUI.get(), &CodeCompleter::Complete, this, &TextEditor::Complete);
    connect(mCompleterUI.get(), &CodeCompleter::Filter,   this, &TextEditor::Filter);
}

TextEditor::~TextEditor()
{
    open_editors.erase(this);

    mCompleterUI->disconnect(this);
    if (mCompleterUI->IsOpen())
        mCompleterUI->Close();
    mCompleterUI.reset();
    
}

void TextEditor::SetDocument(QTextDocument* document)
{
    this->setDocument(document);
    mDocument = document;

    ApplySettings();
}
void TextEditor::SetCompleter(app::CodeCompleter* completer)
{
    mCompleter = completer;
    mCompleterUI->SetCompleter(completer);
}
void TextEditor::SetSyntaxHighlighter(app::CodeHighlighter* highlighter)
{
    mHighlighter = highlighter;
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

void TextEditor::Complete(const QString& text, const QModelIndex& index)
{
    QTextCursor cursor = textCursor();
    if (mCompleter && mCompleter->FinishCompletion(text, index, *mDocument, cursor))
    {
        setTextCursor(cursor);
    }
}

void TextEditor::Filter(const QString& input)
{
    mCompleter->FilterPossibleCompletions(input);
}

void TextEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    if (mLineNumberArea)
        mLineNumberArea->setGeometry(QRect(cr.left(), cr.top(), ComputeLineNumberAreaWidth(), cr.height()));
}

void TextEditor::keyPressEvent(QKeyEvent* event)
{
    const auto ctrl = event->modifiers() & Qt::ControlModifier;
    const auto alt  = event->modifiers() & Qt::AltModifier;
    const auto key  = event->key();

    if (key == Qt::Key_Tab && mSettings.replace_tabs_with_spaces)
    {
        // convert tab to spaces.
        this->insertPlainText("    ");
        return;
    }

    if (mCompleter && mSettings.use_code_completer)
    {
        if (mCompleter->StartCompletion(event, *mDocument, textCursor()))
        {
            const auto& rect  = cursorRect();
            const auto& point = mapToGlobal(rect.bottomRight());
            mCompleterUI->Open(point);

            // propagate to the parent class so the character gets inserted
            // into the text.
            QPlainTextEdit::keyPressEvent(event);
            return;
        }
    }

    if (ctrl && (key == Qt::Key_F))
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::NextCharacter);
        setTextCursor(cursor);
    }
    else if (ctrl && (key == Qt::Key_B))
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::PreviousCharacter);
        setTextCursor(cursor);
    }
    else if (ctrl && (key == Qt::Key_N))
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::Down);
        setTextCursor(cursor);
    }
    else if (ctrl && (key == Qt::Key_P))
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::Up);
        setTextCursor(cursor);
    }
    else if (ctrl && (key == Qt::Key_A))
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::StartOfLine);
        setTextCursor(cursor);
    }
    else if (ctrl && (key == Qt::Key_E))
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::MoveOperation::EndOfLine);
        setTextCursor(cursor);
    }
    else if (ctrl && (key == Qt::Key_K))
    {
        // kill semantics.
        // - if we kill from the middle of the line, the line is killed from cursor until the end
        // - if we're at the end of the line then we just kill the newline char
        QTextCursor cursor = textCursor();
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::MoveOperation::EndOfLine, QTextCursor::MoveMode::KeepAnchor);
        if (cursor.anchor() == cursor.position())
        {
            const auto pos = cursor.position();
            const auto& txt = this->toPlainText();
            if (pos < txt.size() && txt[pos] == '\n')
            {
                cursor.deleteChar();
            }
        }
        else cursor.removeSelectedText();
        cursor.endEditBlock();
    }
    else if (alt && (key == Qt::Key_V))
    {
        QTextCursor cursor = textCursor();
        for (int i=0; i<20; ++i)
        {
            cursor.movePosition(QTextCursor::MoveOperation::Up);
            setTextCursor(cursor);
        }
    }
    else if (ctrl && (key == Qt::Key_V))
    {
        QTextCursor cursor = textCursor();
        for (int i=0; i<20; ++i)
        {
            cursor.movePosition(QTextCursor::MoveOperation::Down);
            setTextCursor(cursor);
        }
        cursor.movePosition(QTextCursor::MoveOperation::NextBlock);
        setTextCursor(cursor);

    }
    else
    {
        if (mHighlighter && mSettings.highlight_syntax && (key == Qt::Key_Return))
            mHighlighter->ApplyHighlight(*mDocument);

        QPlainTextEdit::keyPressEvent(event);
    }
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
        DEBUG("Apply text editor font setting. [font='%1', size=%2]", font_name, font_size);
    }
    else WARN("Text editor font description is invalid. [font='%1']", font_name);

    if (mSettings.highlight_syntax && mHighlighter)
    {
        mHighlighter->ApplyHighlight(*mDocument);
    }
    else if (!mSettings.highlight_syntax && mHighlighter)
    {
        mHighlighter->RemoveHighlight(*mDocument);
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

QString TextEditor::GetCurrentWord() const
{
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

} // gui
