// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgcurveparams.h"
#  include <QDialog>
#include "warnpop.h"

#include "game/fwd.h"

namespace gui
{
    // Edit the rendering parameters (joint style, end cap style)
    // of a curve class. The changes are applied to the curve class
    // right away when the user changes the values in the dialog.
    // The dialog is a popup that closes when the user clicks outside
    // of it so no accept/reject buttons are needed.
    class DlgCurveParams : public QDialog
    {
        Q_OBJECT

    public:
        DlgCurveParams(QWidget* parent, game::CurveClass* curve);

    private slots:
        void on_cmbJointStyle_currentIndexChanged(int);
        void on_cmbEndCapStyle_currentIndexChanged(int);
        void on_width_valueChanged(bool, double value);

    private:
        Ui::DlgCurveParams mUI;
    private:
        game::CurveClass* mCurve = nullptr;
    };

} // namespace
