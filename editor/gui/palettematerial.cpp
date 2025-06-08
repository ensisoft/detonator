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

#include <QPixmap>

#include "game/enum.h"
#include "graphics/material_class.h"
#include "editor/app/workspace.h"
#include "editor/gui/palettematerial.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"
#include "editor/gui/translation.h"

namespace gui
{
PaletteMaterial::PaletteMaterial(const app::Workspace* workspace, QWidget* parent)
   : QWidget(parent)
   , mWorkspace(workspace)
   , mParent(parent)
{
    mUI.setupUi(this);
    PopulateFromEnum<game::TileOcclusion>(mUI.cmbOcclusion);

    SetEnabled(mUI.btnSetMaterialParams, false);
    SetEnabled(mUI.btnResetMaterial, false);
}

 void PaletteMaterial::ResetMaterial()
 {
     SetValue(mUI.cmbMaterial, -1);
     SetValue(mUI.cmbOcclusion, game::TileOcclusion::None);
     SetEnabled(mUI.btnResetMaterial, false);
     SetImage(mUI.preview, QPixmap());
 }

void PaletteMaterial::SetMaterial(const app::AnyString& id)
{
    const QString& current = GetItemId(mUI.cmbMaterial);
    if (current == id)
        return;

    if (SetValue(mUI.cmbMaterial, ListItemId(id)))
    {
        SetEnabled(mUI.btnResetMaterial, true);
        UpdatePreview(id);
    }
    else
    {
        SetEnabled(mUI.btnResetMaterial, false);
        SetImage(mUI.preview, QPixmap());
    }
}
void PaletteMaterial::SetTileIndex(unsigned tile_index)
{
    const  unsigned current = GetValue(mUI.tileIndex);
    if (current == tile_index)
        return;

    SetValue(mUI.tileIndex, tile_index);

    // todo: preview update
}

bool PaletteMaterial::HasSelectedMaterial() const
{
    const auto current = mUI.cmbMaterial->currentIndex();
    if (current == -1)
        return false;

    return true;
}

void PaletteMaterial::UpdateMaterialList(const ResourceList& list)
{
    SetList(mUI.cmbMaterial, list);
}

void PaletteMaterial::UpdatePreview(const app::AnyString& id)
{
    SetImage(mUI.preview, QPixmap());

    const auto& klass = mWorkspace->FindMaterialClassById(id);
    if (!klass)
        return;

    const auto map_count = klass->GetNumTextureMaps();
    if (map_count == 0)
        return;

    const auto* texture = klass->GetTextureMap(0);
    if (!texture->GetNumTextures())
        return;

    // todo: texture rect computation
    // todo: tilemap tile computation inside texture rect

    const auto* texture_source = texture->GetTextureSource(0);
    const auto& texture_data = texture_source->GetData();
    if (texture_data)
    {
        SetImage(mUI.preview, *texture_data);
    }
}

void PaletteMaterial::on_btnSelectMaterial_clicked()
{
    // using *this* as the parent to DlgMaterial sometimes fucks up the
    // rendering and the tilemap widget goes blank.
    // This seems similar to previous bug in the UI widget that was
    // fixed in commit  a7c167ca991f9c0a688212f4c926fc32d9cea77a
    DlgMaterial dlg(mParent, mWorkspace);

    dlg.SetPreviewScale(mPreviewScale);
    dlg.SetMaterialId(GetItemId(mUI.cmbMaterial));
    dlg.SetTileIndex(GetValue(mUI.tileIndex));

    if (dlg.exec() == QDialog::Rejected)
        return;
    SetValue(mUI.cmbMaterial, ListItemId(dlg.GetSelectedMaterialId()));
    SetValue(mUI.tileIndex, dlg.GetTileIndex());
    SetEnabled(mUI.btnResetMaterial, true);
    UpdatePreview(dlg.GetSelectedMaterialId());

    emit ValueChanged(this);
}
void PaletteMaterial::on_btnSetMaterialParams_clicked()
{

}
void PaletteMaterial::on_btnResetMaterial_clicked()
{
    SetValue(mUI.cmbMaterial, -1);
    SetValue(mUI.tileIndex, 0);
    SetValue(mUI.cmbOcclusion, game::TileOcclusion::None);
    SetEnabled(mUI.btnResetMaterial, false);
    SetImage(mUI.preview, QPixmap());
    emit ValueChanged(this);
}

void PaletteMaterial::on_cmbMaterial_currentIndexChanged(int)
{
    SetEnabled(mUI.btnResetMaterial, true);
    UpdatePreview(GetItemId(mUI.cmbMaterial));

    emit ValueChanged(this);
}

void PaletteMaterial::on_tileIndex_valueChanged(int)
{
    emit ValueChanged(this);
}

void PaletteMaterial::on_cmbOcclusion_currentIndexChanged(int)
{
    emit ValueChanged(this);
}

} // namespace

