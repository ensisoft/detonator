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
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/app/process.h"
#include "editor/app/lua-doc.h"
#include "editor/gui/settings.h"
#include "editor/gui/scriptwidget.h"

namespace gui
{
// static
ScriptWidget::Settings ScriptWidget::mSettings;

class ScriptWidget::TableModel : public QAbstractTableModel
{
public:
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& doc = app::GetLuaMethodDoc(index.row());
        if (role == Qt::DisplayRole) {
            if (index.column() == 0) return app::FromUtf8(doc.table);
            else if (index.column() == 1) return app::toString(doc.type);
            else if (index.column() == 2) return app::FromUtf8(doc.name);
            else if (index.column() == 3) return app::FromUtf8(doc.desc);
        } else if (role == Qt::DecorationRole && index.column() == 1)  {
            if (doc.type == app::LuaMemberType::Function ||
                doc.type == app::LuaMemberType::Method  ||
                doc.type == app::LuaMemberType::MetaMethod)
                return QIcon("icons:function.png");
            else return QIcon("icons:bullet_red.png");
        }
        return {};
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
            if (section == 0) return "Table";
            else if (section == 1) return "Type";
            else if (section == 2) return "Member";
            else if (section == 3) return "Desc";
        }
        return {};
    }
    virtual int rowCount(const QModelIndex&) const override
    { return app::GetNumLuaMethodDocs(); }
    virtual int columnCount(const QModelIndex&) const override
    { return 4; }
private:
};
class ScriptWidget::TableModelProxy : public QSortFilterProxyModel
{
public:
    void SetTableModel(TableModel* model)
    { mModel = model; }
    void SetFilterString(const QString& text)
    {
        mFilter = base::ToUpperUtf8(app::ToUtf8(text));
    }
protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        if (mFilter.empty())
            return true;
        const auto& doc = app::GetLuaMethodDoc(row);
        if (base::Contains(base::ToUpperUtf8(doc.name), mFilter) ||
            base::Contains(base::ToUpperUtf8(doc.desc), mFilter) ||
            base::Contains(base::ToUpperUtf8(doc.table), mFilter))
            return true;
        return false;
    }
private:
    std::string mFilter;
    TableModel* mModel = nullptr;
};

ScriptWidget::ScriptWidget(app::Workspace* workspace)
{
    app::InitLuaDoc();

    mWorkspace = workspace;
    mTableModel.reset(new TableModel);
    mTableModelProxy.reset(new TableModelProxy);
    mTableModelProxy->setSourceModel(mTableModel.get());
    mTableModelProxy->SetTableModel(mTableModel.get());
    QPlainTextDocumentLayout* layout = new QPlainTextDocumentLayout(&mDocument);
    layout->setParent(this);
    mDocument.setDocumentLayout(layout);
    DEBUG("Create ScriptWidget");

    mUI.setupUi(this);
    //mUI.actionFindText->setShortcut(QKeySequence::Find); // use custom

    const auto font_sizes = QFontDatabase::standardSizes();
    for (int size : font_sizes)
    {
        QSignalBlocker s(mUI.editorFontSize);
        mUI.editorFontSize->addItem(QString::number(size));
    }
    SetValue(mUI.editorFontName, -1);
    SetValue(mUI.editorFontSize, -1);
    SetValue(mUI.chkFormatOnSave, Qt::PartiallyChecked);

    mUI.formatter->setVisible(false);
    mUI.modified->setVisible(false);
    mUI.find->setVisible(false);
    mUI.code->SetDocument(&mDocument);
    mUI.tableView->setModel(mTableModelProxy.get());
    mUI.tableView->setColumnWidth(0, 100);
    mUI.tableView->setColumnWidth(2, 200);
    // capture some special key presses in order to move the
    // selection (current item) in the list widget more conveniently.
    mUI.filter->installEventFilter(this);

    connect(mUI.tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &ScriptWidget::TableSelectionChanged);
    connect(&mWatcher, &QFileSystemWatcher::fileChanged, this, &ScriptWidget::FileWasChanged);

    std::map<std::string, std::set<std::string>> table_methods;
    for (size_t i=0; i<app::GetNumLuaMethodDocs(); ++i)
    {
        const auto& method = app::GetLuaMethodDoc(i);
        table_methods[method.table].insert(method.name);
    }

    QString html;
    QTextStream stream(&html);
    stream << R"(
<!DOCTYPE html>
<html>
  <head>
    <meta name="qrichtext"/>
    <title>Lua API</title>
    <style type="text/css">
    body {
      font-size: 16px;
    }
    div {
      margin:0px;
    }
    div.method {
      margin-bottom: 20px;
    }
    div.description {
        margin-bottom: 10px;
        margin-left: 0px;
        word-wrap: break-word;
    }
    div.signature {
        font-family: monospace;
    }
    span.return {
       font-weight: bold;
       color: DarkRed;
    }
    span.method {
       font-style: italic;
       font-weight: bold;
    }
    span.arg {
       font-weight: bold;
       color: DarkRed;
    }
    span.table {
       font-size: 20px;
       font-weight: bold;
       font-style: italic;
    }
  </style>
  </head>
  <body>
)";

    // build TOC with unordered lists.
    stream << "<ul>\n";
    for (const auto& pair : table_methods)
    {
        const auto& table   = app::FromUtf8(pair.first);
        const auto& methods = pair.second;
        stream << QString("<li id=\"%1\">%1</li>\n").arg(table);
        stream << QString("<ul>\n");
        for (const auto& m : methods)
        {
            const auto& method_name   = app::FromUtf8(m);
            const auto& method_anchor = QString("%1_%2")
                    .arg(table)
                    .arg(method_name);
            stream << QString(R"(<li><a href="#%1">%2</a></li>)")
                    .arg(method_anchor)
                    .arg(method_name);
            stream << "\n";
        }
        stream << QString("</ul>\n");
    }
    stream << "</ul>\n";

    std::string current_table;

    // build method documentation bodies.
    for (size_t i=0; i<app::GetNumLuaMethodDocs(); ++i)
    {
        const auto& member = app::GetLuaMethodDoc(i);
        if (member.table != current_table)
        {
            stream << QString("<br><span class=\"table\">%1</span><hr>").arg(app::FromUtf8(member.table));
            current_table = member.table;
        }

        if (member.type == app::LuaMemberType::Function ||
            member.type == app::LuaMemberType::Method ||
            member.type == app::LuaMemberType::MetaMethod)
        {
            QString method_args;
            for (const auto& a : member.args)
            {
                method_args.append("<span class=\"arg\">");
                method_args.append(app::ParseLuaDocTypeString(a.type));
                method_args.append("</span> ");
                method_args.append(app::FromUtf8(a.name));
                method_args.append(", ");
            }
            if (!method_args.isEmpty())
                method_args.chop(2);

            std::string name;
            if (member.type == app::LuaMemberType::Function)
                name = member.table + "." + member.name;
            else if (member.type == app::LuaMemberType::Method)
            {
                if (member.name == "new")
                    name = member.table + ":new";
                else  name = "obj:" + member.name;
            }
            else name = member.name;

            const auto& method_html_name = app::toString("%1_%2", member.table, member.name);
            const auto& method_html_anchor = app::toString("%1_%2", member.table, member.name);
            const auto& method_return = app::ParseLuaDocTypeString(member.ret);
            const auto& method_desc = app::FromUtf8(member.desc);
            const auto& method_name = app::FromUtf8(name);

            stream << QString(
R"(<div class="method" name="%1" id="%2">
  <div class="signature">
     <span class="return">%3 </span>
     <span class="method">%4</span>(%5)
  </div>
  <div class="description">%6</div>
</div>
)").arg(method_html_name)
   .arg(method_html_anchor)
   .arg(method_return)
   .arg(app::FromUtf8(name))
   .arg(method_args)
   .arg(method_desc);
        }
        else
        {
            std::string name;
            if (member.type == app::LuaMemberType::TableProperty)
                name = member.table + "." + member.name;
            else name = "obj." + member.name;

            const auto& prop_html_name = app::toString("%1_%2", member.table, member.name);
            const auto& prop_html_anchor = app::toString("%1_%2", member.table, member.name);
            const auto& prop_return = app::ParseLuaDocTypeString(member.ret);
            const auto& prop_desc = app::FromUtf8(member.desc);
            const auto& prop_name= app::FromUtf8(name);

            stream << QString(
R"(<div class="member" name="%1" id="%2">
   <div class="signature">
      <span class="return">%3 </span>
      <span class="method">%4 </span>
   </div>
   <div class="description">%5</div>
</div>
)").arg(prop_html_name)
   .arg(prop_html_anchor)
   .arg(prop_return)
   .arg(prop_name)
   .arg(prop_desc);
        }
    } // for

    stream << R"(
</body>
</html>
)";

    stream.flush();
    //mUI.textEdit->setHtml(html);
    mUI.textBrowser->setHtml(html);

    QTimer::singleShot(10, this, &ScriptWidget::SetInitialFocus);
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
    LoadDocument(mFilename);

    bool show_settings = true;
    GetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    GetUserProperty(resource, "help_splitter", mUI.helpSplitter);
    GetUserProperty(resource, "show_settings", &show_settings);
    SetVisible(mUI.settings, show_settings);
    SetEnabled(mUI.actionSettings, !show_settings);

    QString font_name;
    if (GetUserProperty(resource, "font_name", &font_name))
    {
        mUI.code->SetFontName(font_name);
        SetValue(mUI.editorFontName, font_name);
    }

    QString font_size = 0;
    if (GetUserProperty(resource, "font_size", &font_size))
    {
        mUI.code->SetFontSize(font_size.toInt());
        SetValue(mUI.editorFontSize, font_size);
    }

    mUI.code->ApplySettings();

    bool format_on_save = false;
    if (GetProperty(resource, "format_on_save", &format_on_save))
        SetValue(mUI.chkFormatOnSave, format_on_save);
}
ScriptWidget::~ScriptWidget()
{
    DEBUG("Destroy ScriptWidget");
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
    bar.addSeparator();
    bar.addAction(mUI.actionFindHelp);
    bar.addSeparator();
    bar.addAction(mUI.actionSettings);
}
void ScriptWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionPreview);
    menu.addSeparator();
    menu.addAction(mUI.actionFindText);
    menu.addAction(mUI.actionReplaceText);
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
    settings.GetValue("Script", "resource_id", &mResourceID);
    settings.GetValue("Script", "resource_name", &mResourceName);
    settings.GetValue("Script", "filename", &mFilename);
    settings.GetValue("Script", "show_settings", &show_settings);
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

    QString font_size = 0;
    if (settings.GetValue("Script", "font_size", &font_size))
    {
        mUI.code->SetFontSize(font_size.toInt());
        SetValue(mUI.editorFontSize, font_size);
    }

    bool format_on_save = false;
    if (settings.GetValue("Script", "format_on_save", &format_on_save))
        SetValue(mUI.chkFormatOnSave, format_on_save);

    mUI.code->ApplySettings();

    if (mFilename.isEmpty())
        return true;
    mWatcher.addPath(mFilename);
    return LoadDocument(mFilename);
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
    mUI.find->setVisible(false);
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
            ERROR("Failed to open '%1' for writing. (%2)", filename, file.error());
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
        QTextCursor cursor = mUI.code->textCursor();
        auto cursor_position = cursor.position();
        auto scroll_position = mUI.code->verticalScrollBar()->value();

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
        if (!app::Process::RunAndCapture(exec, "", arg_list, &stdout_buffer, &stderr_buffer))
        {
            ERROR("Failed to run Lua code formatter.");
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
            cursor = mUI.code->textCursor();
            cursor.setPosition(cursor_position);
            mUI.code->setTextCursor(cursor);
            mUI.code->verticalScrollBar()->setValue(scroll_position);
            //mUI.code->ensureCursorVisible();
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
    }
    else
    {
        auto* resource = mWorkspace->FindResourceById(mResourceID);
        SetUserProperty(*resource, "main_splitter", mUI.mainSplitter);
        SetUserProperty(*resource, "help_splitter", mUI.helpSplitter);
        SetUserProperty(*resource, "show_settings", mUI.settings->isVisible());

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

void ScriptWidget::on_editorFontName_currentIndexChanged(int)
{
    if (mUI.editorFontName->currentIndex() == -1)
        mUI.code->ResetFontName();
    else mUI.code->SetFontName(mUI.editorFontName->currentFont().toString());
    mUI.code->ApplySettings();
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
    mTableModelProxy->SetFilterString(text);
    mTableModelProxy->invalidate();

    const auto count = mTableModelProxy->rowCount();
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
        mUI.find->setVisible(false);
        mUI.formatter->setVisible(false);
        mUI.code->setFocus();
        return;
    }
    QWidget::keyPressEvent(key);
}
bool ScriptWidget::eventFilter(QObject* destination, QEvent* event)
{
    const auto count = mTableModelProxy->rowCount();

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

    model->setCurrentIndex(mTableModel->index(index, 0),
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

    mDocument.setPlainText(data);
    mFileHash = qHash(data);
    mFilename = file;
    DEBUG("Loaded script file. [file='%1']", mFilename);
    return true;
}

void ScriptWidget::TableSelectionChanged(const QItemSelection, const QItemSelection&)
{
    const auto& indices = GetSelection(mUI.tableView);
    for (const auto& index : indices)
    {
        const auto& method = app::GetLuaMethodDoc(index.row());
        const auto& name = QString("%1_%2").arg(app::FromUtf8(method.table)).arg(app::FromUtf8(method.name));
        mUI.textBrowser->scrollToAnchor(name);
        //DEBUG("Scroll to anchor. [anchor='%1']", name);
    }
}

void ScriptWidget::SetInitialFocus()
{
    QTextCursor cursor = mUI.code->textCursor();
    cursor.movePosition(QTextCursor::Start);
    mUI.code->setTextCursor(cursor);
    mUI.code->setFocus();
}

// static
void ScriptWidget::SetDefaultSettings(const Settings& settings)
{
    mSettings = settings;
}
// static
void ScriptWidget::GetDefaultSettings(Settings* settings)
{
    *settings = mSettings;
}

} // namespace
