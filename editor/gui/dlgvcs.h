// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgvcs.h"
#  include <QTimer>
#  include <QDialog>
#include "warnpop.h"

#include <memory>

namespace app {
    class Workspace;
} // namespace

namespace gui
{
    struct AppSettings;

    class DlgVCS : public QDialog
    {
        Q_OBJECT

    public:
        DlgVCS(QWidget* parent, const app::Workspace* workspace, const gui::AppSettings& settings);
       ~DlgVCS();

    private slots:
        void on_btnClose_clicked();
        void on_btnSync_clicked();

    private:
        void BeginScan();
        void AppendLog(const QString& str);
        bool RunCommand(const QStringList& args,
                        QStringList* stdout_buffer,
                        QStringList* stderr_buffer);
    private:
        Ui::DlgVCS mUI;
        class TableModel;
        std::unique_ptr<TableModel> mModel;
    private:
        const app::Workspace* mWorkspace = nullptr;
        const gui::AppSettings& mSettings;
    };

} // namespace
