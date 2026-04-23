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

#include "game/enum.h"
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
    SetVisible(mUI.btnSetMaterialParams, false);
    SetEnabled(mUI.btnResetMaterial, false);
    SetPaletteType(game::TilemapPaletteType::MultiMaterial);
}

 void PaletteMaterial::ResetMaterial()
 {
     SetValue(mUI.cmbMaterial, -1);
     SetValue(mUI.cmbOcclusion, game::TileOcclusion::None);
     SetEnabled(mUI.btnResetMaterial, false);
 }

void PaletteMaterial::SetMaterial(const app::AnyString& id)
{
    if (mPaletteType != game::TilemapPaletteType::MultiMaterial)
        return;

    const QString& current = GetItemId(mUI.cmbMaterial);
    if (current == id)
        return;

    if (SetValue(mUI.cmbMaterial, ListItemId(id)))
    {
        SetEnabled(mUI.btnResetMaterial, true);
    }
    else
    {
        SetEnabled(mUI.btnResetMaterial, false);
    }

}
void PaletteMaterial::SetTileIndex(unsigned tile_index)
{
    mTileIndex = tile_index;
}

void PaletteMaterial::ResetDrawable()
{
    SetValue(mUI.cmbDrawable, -1);
    SetEnabled(mUI.btnResetDrawable, false);
}
void PaletteMaterial::SetDrawable(const app::AnyString& id)
{
    const QString& current = GetItemId(mUI.cmbDrawable);
    if (current == id)
        return;

    if (SetValue(mUI.cmbDrawable, ListItemId(id)))
    {
        SetEnabled(mUI.btnResetDrawable, true);
    }
    else
    {
        SetEnabled(mUI.btnResetDrawable, false);
    }
}

bool PaletteMaterial::HasSelectedMaterial() const
{
    const auto current = mUI.cmbMaterial->currentIndex();
    if (current == -1)
        return false;

    return true;
}

bool PaletteMaterial::HasSelectedDrawable() const
{
    const auto current = mUI.cmbDrawable->currentIndex();
    if (current == -1)
        return false;

    return true;
}

void PaletteMaterial::UpdateMaterialList(const ResourceList& list)
{
    SetList(mUI.cmbMaterial, list);
}

void PaletteMaterial::UpdateDrawableList(const ResourceList& list)
{
    SetList(mUI.cmbDrawable, list);
}

void PaletteMaterial::SetPreview(const std::shared_ptr<const gfx::IBitmap>& bitmap, const app::AnyString& id)
{
    if (bitmap)
        SetImage(mUI.preview, *bitmap);
    else
        SetImage(mUI.preview, QPixmap());

    mPreviewMaterialId = id;
}

void PaletteMaterial::ClearPreview()
{
    SetImage(mUI.preview, QPixmap());
    mPreviewMaterialId.Clear();
}

void PaletteMaterial::SetPaletteType(game::TilemapPaletteType palette)
{
    if (palette ==game::TilemapPaletteType::Tilemap)
    {
        SetVisible(mUI.cmbDrawable, true);
        SetVisible(mUI.cmbMaterial, false);
        SetVisible(mUI.btnSelectMaterial, false);
        SetVisible(mUI.btnResetMaterial, false);
    }
    else if (palette == game::TilemapPaletteType::MultiMaterial)
    {
        SetVisible(mUI.cmbDrawable, true);
        SetVisible(mUI.cmbMaterial, true);
        SetVisible(mUI.btnResetMaterial, true);
        SetVisible(mUI.btnSelectMaterial, true);
    }
    mPaletteType = palette;
}

void PaletteMaterial::on_btnSelectMaterial_clicked()
{
    // using *this* as the parent to DlgMaterial sometimes fucks up the
    // rendering and the tilemap widget goes blank.
    // This seems similar to previous bug in the UI widget that was
    // fixed in commit  a7c167ca991f9c0a688212f4c926fc32d9cea77a
    DlgMaterial dlg(mParent, mWorkspace, false);

    dlg.SetPreviewScale(mPreviewScale);
    dlg.SetSelectedMaterialId(GetItemId(mUI.cmbMaterial));
    dlg.ShowTileIndex(false);

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
    SetValue(mUI.cmbOcclusion, game::TileOcclusion::None);
    SetEnabled(mUI.btnResetMaterial, false);
    SetImage(mUI.preview, QPixmap());
    emit ValueChanged(this);
}

void PaletteMaterial::on_btnResetDrawable_clicked()
{
    SetValue(mUI.cmbDrawable, -1);
    SetEnabled(mUI.btnResetDrawable, false);
    emit ValueChanged(this);
}

void PaletteMaterial::on_cmbMaterial_currentIndexChanged(int)
{
    SetEnabled(mUI.btnResetMaterial, true);
    emit ValueChanged(this);
}

void PaletteMaterial::on_cmbDrawable_currentIndexChanged(int)
{
    SetEnabled(mUI.btnResetDrawable, true);

    emit ValueChanged(this);
}

void PaletteMaterial::on_cmbOcclusion_currentIndexChanged(int)
{
    emit ValueChanged(this);
}

} // namespace

