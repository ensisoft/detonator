// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#  include <QDialog>
#  include <QTimer>
#  include "ui_dlgtiletool.h"
#include "warnpop.h"

#include <vector>
#include <string>
#include <cstdint>
#include <functional>

#include "graphics/material.h"
#include "editor/app/workspace.h"

namespace gui
{
    class Settings;

    enum class TileToolFunction {
        TileBrush
    };
    enum class TileToolShape {
        Rectangle
    };

    struct TileTool {
        TileToolFunction tool = TileToolFunction::TileBrush;
        TileToolShape shape   = TileToolShape::Rectangle;
        QString id;
        QString name;

        struct Tile {
            std::string material;
            std::int32_t value  = 0;
            int palette_index   = -1;
            bool apply_material = true;
            bool apply_value    = false;

            // filled at runtime when the tool is used.
            mutable std::unique_ptr<gfx::Material> instance;
            mutable std::int32_t material_palette_index = 0;
            mutable std::int32_t data_value = 0;
        };
        std::vector<Tile> tiles;
        unsigned width  = 0;
        unsigned height = 0;
    };


    class DlgTileTool : public QDialog
    {
        Q_OBJECT

    public:
        using ToolBox = std::vector<std::shared_ptr<TileTool>>;
        using ToolBoxUpdate = std::function<void()>;

        ToolBoxUpdate NotifyToolBoxUpdate;

        DlgTileTool(const app::Workspace* workspace, QWidget* parent, ToolBox* tools);

        inline void SetClass(std::shared_ptr<game::TilemapClass> klass) noexcept
        { mClass = std::move(klass); }

        void SaveState(Settings& settings) const;
        void LoadState(const Settings& settings);


    private slots:
        void on_cmbTool_currentIndexChanged(int index);
        void on_toolName_editingFinished();
        void on_btnAddTool_clicked();
        void on_btnDelTool_clicked();
        void on_btnSelectToolMaterial_clicked();
        void on_btnSetToolMaterialParams_clicked();
        void on_btnEditToolMaterial_clicked();
        void on_btnResetPaletteIndex_clicked();
        void on_cmbToolFunction_currentIndexChanged(int index);
        void on_cmbToolShape_currentIndexChanged(int index);
        void on_toolWidth_valueChanged(int);
        void on_toolHeight_valueChanged(int);
        void on_cmbToolMaterial_currentIndexChanged(int);
        void on_toolPaletteIndex_valueChanged(int);
        void on_toolValue_valueChanged(int);
        void on_material_toggled();
        void on_data_toggled();
        void on_widgetColor_colorChanged(QColor color);
        void on_tileCol_valueChanged(int);
        void on_tileRow_valueChanged(int);

        void ResourceAdded(const app::Resource* resource);
        void ResourceRemoved(const app::Resource* resource);
        void ResourceUpdated(const app::Resource* resource);

    private:
        void PaintScene(gfx::Painter& painter, double);
        bool KeyPress(QKeyEvent* key);
        void MousePress(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void ShowCurrentTool();
        void ShowCurrentTile();
        TileTool* GetCurrentTool();
        TileTool::Tile* GetCurrentTile();
        TileTool::Tile* GetTileUnderMouse(unsigned* col, unsigned* row);
        void SetCurrentTool(const QString& id);
        void ModifyCurrentTool();
        void ModifyCurrentTile();
        void UpdateToolCombo();
    private:
        Ui::DlgTileTool mUI;
    private:
        const app::Workspace* mWorkspace = nullptr;
        ToolBox* mTools = nullptr;
        std::shared_ptr<game::TilemapClass> mClass;
        QTimer mTimer;

        struct State {
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
        } mState;
    };
} // namespace
