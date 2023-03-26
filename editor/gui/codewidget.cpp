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
#include "editor/app/eventlog.h"
#include "editor/app/lua-tools.h"
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

void CodeCompleter::Open(const QRect& rect)
{
    setGeometry(rect);
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

void CodeCompleter::SetModel(app::LuaDocModelProxy* model)
{
    mUI.tableView->setModel(model);
    mModel = model;
    Connect(mUI.tableView, this, &CodeCompleter::TableSelectionChanged);
}

void CodeCompleter::on_lineEdit_textChanged(const QString& text)
{
    QString key;
    for (int i=0; i<text.size(); ++i)
    {
        if (text[i] == ' ' || text[i] == '(' || text[i] == '.' || text[i] == '=')
            break;
        key += text[i];
    }

    mModel->SetFieldNameFilter(key);
    mModel->invalidate();
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

    if (mModel->rowCount() == 0)
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

    const auto& item = mModel->GetDocItemFromSource(index);
    SetValue(mUI.help, item.desc);
    SetValue(mUI.args, app::FormatArgHelp(item));
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
        mHighlighter->Parse(document()->toPlainText());
        mHighlighter->rehighlight();
    }
}

bool TextEditor::CancelCompletion()
{
    if (mCompleter->IsOpen())
    {
        mCompleter->Close();
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
  , mDocModel(app::LuaDocTableModel::Mode::CodeCompletion)
{
    connect(this, &TextEditor::blockCountChanged,     this, &TextEditor::UpdateLineNumberAreaWidth);
    connect(this, &TextEditor::updateRequest,         this, &TextEditor::UpdateLineNumberArea);
    connect(this, &TextEditor::cursorPositionChanged, this, &TextEditor::HighlightCurrentLine);
    connect(this, &TextEditor::copyAvailable,         this, &TextEditor::CopyAvailable);
    connect(this, &TextEditor::undoAvailable,         this, &TextEditor::UndoAvailable);

    open_editors.insert(this);

    setLineWrapMode(LineWrapMode::NoWrap);

    mDocProxy.SetTableModel(&mDocModel);
    mCompleter.reset(new CodeCompleter(this));
    mCompleter->SetModel(&mDocProxy);
    connect(mCompleter.get(), &CodeCompleter::Complete, this, &TextEditor::Complete);
}

TextEditor::~TextEditor()
{
    open_editors.erase(this);

    mCompleter->disconnect(this);
    if (mCompleter->IsOpen())
        mCompleter->Close();
    mCompleter.reset();
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

void TextEditor::Complete(const QString& text, const QModelIndex& index)
{
    if (text.isEmpty() && !index.isValid())
        return;

    // text takes precedence. i.e. the user can type something beyond
    // the completion while the popup is open. For example "foobar = 123".
    // in this case just insert the text.
    if (!text.isEmpty() && !index.isValid())
    {
        QTextCursor tc = textCursor();
        tc.movePosition(QTextCursor::Left);
        tc.movePosition(QTextCursor::EndOfWord);
        tc.insertText(text);
        return;
    }

    const auto& item = mDocProxy.GetDocItemFromSource(index);

    if (item.type == app::LuaMemberType::TableProperty ||
        item.type == app::LuaMemberType::ObjectProperty)
    {
        QString completion = item.name;
        if (text.startsWith(completion))
            completion = text;

        QTextCursor tc = textCursor();
        tc.movePosition(QTextCursor::Left);
        tc.movePosition(QTextCursor::EndOfWord);
        tc.insertText(completion);
        setTextCursor(tc);
    }
    else if (item.type == app::LuaMemberType::Function ||
             item.type == app::LuaMemberType::Method)
    {
        const QString& name = item.name;
        const QString& args = app::FormatArgCompletion(item);
        if (text.startsWith(name))
        {
            QTextCursor tc = textCursor();
            tc.movePosition(QTextCursor::Left);
            tc.movePosition(QTextCursor::EndOfWord);
            tc.insertText(text);
        }
        else
        {
            QTextCursor tc = textCursor();
            tc.movePosition(QTextCursor::Left);
            tc.movePosition(QTextCursor::EndOfWord);
            tc.insertText(name);
            const auto pos = tc.position();
            tc.insertText(args);
            if (!item.args.empty())
            {
                tc.setPosition(pos + 1);
                setTextCursor(tc);
            }
        }
    }
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

    if (key == Qt::Key_Tab && mSettings.insert_spaces)
    {
        // convert tab to spaces.
        this->insertPlainText("    ");
        return;
    }

    // open completion dialog on . or :
    if (key == Qt::Key_Period || key == Qt::Key_Colon)
    {
        QString word = GetCurrentWord();
        if (word.isEmpty())
        {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }
        // simple case, if we're editing a digit like 123.0 then don't
        // open the completion window.
        const auto size = word.size();

        if (word[size-1].isDigit() && !word.contains(QRegExp("[a-z]")) && !word.contains(QRegExp("[A-Z]")))
        {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }
        Reparse();
        QTextCursor cursor = textCursor();
        if (const auto* block = mHighlighter->FindBlockByOffset(cursor.position()))
        {
            if (block->type == app::LuaParser::HighlightKey::Comment ||
                block->type == app::LuaParser::HighlightKey::Literal) {
                QPlainTextEdit::keyPressEvent(event);
                return;
            }
        }

        DEBUG("Start code completion for prefix. [prefix='%1']", word);

        // if we don't know the table name we have no idea
        // what the type is. For example:
        // foo.Doodah()
        // foo:Doodah()
        word = app::FindLuaDocTableMatch(word);

        // interpret period as something like glm.length()
        // i.e. for completion assume the prefix is a table name.
        // thus, the filtering option is using prefix as table name
        // and showing only Functions and Properties.
        mDocProxy.ClearFilter();
        mDocProxy.SetTableNameFilter(word);
        if (key == Qt::Key_Period)
        {
            mDocProxy.SetVisible(0); // nothing
            mDocProxy.SetVisible(app::LuaDocModelProxy::Show::TableProperty, true);
            mDocProxy.SetVisible(app::LuaDocModelProxy::Show::Function, true);
        }
        else if (key == Qt::Key_Colon)
        {
            mDocProxy.SetVisible(0); // nothing
            mDocProxy.SetVisible(app::LuaDocModelProxy::Show::Method, true);
        }
        mDocProxy.invalidate();

        const QRect rect = cursorRect();
        QRect popup;
        popup.setWidth(500);
        popup.setHeight(300);
        popup.moveTo(mapToGlobal(rect.bottomRight()));
        popup.translate(rect.width(), 10.0);
        mCompleter->Open(popup);

        // propagate to the parent class so the character gets inserted
        // into the text.
        QPlainTextEdit::keyPressEvent(event);
        return;
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
        if (mHighlighter && (key == Qt::Key_Return))
            mHighlighter->Parse(document()->toPlainText());

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
    }

    if (mSettings.highlight_syntax && !mHighlighter)
    {
        mHighlighter = new app::LuaParser(mDocument);
        mHighlighter->setParent(this);
        mHighlighter->SetTheme(app::LuaParser::ColorTheme::Monokai);
        mHighlighter->Parse(document()->toPlainText());
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

QString TextEditor::GetCurrentWord() const
{
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

} // gui
