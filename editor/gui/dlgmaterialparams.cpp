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

#include "warnpush.h"
#include "warnpop.h"

#include "editor/gui/dlgmaterialparams.h"
#include "editor/gui/utility.h"
#include "graphics/material.h"
#include "game/entity.h"

namespace gui
{

DlgMaterialParams::DlgMaterialParams(QWidget* parent, app::Workspace* workspace, game::DrawableItemClass* item)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mItem(item)
  , mParams(item->GetMaterialParams())
{
    mUI.setupUi(this);
    SetVisible(mUI.builtInParams, false);
    SetVisible(mUI.lblStatic, false);

    auto material = mWorkspace->GetMaterialClassById(app::FromUtf8(item->GetMaterialId()));
    if (auto* builtin = material->AsBuiltIn())
    {
        SetVisible(mUI.builtInParams, true);
        if (builtin->IsStatic())
        {
            SetVisible(mUI.lblStatic, true);
        }
        else if (const auto* param = item->FindMaterialParam("kBaseColor"))
        {
            if (const auto* color = std::get_if<game::Color4f>(param))
                mUI.baseColor->setColor(FromGfx(*color));
        }
    }
}

void DlgMaterialParams::on_btnResetBaseColor_clicked()
{
    mUI.baseColor->clearColor();
    mItem->DeleteMaterialParam("kBaseColor");
}

void DlgMaterialParams::on_baseColor_colorChanged(QColor color)
{
    mItem->SetMaterialParam("kBaseColor", ToGfx(color));
}

void DlgMaterialParams::on_btnAccept_clicked()
{
    if (mUI.baseColor->hasColor())
         mItem->SetMaterialParam("kBaseColor", ToGfx(mUI.baseColor->color()));
    else mItem->DeleteMaterialParam("kBaseColor");

    accept();
}

void DlgMaterialParams::on_btnCancel_clicked()
{
    // put back the old values.
    mItem->SetMaterialParams(std::move(mParams));
    reject();
}

} // namespace