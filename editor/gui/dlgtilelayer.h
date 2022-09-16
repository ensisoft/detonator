// Copyright (C) 2020-2022 Sami Väisänen
// Copyright (C) 2020-2022 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgtilelayer.h"
#  include <QDialog>
#include "warnpop.h"

#include <string>

#include "game/tilemap.h"
#include "editor/app/workspace.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgmaterial.h"

namespace app {
    class Workspace;
} //

namespace gui
{
    class DlgLayer : public QDialog
    {
        Q_OBJECT

    public:
        DlgLayer(const app::Workspace* workspace, QWidget* parent, unsigned map_width, unsigned map_height)
          : QDialog(parent)
          , mWorkspace(workspace)
          , mMapWidth(map_width)
          , mMapHeight(map_height)
        {
            mUI.setupUi(this);
            PopulateFromEnum<game::TilemapLayerClass::Type>(mUI.cmbLayerType);
            PopulateFromEnum<game::TilemapLayerClass::Storage>(mUI.cmbLayerStorage);
            PopulateFromEnum<game::TilemapLayerClass::Cache>(mUI.cmbLayerCache);
            PopulateFromEnum<game::TilemapLayerClass::Resolution>(mUI.cmbLayerResolution);
            SetValue(mUI.cmbLayerStorage,    game::TilemapLayerClass::Storage::Dense);
            SetValue(mUI.cmbLayerCache,      game::TilemapLayerClass::Cache::Automatic);
            SetValue(mUI.cmbLayerResolution, game::TilemapLayerClass::Resolution::Original);
            SetList(mUI.cmbMaterial, mWorkspace->ListAllMaterials());
            SetValue(mUI.cmbMaterial, -1);
            SetRange(mUI.value, -0x800000, 0xFFFFFF); // min is 24 bit signed and max is 24bit unsigned
            on_cmbLayerType_currentIndexChanged(0);
        }
        std::string GetMaterialId() const
        { return GetItemId(mUI.cmbMaterial); }
        std::string GetName() const
        { return GetValue(mUI.layerName); }
        game::TilemapLayerClass::Type GetLayerType() const
        { return GetValue(mUI.cmbLayerType); }
        game::TilemapLayerClass::Storage GetLayerStorage() const
        { return GetValue(mUI.cmbLayerStorage); }
        game::TilemapLayerClass::Cache GetLayerCache() const
        { return GetValue(mUI.cmbLayerCache); }
        game::TilemapLayerClass::Resolution GetLayerResolution() const
        { return GetValue(mUI.cmbLayerResolution); }
        std::int32_t GetDataValue() const
        { return GetValue(mUI.value); }
    private slots:
        void on_btnAccept_clicked()
        {
            if (!MustHaveInput(mUI.layerName))
                return;
            accept();
        }
        void on_btnCancel_clicked()
        {
            reject();
        }
        void on_btnResetMaterial_clicked()
        {
            SetValue(mUI.cmbMaterial, -1);
        }
        void on_btnSelectMaterial_clicked()
        {
            DlgMaterial dlg(this, mWorkspace, GetItemId(mUI.cmbMaterial));
            if (dlg.exec() == QDialog::Rejected)
                return;
            SetValue(mUI.cmbMaterial, ListItemId(dlg.GetSelectedMaterialId()));
        }
        void on_cmbLayerType_currentIndexChanged(int)
        {
            const game::TilemapLayerClass::Type type = GetValue(mUI.cmbLayerType);
            const bool render = game::TilemapLayerClass::HasRenderComponent(type);
            const bool data   = game::TilemapLayerClass::HasDataComponent(type);
            SetEnabled(mUI.cmbMaterial, render);
            SetEnabled(mUI.btnSelectMaterial, render);
            SetEnabled(mUI.value, data);
        }
    private:
        Ui::DlgTileLayer mUI;
    private:
        const app::Workspace* mWorkspace = nullptr;
        const unsigned mMapWidth  = 0;
        const unsigned mMapHeight = 0;
    };
} // namespace
