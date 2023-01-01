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
#  include "ui_dlgwidgetlist.h"
#  include <QDialog>
#include "warnpop.h"

#include <vector>

#include "uikit/fwd.h"

namespace gui
{
    class DlgWidgetList : public QDialog
    {
        Q_OBJECT

    public:
        DlgWidgetList(QWidget* parent, std::vector<uik::Widget*>& list);

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
    private:
        Ui::DlgWidgetList mUI;
    private:
        std::vector<uik::Widget*>& mList;
    };

} // namespace
