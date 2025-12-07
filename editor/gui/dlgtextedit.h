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

#include "warnpush.h"
#  include <QWidget>
#  include <QColor>
#  include <QDialog>
#  include <QTextDocument>
#  include <QSyntaxHighlighter>
#  include "ui_dlgtextedit.h"
#include "warnpop.h"

#include <string>
#include <memory>
#include <vector>
#include <functional>

#include "app/types.h"
#include "editor/gui/fudialog.h"

namespace gui
{
    class DlgTextEdit : public QWidget, public FUDialog
    {
        Q_OBJECT
    public:
        explicit DlgTextEdit(QWidget* parent);
        ~DlgTextEdit() override;

        void SetTitle(const QString& str);
        void SetText(const app::AnyString& str, const std::string& format);
        app::AnyString GetText() const;
        app::AnyString GetText(const std::string& format) const;

        void SetReadOnly(bool readonly);
        void EnableSaveApply();
        void ShowError(const QString& message);
        void ClearError();

        std::function<void()> applyFunction;

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnApply_clicked();
    private:
        bool OnCloseEvent()  override;
        bool CheckForClose();
    private:
        Ui::DlgTextEdit* mUI = nullptr;
    private:
        QTextDocument mDocument;
        QSyntaxHighlighter* mSyntaxHighlight = nullptr;
        bool mPendingChanges = false;
    };
} // namespace
