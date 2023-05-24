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
#  include <QSortFilterProxyModel>
#  include <QMessageBox>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QTextStream>
#  include <QTextCursor>
#  include <QFile>
#  include <QHash>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <set>

#include "base/assert.h"
#include "base/color4f.h"
#include "base/utility.h"
#include "editor/app/code-tools.h"
#include "editor/app/lua-tools.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/app/process.h"
#include "editor/app/lua-doc.h"
#include "editor/gui/settings.h"
#include "editor/gui/scriptwidget.h"
#include "game/entity.h"
#include "game/scene.h"
#include "uikit/window.h"

namespace {
std::set<gui::ScriptWidget*> all_script_widgets;

struct JumpPosition {
    QString WidgetId;
    int position = 0;
};
std::vector<JumpPosition> JumpStack;

} // namespace

namespace gui
{
// static
ScriptWidget::Settings ScriptWidget::mSettings;

// Our LuaParser provides both syntax highlights and code completion.
class ScriptWidget::LuaParser : public app::CodeCompleter,
                                public app::CodeHighlighter
{
public:
    using Symbol = app::LuaParser::Symbol;

    LuaParser(app::Workspace* workspace)
      : mWorkspace(workspace)
    {
        mTheme.SetTheme(app::LuaTheme::Theme::Monokai);
        mModel.SetMode(app::LuaDocTableModel::Mode::CodeCompletion);
        mProxy.SetTableModel(&mModel);
    }
    ~LuaParser() override
    {
        DEBUG("Destroy ScriptWidget::LuaParser");
        delete mHilight;
    }

    const Symbol* FindSymbol(const QString& name) const
    {
        return mParser.FindSymbol(name);
    }

    virtual bool StartCompletion(const QKeyEvent* event,
                                 const QTextDocument& document,
                                 const QTextCursor& cursor)  override
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
    virtual void FilterPossibleCompletions(const QString& input) override
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
    virtual bool FinishCompletion(const QString& input,
                                  const QModelIndex& index,
                                  const QTextDocument& document,
                                  QTextCursor& cursor)  override
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
    virtual ApiHelp GetCompletionHelp(const QModelIndex& index) const override
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
    virtual QAbstractItemModel* GetCompletionModel() override
    {
        return &mProxy;
    }
    // CodeHighlighter impl
    // Apply code highlight on the given document.
    virtual void ApplyHighlight(QTextDocument& document) override
    {
        if (mHilight == nullptr)
        {
            mHilight = new SyntaxHighlightImpl(&document);
            mHilight->SetLuaTheme(&mTheme);
            mHilight->SetLuaParser(&mParser);
        }
        ASSERT(mHilight->document() == &document);
    }
    virtual void RemoveHighlight(QTextDocument& document) override
    {
        delete mHilight;
        mHilight = nullptr;
    }

    void SetScriptId(const std::string& id)
    { mScriptId = id; }
    void SetCodeCompletionHeuristics(bool on_off)
    { mUseCodeCompletionHeuristics = on_off; }

    void CleanState()
    {
        mParser.ClearParseState();
        mSource.clear();
    }
    void ParseSource(QTextDocument& document)
    {
        mSource = document.toPlainText();
        mParser.ParseSource(mSource);
        if (mHilight)
            mHilight->rehighlight();
    }
    void EditSource(QTextDocument& document, uint32_t position, uint32_t chars_removed, uint32_t chars_added)
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

protected:
    class SyntaxHighlightImpl : public QSyntaxHighlighter {
    public:
        explicit SyntaxHighlightImpl(QTextDocument* parent)
          : QSyntaxHighlighter(parent)
        {
            // the text document becomes the owner of this. Smart! /s
        }
        ~SyntaxHighlightImpl()
        {
            DEBUG("Destroy ScriptWidget::LuaParser::SyntaxHighlightImpl");
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
private:
    // use some heuristics to suggest a table name for code completion aid.
    QString DiscoverDynamicCompletions(const QString& word)
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
        else if (word.endsWith("node"))
            if (word.contains("spatial"))
                return "game.SpatialNode";
            else return "game.EntityNode";
        else if (word.endsWith("scene"))
            return "game.Scene";
        else if (word.endsWith("body"))
            return "game.RigidBody";
        else if (word.endsWith("widget"))
            return "uik.Widget";
        else if (word.endsWith("ui"))
            return "uik.Window";
        else if (word.endsWith("animator"))
            return "game.Animator";
        else if (word.endsWith("drawable"))
            return "game.Drawable";
        else if (word.endsWith("item"))
            if (word.contains("draw"))
                return "game.Drawable";
            else if (word.contains("text"))
                return "game.TextItem";

        return app::FindLuaDocTableMatch(word);
    }
    void AddTableSuggestions(const game::EntityClass* klass)
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
    void AddTableSuggestions(const game::SceneClass* klass)
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
    void AddTableSuggestions(const uik::Window* window)
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

private:
    SyntaxHighlightImpl* mHilight = nullptr;

    app::LuaTheme mTheme;
    app::LuaParser mParser;
    app::LuaDocTableModel mModel;
    app::LuaDocModelProxy mProxy;
    app::Workspace* mWorkspace = nullptr;
    std::string mScriptId;
    bool mUseCodeCompletionHeuristics = true;
    QString mSource;
};

ScriptWidget::ScriptWidget(app::Workspace* workspace)
{
    DEBUG("Create ScriptWidget");
    all_script_widgets.insert(this);

    app::InitLuaDoc();

    mWorkspace = workspace;
    mLuaHelpFilter.SetTableModel(&mLuaHelpData);
    mLuaHelpFilter.setSourceModel(&mLuaHelpData);
    mLuaParser.reset(new LuaParser(workspace));
    mLuaParser->SetCodeCompletionHeuristics(mSettings.use_code_heuristics);

    auto* layout = new QPlainTextDocumentLayout(&mDocument);
    layout->setParent(this);
    mDocument.setDocumentLayout(layout);

    mUI.setupUi(this);
    mUI.formatter->setVisible(false);
    mUI.modified->setVisible(false);
    mUI.find->setVisible(false);
    mUI.code->SetDocument(&mDocument);
    mUI.code->SetCompleter(mLuaParser.get());
    mUI.code->SetSettings(mSettings.editor_settings);
    mUI.tableView->setModel(&mLuaHelpFilter);
    mUI.tableView->setColumnWidth(0, 100);
    mUI.tableView->setColumnWidth(2, 200);
    // capture some special key presses in order to move the
    // selection (current item) in the list widget more conveniently.
    mUI.filter->installEventFilter(this);
    mUI.textBrowser->setHtml(app::GenerateLuaDocHtml());

    PopulateFontSizes(mUI.editorFontSize);
    SetValue(mUI.editorFontName, -1);
    SetValue(mUI.editorFontSize, -1);
    SetValue(mUI.chkFormatOnSave, Qt::PartiallyChecked);

    connect(mUI.tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &ScriptWidget::TableSelectionChanged);
    connect(&mWatcher, &QFileSystemWatcher::fileChanged, this, &ScriptWidget::FileWasChanged);
    connect(&mDocument, &QTextDocument::contentsChange, this, &ScriptWidget::DocumentEdited);

    connect(mWorkspace, &app::Workspace::ResourceRemoved, this, &ScriptWidget::ResourceRemoved);

    QTimer::singleShot(0, this, &ScriptWidget::SetInitialFocus);
}

ScriptWidget::ScriptWidget(app::Workspace* workspace, const app::Resource& resource) : ScriptWidget(workspace)
{
    const app::Script* script = nullptr;
    resource.GetContent(&script);

    const std::string& uri = script->GetFileURI();
    DEBUG("Editing script: '%1'", uri);
    mFilename     = mWorkspace->MapFileToFilesystem(app::FromUtf8(uri));
    mResourceID   = resource.GetId();
    mResourceName = resource.GetName();
    mWatcher.addPath(mFilename);
    mLuaParser->SetScriptId(resource.GetIdUtf8());
    LoadDocument(mFilename);

    bool show_settings = true;
    unsigned scroll_position = 0;
    unsigned cursor_position = 0;
    GetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    GetUserProperty(resource, "help_splitter", mUI.helpSplitter);
    GetUserProperty(resource, "show_settings", &show_settings);
    GetUserProperty(resource, "scroll_position", &scroll_position);
    GetUserProperty(resource, "cursor_position", &cursor_position);
    SetVisible(mUI.settings, show_settings);
    SetEnabled(mUI.actionSettings, !show_settings);

    // Custom font name setting (applies to this widget only)
    QString font_name;
    if (GetUserProperty(resource, "font_name", &font_name))
    {
        mUI.code->SetFontName(font_name);
        SetValue(mUI.editorFontName, font_name);
    }

    // Custom font size setting (applies to this widget only)
    QString font_size;
    if (GetUserProperty(resource, "font_size", &font_size))
    {
        mUI.code->SetFontSize(font_size.toInt());
        SetValue(mUI.editorFontSize, font_size);
    }

    bool format_on_save = false;
    if (GetProperty(resource, "format_on_save", &format_on_save))
        SetValue(mUI.chkFormatOnSave, format_on_save);

    QTimer::singleShot(10, [this, scroll_position, cursor_position]() {
        mUI.code->SetCursorPosition(cursor_position);
        mUI.code->verticalScrollBar()->setValue(scroll_position);
    });
    setWindowTitle(resource.GetName());
}
ScriptWidget::~ScriptWidget()
{
    DEBUG("Destroy ScriptWidget");
    mWatcher.disconnect(this);
    mDocument.disconnect(this);
    mUI.code->SetCompleter(nullptr);
    mLuaParser.reset();
    all_script_widgets.erase(this);

    if (all_script_widgets.empty())
        JumpStack.clear();
}

QString ScriptWidget::GetId() const
{
    return mResourceID;
}

bool ScriptWidget::CanTakeAction(Actions action, const Clipboard*) const
{
    // canPaste seems to be broken??
    switch (action)
    {
        // todo: could increase/decrease font size on zoom in/out
        case Actions::CanZoomOut:
        case Actions::CanZoomIn:
            return false;
        case Actions::CanReloadShaders:
        case Actions::CanReloadTextures:
            return false;
        case Actions::CanCut:
        case Actions::CanCopy:
            return mUI.code->hasFocus() &&  mUI.code->CanCopy();
        case Actions::CanPaste:
            return mUI.code->hasFocus() && mUI.code->canPaste();
        case Actions::CanUndo:
            return mUI.code->hasFocus() && mUI.code->CanUndo();
    }
    return false;
}

void ScriptWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionPreview);
    bar.addSeparator();
    bar.addAction(mUI.actionFindText);
    bar.addAction(mUI.actionReplaceText);
    bar.addAction(mUI.actionGotoSymbol);
    bar.addAction(mUI.actionGoBack);
    bar.addSeparator();
    bar.addAction(mUI.actionFindHelp);
    bar.addSeparator();
    bar.addAction(mUI.actionSettings);
    bar.addSeparator();
}
void ScriptWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionPreview);
    menu.addSeparator();
    menu.addAction(mUI.actionFindText);
    menu.addAction(mUI.actionReplaceText);
    menu.addAction(mUI.actionGotoSymbol);
    menu.addAction(mUI.actionGoBack);
    menu.addSeparator();
    menu.addAction(mUI.actionFindHelp);
    menu.addSeparator();
    menu.addAction(mUI.actionOpen);
    menu.addSeparator();
    menu.addAction(mUI.actionSettings);
}

void ScriptWidget::Cut(Clipboard&)
{
    // uses the global QClipboard which is fine here
    // because that allows cutting/pasting between apps
    mUI.code->cut();
}
void ScriptWidget::Copy(Clipboard&) const
{
    // uses the global QClipboard which is fine here
    // because that allows cutting/pasting between apps
    mUI.code->copy();
}
void ScriptWidget::Paste(const Clipboard&)
{
    // uses the global QClipboard which is fine here
    // because that allows cutting/pasting between apps
    mUI.code->paste();
}

void ScriptWidget::Save()
{
    on_actionSave_triggered();
}

bool ScriptWidget::SaveState(gui::Settings& settings) const
{
    // todo: if there are changes that have not been saved
    // to the file they're then lost. options are to either
    // ask for save when shutting down or to save to an
    // intermediate scrap file somewhere.
    settings.SetValue("Script", "resource_id", mResourceID);
    settings.SetValue("Script", "resource_name", mResourceName);
    settings.SetValue("Script", "filename", mFilename);
    settings.SetValue("Script", "show_settings", mUI.settings->isVisible());
    settings.SetValue("Script", "cursor_position", mUI.code->GetCursorPosition());
    settings.SetValue("Script", "scroll_position", mUI.code->verticalScrollBar()->value());
    settings.SaveWidget("Script", mUI.findText);
    settings.SaveWidget("Script", mUI.replaceText);
    settings.SaveWidget("Script", mUI.findBackwards);
    settings.SaveWidget("Script", mUI.findCaseSensitive);
    settings.SaveWidget("Script", mUI.findWholeWords);
    settings.SaveWidget("Script", mUI.mainSplitter);
    settings.SaveWidget("Script", mUI.helpSplitter);
    settings.SaveWidget("Script", mUI.tableView);

    if (mUI.editorFontName->currentIndex() != -1)
        settings.SetValue("Script", "font_name", mUI.editorFontName->currentFont().toString());
    if (mUI.editorFontSize->currentIndex() != -1)
        settings.SetValue("Script", "font_size", mUI.editorFontSize->currentText());

    const auto format_on_save = mUI.chkFormatOnSave->checkState();
    if (format_on_save == Qt::Checked)
        settings.SetValue("Script", "format_on_save", true);
    else if (format_on_save == Qt::Unchecked)
        settings.SetValue("Script", "format_on_save", false);

    return true;
}
bool ScriptWidget::LoadState(const gui::Settings& settings)
{
    bool show_settings = true;
    unsigned cursor_position = 0;
    unsigned scroll_position = 0;
    settings.GetValue("Script", "resource_id", &mResourceID);
    settings.GetValue("Script", "resource_name", &mResourceName);
    settings.GetValue("Script", "filename", &mFilename);
    settings.GetValue("Script", "show_settings", &show_settings);
    settings.GetValue("Script", "cursor_position", &cursor_position);
    settings.GetValue("Script", "scroll_position", &scroll_position);
    settings.LoadWidget("Script", mUI.findText);
    settings.LoadWidget("Script", mUI.replaceText);
    settings.LoadWidget("Script", mUI.findBackwards);
    settings.LoadWidget("Script", mUI.findCaseSensitive);
    settings.LoadWidget("Script", mUI.findWholeWords);
    settings.LoadWidget("Script", mUI.mainSplitter);
    settings.LoadWidget("Script", mUI.helpSplitter);
    settings.LoadWidget("Script", mUI.tableView);
    SetVisible(mUI.settings, show_settings);
    SetEnabled(mUI.actionSettings, !show_settings);

    QString font_name;
    if (settings.GetValue("Script", "font_name", &font_name))
    {
        mUI.code->SetFontName(font_name);
        SetValue(mUI.editorFontName, font_name);
    }

    QString font_size;
    if (settings.GetValue("Script", "font_size", &font_size))
    {
        mUI.code->SetFontSize(font_size.toInt());
        SetValue(mUI.editorFontSize, font_size);
    }

    bool format_on_save = false;
    if (settings.GetValue("Script", "format_on_save", &format_on_save))
        SetValue(mUI.chkFormatOnSave, format_on_save);

    mLuaParser->SetScriptId(app::ToUtf8(mResourceID));

    if (mFilename.isEmpty())
        return true;
    mWatcher.addPath(mFilename);

    if (LoadDocument(mFilename))
    {
        QTimer::singleShot(10, [this, scroll_position, cursor_position]() {
            mUI.code->SetCursorPosition(cursor_position);
            mUI.code->verticalScrollBar()->setValue(scroll_position);
        });
    }
    return true;
}

bool ScriptWidget::HasUnsavedChanges() const
{
    if (!mFileHash)
        return false;

    const auto& plain = mDocument.toPlainText();
    const auto hash = qHash(plain);
    return mFileHash != hash;
}

bool ScriptWidget::OnEscape()
{
    if (!mUI.code->CancelCompletion())
    {
        if (mUI.formatter->isVisible())
            mUI.formatter->setVisible(false);
        else if (mUI.find->isVisible())
            on_btnFindClose_clicked();
        else if (mUI.settings->isVisible())
            on_btnSettingsClose_clicked();
    }
    mUI.code->setFocus();
    return true;
}

void ScriptWidget::Activate()
{
    mUI.code->setFocus();
}

void ScriptWidget::on_actionPreview_triggered()
{
    if (mResourceID.isEmpty())
        return;

    emit RequestScriptLaunch(mResourceID);
}

void ScriptWidget::on_actionSave_triggered()
{
    QString filename = mFilename;

    if (filename.isEmpty())
    {
        const auto& luadir = app::JoinPath(mWorkspace->GetDir(), "lua");
        const auto& file = QFileDialog::getSaveFileName(this,
            tr("Save Script As ..."), luadir,
            tr("Lua Scripts (*.lua)"));
        if (file.isEmpty())
            return;
        filename = file;
    }
    // save file
    {
        const QString& text = mDocument.toPlainText();
        QFile file;
        file.setFileName(filename);
        file.open(QIODevice::WriteOnly);
        if (!file.isOpen())
        {
            ERROR("Failed to open file for writing. [file='%1', error=%2]", filename, file.error());
            QMessageBox msg(this);
            msg.setText(tr("There was an error saving the file.\n%1").arg(file.errorString()));
            msg.setIcon(QMessageBox::Critical);
            msg.exec();
            return;
        }
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << text;
        INFO("Saved Lua script '%1'", filename);
        NOTE("Saved Lua script '%1'", filename);
        mFilename = filename;
        mFileHash = qHash(text);
    }

    const auto format_on_save = mUI.chkFormatOnSave->checkState();
    if ((format_on_save == Qt::PartiallyChecked && mSettings.lua_format_on_save) ||
         format_on_save == Qt::Checked)
    {
        const auto scroll_position = mUI.code->verticalScrollBar()->value();
        const auto cursor_position = mUI.code->GetCursorPositionHashIgnoreWS();

        QSignalBlocker blocker(&mWatcher);
        QString exec = mSettings.lua_formatter_exec;
        QString args = mSettings.lua_formatter_args;
        QStringList arg_list;
        QStringList tokens = args.split(" ", Qt::SkipEmptyParts);
        for (auto& token : tokens)
        {
            token.replace("${file}", mFilename);
            token.replace("${install-dir}", app::GetAppInstFilePath(""));
            arg_list << token;
        }
        exec.replace("${install-dir}", app::GetAppInstFilePath(""));

        QStringList stdout_buffer;
        QStringList stderr_buffer;
        app::Process::Error error;
        if (!app::Process::RunAndCapture(exec, "", arg_list, &stdout_buffer, &stderr_buffer, &error))
        {
            ERROR("Failed to run Lua code formatter. [error=%1]", error);
        }
        else
        {
            for (const auto& line : stdout_buffer)
                DEBUG("stdout: %1", line);

            QString message;
            message.append(stderr_buffer.join("\n"));
            message.append(stdout_buffer.join("\n"));
            if (!message.isEmpty())
            {
                mUI.formatter->setVisible(true);
                mUI.luaFormatStdErr->setPlainText(message);
            }
            else
            {
                mUI.formatter->setVisible(false);
            }
            LoadDocument(mFilename);
            mUI.code->verticalScrollBar()->setValue(scroll_position);
            mUI.code->SetCursorPositionFromHashIgnoreWS(cursor_position);
        }
    }

    // start watching this file if it wasn't being watched before.
    mWatcher.addPath(mFilename);

    if (mResourceID.isEmpty())
    {
        const QFileInfo info(mFilename);
        mResourceName = info.baseName();

        const auto& URI = mWorkspace->MapFileToWorkspace(mFilename);
        DEBUG("Script file URI '%1'", URI);

        app::Script script;
        script.SetFileURI(app::ToUtf8(URI));
        script.SetTypeTag(app::Script::TypeTag::ScriptData);
        app::ScriptResource resource(script, mResourceName);
        SetUserProperty(resource, "main_splitter", mUI.mainSplitter);
        SetUserProperty(resource, "help_splitter", mUI.helpSplitter);
        SetUserProperty(resource, "show_settings", mUI.settings->isVisible());
        SetUserProperty(resource, "cursor_position", mUI.code->GetCursorPosition());
        SetUserProperty(resource, "scroll_position", mUI.code->verticalScrollBar()->value());

        if (mUI.editorFontName->currentIndex() != -1)
            SetUserProperty(resource, "font_name", mUI.editorFontName->currentFont().toString());
        if (mUI.editorFontSize->currentIndex() != -1)
            SetUserProperty(resource, "font_size", mUI.editorFontSize->currentText());

        if (format_on_save == Qt::Checked)
            SetProperty(resource, "format_on_save", true);
        else if (format_on_save == Qt::Unchecked)
            SetProperty(resource, "format_on_save", false);

        mWorkspace->SaveResource(resource);
        mResourceID = app::FromUtf8(script.GetId());
        mLuaParser->SetScriptId(script.GetId());
    }
    else
    {
        auto* resource = mWorkspace->FindResourceById(mResourceID);
        SetUserProperty(*resource, "main_splitter", mUI.mainSplitter);
        SetUserProperty(*resource, "help_splitter", mUI.helpSplitter);
        SetUserProperty(*resource, "show_settings", mUI.settings->isVisible());
        SetUserProperty(*resource, "cursor_position", mUI.code->GetCursorPosition());
        SetUserProperty(*resource, "scroll_position", mUI.code->verticalScrollBar()->value());

        if (mUI.editorFontName->currentIndex() != -1)
            SetUserProperty(*resource, "font_name", mUI.editorFontName->currentFont().toString());
        else resource->DeleteUserProperty("font_name");
        if (mUI.editorFontSize->currentIndex() != -1)
            SetUserProperty(*resource, "font_size", mUI.editorFontSize->currentText());
        else resource->DeleteUserProperty("font_size");

        if (format_on_save == Qt::PartiallyChecked)
            resource->DeleteProperty("format_on_save");
        else if (format_on_save == Qt::Checked)
            resource->SetProperty("format_on_save", true);
        else if (format_on_save == Qt::Unchecked)
            resource->SetProperty("format_on_save", false);
    }
}

void ScriptWidget::on_actionOpen_triggered()
{
    if (mFilename.isEmpty())
    {
        QMessageBox msg(this);
        msg.setText(tr("You haven't yet saved the file. It cannot be opened in another application."));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }
    emit OpenExternalScript(mFilename);
}

void ScriptWidget::on_actionFindHelp_triggered()
{
    mUI.filter->setFocus();
    auto sizes = mUI.mainSplitter->sizes();
    ASSERT(sizes.size() == 2);
    if (sizes[1] == 0 && sizes[0] > 500)
    {
        sizes[0] = sizes[0] - 500;
        sizes[1] = 500;
        mUI.mainSplitter->setSizes(sizes);
    }

    const auto& word = mUI.code->GetCurrentWord();
    if (word.trimmed().isEmpty())
        return;

    if (const auto& table = app::FindLuaDocTableMatch(word); !table.isEmpty())
    {
        FilterHelp(table);
    }
    else if (const auto& field = app::FindLuaDocFieldMatch(word); !field.isEmpty())
    {
        // not a bug! want to filter by word to find whatever matches
        FilterHelp(word);
    }
}

void ScriptWidget::on_actionFindText_triggered()
{
    mUI.find->setVisible(true);
    mUI.findText->setFocus();
    SetValue(mUI.find, QString("Find text"));
    SetValue(mUI.findResult, QString(""));
    SetEnabled(mUI.btnReplaceNext, false);
    SetEnabled(mUI.btnReplaceAll, false);
    SetEnabled(mUI.replaceText, false);
}

void ScriptWidget::on_actionReplaceText_triggered()
{
    mUI.find->setVisible(true);
    mUI.findText->setFocus();
    SetValue(mUI.find, QString("Replace text"));
    SetValue(mUI.findResult, QString(""));
    SetEnabled(mUI.btnReplaceNext, true);
    SetEnabled(mUI.btnReplaceAll, true);
    SetEnabled(mUI.replaceText, true);
}

void ScriptWidget::on_actionSettings_triggered()
{
    SetVisible(mUI.settings, true);
    SetEnabled(mUI.actionSettings, false);
}

void ScriptWidget::on_actionGotoSymbol_triggered()
{
    const auto& word = mUI.code->GetCurrentWord();
    if (word.isEmpty())
    {
        NOTE("Current word is empty. Nothing to go to.");
        return;
    }

    // remember, widget could be an alias for this, thus
    // better to record our current position here.
    const auto position = mUI.code->GetCursorPosition();

    for (auto*  widget : all_script_widgets)
    {
        if (const auto* symbol = widget->mLuaParser->FindSymbol(word))
        {
            widget->mUI.code->SetCursorPosition(symbol->start);

            // record where we came from to the symbol
            JumpPosition jump;
            jump.WidgetId = this->GetId();
            jump.position = position;
            JumpStack.push_back(jump);

            emit FocusWidget(widget->GetId());
            return;
        }
    }
    NOTE(tr("No such symbol found. '%1'").arg(word));
}

void ScriptWidget::on_actionGoBack_triggered()
{
    if (JumpStack.empty())
        return;

    auto jump = JumpStack.back();

    JumpStack.pop_back();

    for (auto* widget : all_script_widgets)
    {
        if (widget->GetId() == jump.WidgetId)
        {
            widget->mUI.code->SetCursorPosition(jump.position);
            emit FocusWidget(widget->GetId());
            return;
        }
    }
}

void ScriptWidget::on_btnFindNext_clicked()
{
    QString text = GetValue(mUI.findText);
    if (text.isEmpty())
        return;

    QTextDocument::FindFlags flags;
    flags.setFlag(QTextDocument::FindBackward, GetValue(mUI.findBackwards));
    flags.setFlag(QTextDocument::FindCaseSensitively, GetValue(mUI.findCaseSensitive));
    flags.setFlag(QTextDocument::FindWholeWords, GetValue(mUI.findWholeWords));

    QTextCursor cursor = mUI.code->textCursor();
    cursor = mDocument.find(text, cursor, flags);
    if (cursor.isNull() && GetValue(mUI.findBackwards))
        cursor = mDocument.find(text, mDocument.characterCount(), flags);
    else if (cursor.isNull())
        cursor = mDocument.find(text, 0, flags);

    if (cursor.isNull())
    {
        SetValue(mUI.findResult, tr("No results found."));
        return;
    }
    SetValue(mUI.findResult, QString(""));
    mUI.code->setTextCursor(cursor);
}
void ScriptWidget::on_btnFindClose_clicked()
{
    mUI.find->setVisible(false);
}

void ScriptWidget::on_btnReplaceNext_clicked()
{
    QString text = GetValue(mUI.findText);
    if (text.isEmpty())
        return;
    QString replacement = GetValue(mUI.replaceText);

    QTextDocument::FindFlags flags;
    flags.setFlag(QTextDocument::FindBackward, GetValue(mUI.findBackwards));
    flags.setFlag(QTextDocument::FindCaseSensitively, GetValue(mUI.findCaseSensitive));
    flags.setFlag(QTextDocument::FindWholeWords, GetValue(mUI.findWholeWords));

    QTextCursor cursor = mUI.code->textCursor();
    cursor = mDocument.find(text, cursor, flags);
    if (cursor.isNull() && GetValue(mUI.findBackwards))
        cursor = mDocument.find(text, mDocument.characterCount(), flags);
    else if (cursor.isNull())
        cursor = mDocument.find(text, 0, flags);

    if (cursor.isNull())
    {
        SetValue(mUI.findResult, tr("No results found."));
        return;
    }
    SetValue(mUI.findResult, QString(""));
    // find returns with a selection, so no need to move the cursor and play
    // with the anchor. insertText will delete the selection
    cursor.insertText(replacement);
    mUI.code->setTextCursor(cursor);
}
void ScriptWidget::on_btnReplaceAll_clicked()
{
    QString text = GetValue(mUI.findText);
    if (text.isEmpty())
        return;
    QString replacement = GetValue(mUI.replaceText);

    QTextDocument::FindFlags flags;
    flags.setFlag(QTextDocument::FindBackward, GetValue(mUI.findBackwards));
    flags.setFlag(QTextDocument::FindCaseSensitively, GetValue(mUI.findCaseSensitive));
    flags.setFlag(QTextDocument::FindWholeWords, GetValue(mUI.findWholeWords));

    int count = 0;

    QTextCursor cursor(&mDocument);
    while (!cursor.isNull())
    {
        cursor = mDocument.find(text, cursor, flags);
        if (cursor.isNull())
            break;

        //cursor.movePosition(QTextCursor::MoveOperation::WordRight, QTextCursor::MoveMode::KeepAnchor);
        // find returns with a selection, so no need to move the cursor and play
        // with the anchor. insertText will delete the selection
        cursor.insertText(replacement);
        count++;
    }
    SetValue(mUI.findResult, QString("Replaced %1 occurrences.").arg(count));
}

void ScriptWidget::on_btnNavBack_clicked()
{
    mUI.textBrowser->backward();
}
void ScriptWidget::on_btnNavForward_clicked()
{
    mUI.textBrowser->forward();
}
void ScriptWidget::on_btnRejectReload_clicked()
{
    SetEnabled(mUI.code, true);
    SetEnabled(mUI.actionFindText, true);
    SetEnabled(mUI.actionReplaceText, true);
    SetEnabled(mUI.actionSave, true);
    SetVisible(mUI.modified, false);
}
void ScriptWidget::on_btnAcceptReload_clicked()
{
    LoadDocument(mFilename);
    SetEnabled(mUI.code, true);
    SetEnabled(mUI.actionFindText, true);
    SetEnabled(mUI.actionReplaceText, true);
    SetEnabled(mUI.actionSave, true);
    SetVisible(mUI.modified, false);
}

void ScriptWidget::on_btnResetFont_clicked()
{
    SetValue(mUI.editorFontName, -1);
    SetValue(mUI.editorFontSize, -1);
    mUI.code->ResetFontSize();
    mUI.code->ResetFontName();
    mUI.code->ApplySettings();
}

void ScriptWidget::on_btnSettingsClose_clicked()
{
    SetVisible(mUI.settings, false);
    SetEnabled(mUI.actionSettings, true);
}

void ScriptWidget::on_editorFontName_currentIndexChanged(int index)
{
    // Qt bollocks, this signal is emitted *before* the current value
    // of the combobox object is changed (except when idnex is -1, huhu!??!).
    // So we must use the index value coming in the signal to get the right font setting,
    // except that I don't know how to get the font at the index ????
    // use a f*n timer to WAR this.
    QTimer::singleShot(0, [this]() {
        if (mUI.editorFontName->currentIndex() == -1)
            mUI.code->ResetFontName();
        else mUI.code->SetFontName(GetValue(mUI.editorFontName));
        mUI.code->ApplySettings();
    });
}
void ScriptWidget::on_editorFontSize_currentIndexChanged(int)
{
    if (mUI.editorFontSize->currentIndex() == -1)
        mUI.code->ResetFontSize();
    else mUI.code->SetFontSize(GetValue(mUI.editorFontSize));
    mUI.code->ApplySettings();
}

void ScriptWidget::on_filter_textChanged(const QString& text)
{
    mLuaHelpFilter.SetFindFilter(text);
    mLuaHelpFilter.invalidate();

    auto* model = mUI.tableView->selectionModel();
    model->reset();
}
void ScriptWidget::on_textBrowser_backwardAvailable(bool available)
{
    mUI.btnNavBack->setEnabled(available);
}
void ScriptWidget::on_textBrowser_forwardAvailable(bool available)
{
    mUI.btnNavForward->setEnabled(available);
}

void ScriptWidget::FileWasChanged()
{
    DEBUG("File change was detected. [file='%1']", mFilename);

    // watch for further modifications
    mWatcher.addPath(mFilename);

    // if already notifying then ignore.
    if (mUI.modified->isVisible())
        return;

    // block recursive signals from happening, i.e.
    // if we've already reacted to this signal and opened the dialog
    // then don't open yet another dialog.
    QSignalBlocker blocker(&mWatcher);

    // our hash is computed on save and load. if the hash of the
    // file contents is now something else then someone else has
    // changed the file somewhere else.
    QFile io;
    io.setFileName(mFilename);
    io.open(QIODevice::ReadOnly);
    if (!io.isOpen())
    {
        // todo: the file could have been removed or renamed.
        ERROR("Failed to open file for reading. [file='%1', error='%2']", mFilename, io.error());
        return;
    }
    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    const size_t hash = qHash(stream.readAll());
    if (hash == mFileHash)
        return;

    // Disable code editing until the reload request has been dismissed
    SetEnabled(mUI.code, false);
    SetEnabled(mUI.actionFindText, false);
    SetEnabled(mUI.actionReplaceText, false);
    SetEnabled(mUI.actionSave, false);
    SetVisible(mUI.modified, true);
}

void ScriptWidget::keyPressEvent(QKeyEvent *key)
{
    if (key->key() == Qt::Key_Escape ||
       (key->key() == Qt::Key_G && key->modifiers() & Qt::ControlModifier))
    {
        OnEscape();
        return;
    }
    QWidget::keyPressEvent(key);
}
bool ScriptWidget::eventFilter(QObject* destination, QEvent* event)
{
    const auto count = mLuaHelpFilter.rowCount();

    // returning true will eat the event and stop from
    // other event handlers ever seeing the event.
    if (destination != mUI.filter)
        return false;
    else if (event->type() != QEvent::KeyPress)
        return false;
    if (count == 0)
        return false;

    const auto* key = static_cast<QKeyEvent*>(event);
    const bool ctrl = key->modifiers() & Qt::ControlModifier;
    const bool shift = key->modifiers() & Qt::ShiftModifier;
    const bool alt   = key->modifiers() & Qt::AltModifier;

    int current = 0;
    auto* model = mUI.tableView->selectionModel();
    const auto& selection = model->selectedRows();
    if (!selection.isEmpty())
        current = selection[0].row();

    // Ctrl+N, Ctrl+P (similar to DlgOpen) don't work here reliable
    // since they are global hot keys which take precedence.

    if (ctrl && key->key() == Qt::Key_N)
        current = math::wrap(0, count-1, current+1);
    else if (ctrl && key->key() == Qt::Key_P)
        current = math::wrap(0, count-1, current-1);
    else if (key->key() == Qt::Key_Up)
        current = math::wrap(0, count-1, current-1);
    else if (key->key() == Qt::Key_Down)
        current = math::wrap(0, count-1, current+1);
    else return false;

    const auto index = selection.isEmpty() ? 0 : current;

    model->setCurrentIndex(mLuaHelpData.index(index, 0),
                           QItemSelectionModel::SelectionFlag::ClearAndSelect |
                           QItemSelectionModel::SelectionFlag::Rows);
    return true;
}

bool ScriptWidget::LoadDocument(const QString& file)
{
    QFile io;
    io.setFileName(file);
    io.open(QIODevice::ReadOnly);
    if (!io.isOpen())
    {
        ERROR("Failed to open script file for reading. [file='%1', error=%2]", file, io.error());
        return false;
    }
    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    const QString& data = stream.readAll();

    QSignalBlocker s(mDocument);

    mDocument.setPlainText(data);
    mFileHash = qHash(data);
    mFilename = file;

    mLuaParser->CleanState();
    mLuaParser->ParseSource(mDocument);

    DEBUG("Loaded script file. [file='%1']", mFilename);
    return true;
}

void ScriptWidget::FilterHelp(const QString& keyword)
{
    SetValue(mUI.filter, keyword);
    mLuaHelpFilter.SetFindFilter(keyword);
    mLuaHelpFilter.invalidate();

    auto* model = mUI.tableView->selectionModel();
    model->reset();

    // If there's only 1 good match then jump to that directly
    // for convenience.
    if (const auto count = GetCount(mUI.tableView))
        SelectRow(mUI.tableView, 0);
}

void ScriptWidget::TableSelectionChanged(const QItemSelection&, const QItemSelection&)
{
    const auto& indices = GetSelection(mUI.tableView);
    for (const auto& index : indices)
    {
        const auto& method = app::GetLuaMethodDoc(index.row());
        const auto& anchor = app::GenerateLuaDocHtmlAnchor(method);
        mUI.textBrowser->scrollToAnchor(anchor);
    }
}

void ScriptWidget::SetInitialFocus()
{
    QTextCursor cursor = mUI.code->textCursor();
    cursor.movePosition(QTextCursor::Start);
    mUI.code->setTextCursor(cursor);
    mUI.code->setFocus();
    mUI.code->ApplySettings();
    if (mSettings.editor_settings.highlight_syntax)
        mLuaParser->ApplyHighlight(mDocument);
}

void ScriptWidget::DocumentEdited(int position, int chars_removed, int chars_added)
{
    //DEBUG("Document edited at %1, removed = %2, added = %3", position, chars_removed, chars_added);

    // this happens when selecting and dragging text to some other location.
    // don't know how to handle this properly yet. How can a single position
    // be used for both removal and addition ???
    if (chars_removed && chars_added)
    {
        mLuaParser->CleanState();
        mLuaParser->ParseSource(mDocument);
    }
    else if (chars_removed || chars_added)
        mLuaParser->EditSource(mDocument, (uint32_t)position, (uint32_t)chars_removed, (uint32_t)chars_added);
}

void ScriptWidget::ResourceRemoved(const app::Resource* resource)
{
    if (resource->GetId() == mResourceID)
    {
        mResourceID.clear();
    }
}

// static
void ScriptWidget::SetDefaultSettings(const Settings& settings)
{
    mSettings = settings;
    for (auto* widget : all_script_widgets)
    {
        widget->mUI.code->SetSettings(settings.editor_settings);
        widget->mUI.code->ApplySettings();
        widget->mLuaParser->SetCodeCompletionHeuristics(settings.use_code_heuristics);
        if (settings.editor_settings.highlight_syntax)
            widget->mLuaParser->ApplyHighlight(widget->mDocument);
        else widget->mLuaParser->RemoveHighlight(widget->mDocument);
    }
}
// static
void ScriptWidget::GetDefaultSettings(Settings* settings)
{
    *settings = mSettings;
}

} // namespace
