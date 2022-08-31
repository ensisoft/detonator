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
#include "warnpop.h"

#include "editor/gui/mainwidget.h"

namespace app {
    class Resource;
    class Workspace;
} // namespace

namespace gui
{
    class ScriptWidget : public MainWidget
    {
        Q_OBJECT
    public:
        struct Settings {
            QString lua_formatter_exec = "${install-dir}/lua-format/lua-format";
            QString lua_formatter_args = "${file} -i --config=${install-dir}/lua-format/style.yaml";
            bool lua_format_on_save    = true;
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

        static void SetDefaultSettings(const Settings& settings);
        static void GetDefaultSettings(Settings* settings);

    private slots:
        void on_actionSave_triggered();
        void on_actionOpen_triggered();
        void on_actionFindHelp_triggered();
        void on_actionFindText_triggered();
        void on_actionReplaceText_triggered();
        void on_btnFindNext_clicked();
        void on_btnFindClose_clicked();
        void on_btnReplaceNext_clicked();
        void on_btnReplaceAll_clicked();
        void on_btnNavBack_clicked();
        void on_btnNavForward_clicked();
        void on_btnRejectReload_clicked();
        void on_btnAcceptReload_clicked();
        void on_filter_textChanged(const QString& text);
        void on_textBrowser_backwardAvailable(bool available);
        void on_textBrowser_forwardAvailable(bool available);

        void FileWasChanged();
        void TableSelectionChanged(const QItemSelection, const QItemSelection&);
        void SetInitialFocus();
    private:
        virtual void keyPressEvent(QKeyEvent* key) override;
        virtual bool eventFilter(QObject* destination, QEvent* event) override;
        bool LoadDocument(const QString& file);
    private:
        Ui::ScriptWidget mUI;
    private:
        class TableModel;
        class TableModelProxy;
        std::unique_ptr<TableModel> mTableModel;
        std::unique_ptr<TableModelProxy> mTableModelProxy;
        app::Workspace* mWorkspace = nullptr;
        QFileSystemWatcher mWatcher;
        QTextDocument mDocument;
        QTextCursor   mCursor;
        QString mFilename;
        QString mResourceID;
        QString mResourceName;
        std::size_t mFileHash = 0;
        static Settings mSettings;
    };

} // namespace

