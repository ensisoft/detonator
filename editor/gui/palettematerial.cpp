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

#include "config.h"

#include "editor/app/workspace.h"
#include "editor/gui/palettematerial.h"
#include "editor/gui/dlgmaterial.h"

namespace gui
{
PaletteMaterial::PaletteMaterial(const app::Workspace* workspace, QWidget* parent)
   : QWidget(parent)
   , mWorkspace(workspace)
{
    mUI.setupUi(this);
    SetList(mUI.cmbMaterial, mWorkspace->ListAllMaterials());
    SetEnabled(mUI.btnSetMaterialParams, false);
    SetEnabled(mUI.btnResetMaterial, false);
}

void PaletteMaterial::UpdateMaterialList(const ResourceList& list)
{
    SetList(mUI.cmbMaterial, list);
}

void PaletteMaterial::on_btnSelectMaterial_clicked()
{
    DlgMaterial dlg(this, mWorkspace, GetValue(mUI.cmbMaterial));
    if (dlg.exec() == QDialog::Rejected)
        return;
    SetValue(mUI.cmbMaterial, ListItemId(dlg.GetSelectedMaterialId()));
    SetEnabled(mUI.btnResetMaterial, true);
    emit ValueChanged(this);
}
void PaletteMaterial::on_btnSetMaterialParams_clicked()
{

}
void PaletteMaterial::on_btnResetMaterial_clicked()
{
    SetValue(mUI.cmbMaterial, -1);
    SetEnabled(mUI.btnResetMaterial, false);
    emit ValueChanged(this);
}

void PaletteMaterial::on_cmbMaterial_currentIndexChanged(int)
{
    SetEnabled(mUI.btnResetMaterial, true);
    emit ValueChanged(this);
}



} // namespace

