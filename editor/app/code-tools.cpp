// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <QKeyEvent>
#  include <QSyntaxHighlighter>
#  include <QTextDocument>
#include "warnpop.h"

#include "base/logging.h"
#include "app/eventlog.h"
#include "game/entity.h"
#include "game/scene.h"
#include "uikit/window.h"
#include "editor/app/code-tools.h"
#include "editor/app/workspace.h"
#include "editor/app/resource.h"

namespace app
{

class CodeAssistant::SyntaxHighlightImpl : public QSyntaxHighlighter
{
public:
    explicit SyntaxHighlightImpl(QTextDocument* parent)
      : QSyntaxHighlighter(parent)
    {
        // the text document becomes the owner of this. Smart! /s
    }
    ~SyntaxHighlightImpl()
    {
        DEBUG("Destroy CodeAssistant::SyntaxHighlightImpl");
    }
    void SetLuaTheme(app::LuaTheme* theme)
    { mTheme = theme; }
    void SetLuaParser(app::LuaParser* parser)
    { mParser = parser; }
protected:
    virtual void highlightBlock(const QString& text) override
    {
        // potential problem here that the characters in the plainText() result
        // don't map exactly to the text characters in the text document blocks.
        // Maybe this should use a cursor ? (probably slow as a turtle)
        // Investigate this more later if there's a problem .

        QTextBlock text_block = currentBlock();
        // character offset into the document where the block begins.
        // the position includes all characters prior to including formatting
        // characters such as newline.
        const auto text_block_start = text_block.position();
        const auto text_block_length = text_block.length();

        if (const auto* color = mTheme->GetColor(app::LuaTheme::Key::Other))
        {
            QTextCharFormat format;
            format.setForeground(*color);
            setFormat(text_block_start, text_block_length, format);
        }

        const auto& blocks = mParser->FindBlocks(text_block_start, text_block_length);
        for (const auto& block: blocks)
        {
            if (auto* color = mTheme->GetColor(block.type))
            {
                QTextCharFormat format;
                format.setForeground(*color);

                const auto block_offset = block.start - text_block_start;
                // setting the format is specified in offsets into the block itself!
                setFormat((int) block_offset, (int) block.length, format);
            }
        }
    }
private:
    app::LuaTheme* mTheme = nullptr;
    app::LuaParser* mParser = nullptr;
};

CodeAssistant::CodeAssistant(app::Workspace* workspace)
  : mWorkspace(workspace)
{
    mTheme.SetTheme(app::LuaTheme::Theme::Monokai);
    mModel.SetMode(app::LuaDocTableModel::Mode::CodeCompletion);
    mProxy.SetTableModel(&mModel);
}
CodeAssistant::~CodeAssistant()
{
    DEBUG("Destroy CodeAssistant)");
    delete mHilight;
}

void CodeAssistant::SetTheme(const QString& theme)
{
    if (theme == "Monokai")
        mTheme.SetTheme(app::LuaTheme::Theme::Monokai);
    else if (theme == "Solar Flare")
        mTheme.SetTheme(app::LuaTheme::Theme::SolarFlare);
    else if (theme == "Pastel Dream")
        mTheme.SetTheme(app::LuaTheme::Theme::PastelDream);
    else if (theme == "Dark Mirage")
        mTheme.SetTheme(app::LuaTheme::Theme::DarkMirage);
    else if (theme == "Cyber Flux")
        mTheme.SetTheme(app::LuaTheme::Theme::CyberFlux);
    else if (theme == "Orange Crush")
        mTheme.SetTheme(app::LuaTheme::Theme::OrangeCrush);
}

const LuaParser::Symbol* CodeAssistant::FindSymbol(const QString& name) const
{
    return mParser.FindSymbol(name);
}

bool CodeAssistant::StartCompletion(const QKeyEvent* event,
                                 const QTextDocument& document,
                                 const QTextCursor& cursor)
{
    const auto key = event->key();
    if (key == Qt::Key_Period || key == Qt::Key_Colon)
    {

        // the problem with this simple code here is that WordUnderCursor
        // returns a word that is a combination of the characters before
        // and after the cursor. For example if user is editing "some|thing"
        // and | is the cursor WordUnderCursor will be "something".
        // In our use-case however we only want the prefix string that
        // immediately precedes the current cursor position, so in the above
        // example we'd only want "some".

        //QTextCursor tc = cursor;
        //tc.select(QTextCursor::WordUnderCursor);
        //QString word = tc.selectedText();

        // Take the current cursor position in the document and read backwards
        // until space is encountered (or start of document happens).
        QString word;
        int pos = cursor.position();
        while (--pos >= 0)
        {
            auto cha = document.characterAt(pos);
            if (cha.isSpace()  || cha == '(' || cha == '[')
                break;
            word.prepend(cha);
        }

        if (word.isEmpty())
            return false;

        const auto size = word.size();
        // simple case, if we're editing a digit like 123.0 then don't
        // open the completion window. the regexp is trying to distinguish
        // between a case like 123.0 and abc123, the latter is a valid identifier
        // name in Lua.
        if (word[size-1].isDigit() && !word.contains(QRegExp("[a-z]")) && !word.contains(QRegExp("[A-Z]")))
            return false;

        // string concatenation operator is .. in Lua.
        if (word[size-1] == '.')
            return false;

        if (const auto* block = mParser.FindBlock(cursor.position()))
        {
            if (block->type == app::LuaSyntax::Comment ||
                block->type == app::LuaSyntax::Literal)
                return false;
        }

        DEBUG("Start code completion for word. [word='%1']", word);

        word = DiscoverDynamicCompletions(word);

        // interpret period as something like glm.length()
        // i.e. for completion assume the prefix is a table name.
        // thus, the filtering option is using prefix as table name
        // and showing only Functions and Properties.
        mProxy.ClearFilter();
        mProxy.SetTableNameFilter(word);
        if (key == Qt::Key_Period)
        {
            mProxy.SetVisible(0); // nothing
            mProxy.SetVisible(app::LuaDocModelProxy::Show::TableProperty, true);
            mProxy.SetVisible(app::LuaDocModelProxy::Show::Function, true);
            mProxy.SetVisible(app::LuaDocModelProxy::Show::Table, true);
        }
        else if (key == Qt::Key_Colon)
        {
            mProxy.SetVisible(0); // nothing
            mProxy.SetVisible(app::LuaDocModelProxy::Show::Method, true);
        }
        mProxy.invalidate();

        return true;
    }
    return false;
}

void CodeAssistant::FilterPossibleCompletions(const QString& input)
{
    QString key;
    for (int i=0; i<input.size(); ++i)
    {
        if (input[i] == ' ' || input[i] == '(' || input[i] == '.' || input[i] == '=')
            break;
        key += input[i];
    }
    mProxy.SetFieldNameFilter(key);
    mProxy.invalidate();
}

bool CodeAssistant::FinishCompletion(const QString& input,
                                  const QModelIndex& index,
                                  const QTextDocument& document,
                                  QTextCursor& cursor)
{
    if (input.isEmpty() && !index.isValid())
        return false;

    // text takes precedence. i.e. the user can type something beyond
    // the completion while the popup is open. For example "foobar = 123".
    // in this case just insert the text.
    if (!input.isEmpty() && !index.isValid())
    {
        cursor.movePosition(QTextCursor::Left);
        cursor.movePosition(QTextCursor::EndOfWord);
        cursor.insertText(input);
        return true;
    }
    const auto& item = mProxy.GetDocItemFromSource(index);

    if (item.type == app::LuaMemberType::TableProperty ||
        item.type == app::LuaMemberType::ObjectProperty ||
        item.type == app::LuaMemberType::Table)
    {
        QString completion = item.name;
        if (input.startsWith(completion))
            completion = input;

        cursor.movePosition(QTextCursor::Left);
        cursor.movePosition(QTextCursor::EndOfWord);
        cursor.insertText(completion);
    }
    else if (item.type == app::LuaMemberType::Function ||
             item.type == app::LuaMemberType::Method)
    {
        const QString& name = item.name;
        const QString& args = app::FormatArgCompletion(item);
        if (input.startsWith(name))
        {
            cursor.movePosition(QTextCursor::Left);
            cursor.movePosition(QTextCursor::EndOfWord);
            cursor.insertText(input);
        }
        else
        {
            cursor.movePosition(QTextCursor::Left);
            cursor.movePosition(QTextCursor::EndOfWord);
            cursor.insertText(name);
            const auto pos = cursor.position();
            cursor.insertText(args);
            if (!item.args.empty())
            {
                cursor.setPosition(pos + 1);
            }
        }
    }
    return true;
}

CodeCompleter::ApiHelp CodeAssistant::GetCompletionHelp(const QModelIndex& index) const
{
    ApiHelp ret;
    if (index.isValid())
    {
        const auto& item = mProxy.GetDocItemFromSource(index);
        ret.args = app::FormatArgHelp(item, app::LuaHelpStyle::DescriptionFormat,
                                      app::LuaHelpFormat::PlainText);
        ret.desc = app::FormatHelp(item, app::LuaHelpFormat::PlainText);
        ret.name = item.name;
    }
    return ret;
}

QAbstractItemModel* CodeAssistant::GetCompletionModel()
{
    return &mProxy;
}

void CodeAssistant::ApplyHighlight(QTextDocument& document)
{
    if (mHilight == nullptr)
    {
        mHilight = new SyntaxHighlightImpl(&document);
        mHilight->SetLuaTheme(&mTheme);
        mHilight->SetLuaParser(&mParser);
    }
    ASSERT(mHilight->document() == &document);
}
void CodeAssistant::RemoveHighlight(QTextDocument& document)
{
    delete mHilight;
    mHilight = nullptr;
}

void CodeAssistant::CleanState()
{
    mParser.ClearParseState();
    mSource.clear();
}
void CodeAssistant::ParseSource(QTextDocument& document)
{
    mSource = document.toPlainText();
    mParser.ParseSource(mSource);
    if (mHilight)
        mHilight->rehighlight();
}
void CodeAssistant::EditSource(QTextDocument& document, uint32_t position, uint32_t chars_removed, uint32_t chars_added)
{
    if (!mParser.HasParseState())
    {
        ParseSource(document);
        return;
    }

    QString current = document.toPlainText();
    app::LuaParser::Edit edit;
    edit.position = position;
    edit.characters_added = chars_added;
    edit.characters_removed = chars_removed;
    edit.new_source = &current;
    edit.old_source = &mSource;
    mParser.EditSource(edit);
    mSource = current;
    mParser.ParseSource(mSource);
    if (mHilight)
        mHilight->rehighlight();
}

QString CodeAssistant::DiscoverDynamicCompletions(const QString& word)
{
    if (!mUseCodeCompletionHeuristics)
        return "";

    mModel.ClearDynamicCompletions();

    // these are the "known" special names that we might expect to encounter.
    // This might cause failures if a user specified name is somehow associated
    // with these names but that's probably a bad idea in the game code anyway.
    if (word == "Audio")
        return "game.Audio";
    else if (word == "Game")
        return "game.Engine";
    else if (word == "Physics")
        return "game.Physics";
    else if (word == "Scene")
        return "game.Scene";
    else if (word == "State")
        return "game.KeyValueStore";
    else if (word == "ClassLib")
        return "game.ClassLibrary";

    // if the variable matches the name of some known class type that uses
    // scripts then assume that the type is the same.
    // For example 'ball' would match with an entity class resource called 'Ball'.
    // this way we can offer some extra properties such as the entity script vars
    // or UI widget names as completions
    for (size_t i=0; i<mWorkspace->GetNumUserDefinedResources(); ++i)
    {
        const auto& res = mWorkspace->GetUserDefinedResource(i);
        if (const auto* klass = res.GetContent<game::EntityClass>())
        {
            if (!klass->HasScriptFile())
                continue;
            else if (klass->GetScriptFileId() != mScriptId)
                continue;

            QString name = res.GetName();
            name.replace(' ', '_');
            name = name.toLower();
            if (name != word)
                continue;

            AddTableSuggestions(klass);
            return "game.Entity";
        }
        else if (const auto* klass = res.GetContent<game::SceneClass>())
        {
            if (!klass->HasScriptFile())
                continue;
            else if (klass->GetScriptFileId() != mScriptId)
                continue;

            QString name = res.GetName();
            name.replace(' ', '_');
            name = name.toLower();
            if (name != word)
                continue;

            AddTableSuggestions(klass);
            return "game.Scene";
        }
        else if (const auto* window = res.GetContent<uik::Window>())
        {
            if (!window->HasScriptFile())
                continue;
            else if (window->GetScriptFile() != mScriptId)
                continue;

            QString name = res.GetName();
            name.replace(' ', '_');
            name = name.toLower();
            if (name != word)
                continue;

            AddTableSuggestions(window);
            return "uik.Window";
        }
    }

    // special cases heuristics.
    if (word.endsWith("entity"))
        return "game.Entity";
    else if (word.endsWith("joint"))
        return "game.RigidBodyJoint";
    else if (word.endsWith("node"))
        if (word.contains("spatial"))
            return "game.SpatialNode";
        else return "game.EntityNode";
    else if (word.endsWith("scene"))
        return "game.Scene";
    else if (word.endsWith("body"))
        return "game.RigidBody";
    else if (word.endsWith("light"))
        return "game.BasicLight";
    else if (word.endsWith("widget"))
        return "uik.Widget";
    else if (word.endsWith("ui"))
        return "uik.Window";
    else if (word.endsWith("animator"))
        return "game.EntityStateController";
    else if (word.endsWith("drawable"))
        return "game.Drawable";
    else if (word.endsWith("item"))
        if (word.contains("draw") || word.contains("skin"))
            return "game.Drawable";
        else if (word.contains("text"))
            return "game.TextItem";
    else if (word.endsWith("transformer"))
            return "game.NodeTransformer";

    return app::FindLuaDocTableMatch(word);
}

void CodeAssistant::AddTableSuggestions(const game::EntityClass* klass)
{
    std::vector<app::LuaMemberDoc> docs;

    for (size_t i=0; i<klass->GetNumScriptVars(); ++i)
    {
        const auto& var = klass->GetScriptVar(i);
        app::LuaMemberDoc doc;
        doc.type = app::LuaMemberType::TableProperty;
        doc.name = app::FromUtf8(var.GetName());
        doc.desc = app::toString("Entity script variable (%1)", var.IsReadOnly() ? "readonly" : "read/write");
        doc.table = "game.Entity";
        if (var.IsArray())
            doc.ret = app::toString("%1 []", var.GetType());
        else doc.ret = app::toString("%1", var.GetType());

        docs.push_back(std::move(doc));
    }
    mModel.SetDynamicCompletions(std::move(docs));
}

void CodeAssistant::AddTableSuggestions(const game::SceneClass* klass)
{
    std::vector<app::LuaMemberDoc> docs;

    for (size_t i=0; i<klass->GetNumScriptVars(); ++i)
    {
        const auto& var = klass->GetScriptVar(i);
        app::LuaMemberDoc doc;
        doc.type = app::LuaMemberType::TableProperty;
        doc.name = app::FromUtf8(var.GetName());
        doc.desc = app::toString("Scene script variable (%1)", var.IsReadOnly() ? "readonly" : "read/write");
        doc.table = "game.Scene";
        if (var.IsArray())
            doc.ret = app::toString("%1 []", var.GetType());
        else doc.ret = app::toString("%1", var.GetType());

        docs.push_back(std::move(doc));
    }
    mModel.SetDynamicCompletions(std::move(docs));
}

void CodeAssistant::AddTableSuggestions(const uik::Window* window)
{
    std::vector<app::LuaMemberDoc> docs;

    for (size_t i=0; i<window->GetNumWidgets(); ++i)
    {
        const auto& widget = window->GetWidget(i);
        app::LuaMemberDoc doc;
        doc.table = "uik.Window";
        doc.type  = app::LuaMemberType::TableProperty;
        doc.name  = app::FromUtf8(widget.GetName());
        doc.desc  = app::toString("Widget '%1'", widget.GetName());
        doc.ret   = app::toString(widget.GetType());
        docs.push_back(std::move(doc));
    }
    mModel.SetDynamicCompletions(std::move(docs));
}

} // namespace