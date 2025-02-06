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

#include <memory>

#include "warnpush.h"
#  include "ui_scriptwidget.h"
#  include <QTextDocument>
#  include <QFileSystemWatcher>
#  include <QDialog>
#  include <QUrl>
#include "warnpop.h"

#include "editor/app/lua-doc.h"
#include "editor/gui/codewidget.h"
#include "editor/gui/mainwidget.h"

namespace app {
    class Resource;
    class Workspace;
    class CodeAssistant;
} // namespace

namespace gui
{
    class ScriptWidget : public MainWidget
    {
        Q_OBJECT
    public:
        struct Settings {
            QString theme = "Monokai";
            QString lua_formatter_exec = "${install-dir}/lua-format/lua-format";
            QString lua_formatter_args = "${file} -i --config=${install-dir}/lua-format/style.yaml";
            bool lua_format_on_save    = true;
            bool use_code_heuristics   = true;
            TextEditor::Settings editor_settings;
        };

        ScriptWidget(app::Workspace* workspace);
        ScriptWidget(app::Workspace* workspace, const app::Resource& resource);
       ~ScriptWidget();

        virtual QString GetId() const override;
        virtual bool IsAccelerated() const override
        { return false; }
        virtual bool HasStats() const override
        { return false; }
        virtual bool CanTakeAction(Actions action, const Clipboard*) const override;
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipboard) const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual void Save() override;
        virtual bool SaveState(gui::Settings& settings) const override;
        virtual bool LoadState(const gui::Settings& settings) override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool OnEscape() override;
        virtual void Activate() override;

        static void SetDefaultSettings(const Settings& settings);
        static void GetDefaultSettings(Settings* settings);

    private slots:
        void on_actionPreview_triggered();
        void on_actionSave_triggered();
        void on_actionOpen_triggered();
        void on_actionFindHelp_triggered();
        void on_actionFindText_triggered();
        void on_actionReplaceText_triggered();
        void on_actionSettings_triggered();
        void on_actionGotoSymbol_triggered();
        void on_actionGoBack_triggered();
        void on_btnFindNext_clicked();
        void on_btnFindClose_clicked();
        void on_btnReplaceNext_clicked();
        void on_btnReplaceAll_clicked();
        void on_btnNavBack_clicked();
        void on_btnNavForward_clicked();
        void on_btnZoomIn_clicked();
        void on_btnZoomOut_clicked();
        void on_btnRejectReload_clicked();
        void on_btnAcceptReload_clicked();
        void on_btnResetFont_clicked();
        void on_btnSettingsClose_clicked();
        void on_editorFontName_currentIndexChanged(int);
        void on_editorFontSize_currentIndexChanged(int);
        void on_filter_textChanged(const QString& text);
        void on_textBrowser_backwardAvailable(bool available);
        void on_textBrowser_forwardAvailable(bool available);

        void FileWasChanged();
        void TableSelectionChanged(const QItemSelection&, const QItemSelection&);
        void SetInitialFocus();
        void DocumentEdited(int position, int chars_removed, int chars_added);

        void ResourceRemoved(const app::Resource* resource);
    private:
        virtual void keyPressEvent(QKeyEvent* key) override;
        virtual bool eventFilter(QObject* destination, QEvent* event) override;
        bool LoadDocument(const QString& file);
        void FilterHelp(const QString& keyword);
    private:
        Ui::ScriptWidget mUI;
    private:
        std::unique_ptr<app::CodeAssistant> mAssistant;
        app::LuaDocTableModel mLuaHelpData;
        app::LuaDocModelProxy mLuaHelpFilter;
        app::Workspace* mWorkspace = nullptr;
        QFileSystemWatcher mWatcher;
        QTextDocument mDocument;
        QString mFilename;
        QString mResourceID;
        QString mResourceName;
        std::size_t mFileHash = 0;
        int mZoom = 0;
        static Settings mSettings;
    };

} // namespace

