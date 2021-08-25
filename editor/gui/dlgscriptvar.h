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
#  include "ui_dlgscriptvar.h"
#  include <QDialog>
#include "warnpop.h"

#include "game/fwd.h"

namespace gui
{
    class DlgScriptVar :  public QDialog
    {
        Q_OBJECT
    public:
        DlgScriptVar(QWidget* parent, game::ScriptVar& variable);

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_varType_currentIndexChanged(int);
    private:
        Ui::DlgScriptVar mUI;
    private:
        game::ScriptVar& mVar;
    };
} // namespace