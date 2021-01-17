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
        ScriptWidget(app::Workspace* workspace);
        ScriptWidget(app::Workspace* workspace, const app::Resource& resource);
       ~ScriptWidget();

        virtual bool IsAccelerated() const override
        { return false; }
        virtual bool CanTakeAction(Actions action, const Clipboard*) const override;
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipboard) const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool ConfirmClose() override;

    private slots:
        void on_actionSave_triggered();
        void on_actionOpen_triggered();
        void on_actionFind_triggered();
        void on_actionReplace_triggered();
        void on_btnFindNext_clicked();
        void on_btnFindClose_clicked();
        void on_btnReplaceNext_clicked();
        void on_btnReplaceAll_clicked();

        void FileWasChanged();
    private:
        virtual void keyPressEvent(QKeyEvent* key) override;
        bool LoadDocument(const QString& file);
    private:
        Ui::ScriptWidget mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        QFileSystemWatcher mWatcher;
        QTextDocument mDocument;
        QTextCursor   mCursor;
        QString mFilename;
        QString mResourceID;
        std::size_t mFileHash = 0;
    };

} // namespace

