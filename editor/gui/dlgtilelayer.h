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

#include "base/assert.h"
#include "game/tilemap.h"
#include "editor/app/workspace.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgmaterial.h"

namespace app {
    class Workspace;
} //

namespace game {
    inline std::string TranslateEnum(game::TilemapLayerClass::Storage storage)
    {
        using S = game::TilemapLayerClass::Storage;
        if (storage == S::Dense)
            return "Dense Storage";
        else if (storage == S::Sparse)
            return "Sparse Storage";
        else BUG("Missing translation");
        return "???";
    }

    inline std::string TranslateEnum(game::TilemapLayerClass::Resolution res)
    {
        using R = game::TilemapLayerClass::Resolution;
        if (res == R::Original)
            return "Original Map Resolution (1:1)";
        else if (res == R::DownScale2)
            return "Downscale by 2 (1:2)";
        else if (res == R::DownScale4)
            return "Downscale by 4 (1:4)";
        else if (res == R::DownScale8)
            return "Downscale by 8 (1:8)";
        else if (res == R::UpScale2)
            return "Upscale by 2 (2:1)";
        else if (res == R::UpScale4)
            return "Upscale by 4 (4:1)";
        else if (res == R::UpScale8)
            return "Upscale by 8 (8:1)";
        else BUG("Missing translation");
        return "???";
    }

    inline std::string TranslateEnum(game::TilemapLayerClass::Cache cache)
    {
        using C = game::TilemapLayerClass::Cache;
        if (cache == C::Automatic)
            return "Automatic Cache Size";
        else if (cache == C::Cache8)
            return "Cache 8 Tiles";
        else if (cache == C::Cache16)
            return "Cache 16 Tiles";
        else if (cache == C::Cache32)
            return "Cache 32 Tiles";
        else if (cache == C::Cache64)
            return "Cache 64 Tiles";
        else if (cache == C::Cache128)
            return "Cache 128 Tiles";
        else if (cache == C::Cache256)
            return "Cache 256 Tiles";
        else if (cache == C::Cache512)
            return "Cache 512 Tiles";
        else if (cache == C::Cache1024)
            return "Cache 1024 Tiles";
        else BUG("Missing translation");
        return "???";
    }

    namespace detail {
        inline std::string TranslateEnum(TilemapLayerType type)
        {
            using T = TilemapLayerType;
            if (type == T::Render)
                return "256 Color Render Layer";
            else if (type == T::Render_DataSInt4)
                return "16 Color Render Layer with 4bit Signed Integer Data";
            else if (type == T::Render_DataUInt4)
                return "16 Color Render Layer with 4bit Signed Integer Data";
            else if (type == T::Render_DataSInt8)
                return "256 Color Render Layer with 8bit Signed Integer Data";
            else if (type == T::Render_DataUInt8)
                return "256 Color Render Layer with 8bit Unsigned Integer Data";
            else if (type == T::Render_DataSInt24)
                return "256 Color Render Layer with 24bit Signed Integer Data";
            else if (type == T::Render_DataUInt24)
                return "256 Color Render Layer with 24bit Unsigned Integer Data";
            else if (type == T::DataSInt8)
                return "8bit Signed Integer Data Layer";
            else if (type == T::DataUInt8)
                return "8bit Unsigned Integer Data Layer";
            else if (type == T::DataSInt16)
                return "16bit Signed Integer Data Layer";
            else if (type == T::DataUInt16)
                return "16bit Unsigned Integer Data Layer";
            else BUG("Missing translation");
            return "???";
        }
    }

} // namespace

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
