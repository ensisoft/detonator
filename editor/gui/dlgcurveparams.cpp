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

#define LOGTAG "gui"

#include "config.h"

#include "game/curve.h"
#include "editor/gui/dlgcurveparams.h"
#include "editor/gui/spinboxwidget.h"
#include "editor/gui/translation.h"
#include "editor/gui/utility.h"

namespace gui
{

DlgCurveParams::DlgCurveParams(QWidget* parent, game::CurveClass* curve)
  : QDialog(parent, Qt::Popup)
  , mCurve(curve)
{
    mUI.setupUi(this);

    PopulateFromEnum<game::CurveJointStyle>(mUI.cmbJointStyle);
    PopulateFromEnum<game::CurveEndCapStyle>(mUI.cmbEndCapStyle);
    SetValue(mUI.cmbJointStyle,  mCurve->GetJointStyle());
    SetValue(mUI.cmbEndCapStyle, mCurve->GetEndCapStyle());
    if (curve->HasWidthOverride())
        SetValue(mUI.width, curve->GetWidthOverride());
}

void DlgCurveParams::on_cmbJointStyle_currentIndexChanged(int)
{
    mCurve->SetJointStyle(GetValue(mUI.cmbJointStyle));
}

void DlgCurveParams::on_cmbEndCapStyle_currentIndexChanged(int)
{
    mCurve->SetEndCapStyle(GetValue(mUI.cmbEndCapStyle));
}

void DlgCurveParams::on_width_valueChanged(bool has_value, double value)
{
    if (has_value)
        mCurve->SetWidthOverride(value);
    else mCurve->ResetWidthOverride();
}

} // namespace gui
