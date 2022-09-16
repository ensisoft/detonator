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

#include "warnpush.h"
#  include "ui_tilemapwidget.h"
#include "warnpop.h"

#include <memory>
#include <vector>
#include <optional>
#include <unordered_map>

#include "game/tilemap.h"
#include "engine/renderer.h"
#include "editor/gui/mainwidget.h"

namespace gui
{
    class MouseTool;
    class PaletteMaterial;

    class TilemapWidget : public MainWidget
    {
        Q_OBJECT

    public:
        TilemapWidget(app::Workspace* workspace);
        TilemapWidget(app::Workspace* workspace, const app::Resource& resource);
       ~TilemapWidget();

        virtual QString GetId() const override;
        virtual void Initialize(const UISettings& settings) override;
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipbboad)  const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Update(double dt) override;
        virtual void Render() override;
        virtual void Save() override;
        virtual void Undo() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool OnEscape() override;
        virtual bool GetStats(Stats* stats) const override;
        virtual void Refresh() override;
    private slots:
        void on_mapName_textChanged();
        void on_mapTileSize_valueChanged(int);
        void on_mapHeight_valueChanged(int);
        void on_mapWidth_valueChanged(int);
        void on_btnApplyMapSize_clicked();
        void on_actionSave_triggered();
        void on_btnNewLayer_clicked();
        void on_btnDeleteLayer_clicked();
        void on_btnEditLayer_clicked();
        void on_btnViewReset_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnViewPlus90_clicked();
        void on_widgetColor_colorChanged(QColor color);
        void on_cmbTool_currentIndexChanged(int);
        void on_cmbTool_editTextChanged(const QString& text);
        void on_btnAddTool_clicked();
        void on_btnDelTool_clicked();
        void on_btnSelectToolMaterial_clicked();
        void on_btnSetToolMaterialParams_clicked();
        void on_btnResetPaletteIndex_clicked();
        void on_cmbToolFunction_currentIndexChanged(int);
        void on_cmbToolShape_currentIndexChanged(int);
        void on_toolWidth_valueChanged(int);
        void on_toolHeight_valueChanged(int);
        void on_cmbToolMaterial_currentIndexChanged(int);
        void on_toolPaletteIndex_valueChanged(int);
        void on_toolValue_valueChanged(int);
        void on_chkToolMaterial_stateChanged(int);
        void on_chkToolValue_stateChanged(int);
        void on_layerName_textChanged();
        void on_cmbLayerCache_currentIndexChanged(int);
        void on_chkLayerVisible_stateChanged(int);
        void on_chkLayerEnabled_stateChanged(int);
        void on_chkLayerReadOnly_stateChanged(int);
        void on_cmbTileMaterial_currentIndexChanged(int);
        void on_btnSelectTileMaterial_clicked();
        void on_btnDeleteTileMaterial_clicked();
        void on_tileValue_valueChanged(int);

        void StartTool();

        void NewResourceAvailable(const app::Resource* resource);
        void ResourceToBeDeleted(const app::Resource* resource);
        void ResourceUpdated(const app::Resource* resource);
        void LayerSelectionChanged(const QItemSelection, const QItemSelection);
        void PaletteMaterialChanged(const PaletteMaterial* material);
    private:
        void DisplayCurrentCameraLocation();
        void SetMapProperties();
        void SetLayerProperties();
        void DisplayMapProperties();
        void DisplayLayerProperties();
        void DisplaySelection();
        void UpdateLayerPalette();
        void InitScene(unsigned width, unsigned height);
        void PaintScene(gfx::Painter& painter, double sec);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);
        game::TilemapLayerClass* GetCurrentLayer();
        game::TilemapLayer* GetCurrentLayerInstance();
        const game::TilemapLayerClass* GetCurrentLayer() const;
        const game::TilemapLayer* GetCurrentLayerInstance() const;
        size_t GetCurrentLayerIndex() const;
        void GenerateTools();
        void UpdateToolCombo();
        void UpdateToolToolbar();
        void ModifyCurrentTool();
        void ShowCurrentTool();
        void UncheckTools();
        struct Tool;
        Tool* GetCurrentTool(size_t* index = nullptr);
        void SetCurrentTool(const QString& id);
        void ModifyCurrentLayer();
        bool ValidateToolAgainstLayer(const Tool& tool, const game::TilemapLayer& layer);
        void ToolIntoJson(const Tool& tool, QJsonObject& json) const;
        void ToolFromJson(Tool& tool, const QJsonObject& json) const;
        void ReplaceDeletedResources();
        void ClearUnusedPaletteEntries();
    private:
        Ui::Tilemap mUI;
    private:
        class LayerModel;
        class LayerData;
        class TileBrushTool;
        class TileSelectTool;
        enum class ToolFunction {
            TileBrush
        };
        enum class ToolShape {
            Rectangle
        };

        struct Tool {
            ToolFunction tool = ToolFunction::TileBrush;
            ToolShape shape = ToolShape::Rectangle;
            QString id;
            QString name;
            std::string material;
            std::int32_t value = 0;
            int palette_index = -1;
            unsigned width  = 0;
            unsigned height = 0;
            bool apply_material = true;
            bool apply_value = false;
        };
        std::vector<Tool> mTools;
        std::vector<QAction*> mToolActions;

        // tile selection. dimensions in tiles.
        struct TileSelection {
            unsigned start_row = 0;
            unsigned start_col = 0;
            unsigned width  = 0;
            unsigned height = 0;
        };

        struct State {
            app::Workspace* workspace = nullptr;
            std::shared_ptr<game::TilemapClass> klass;
            std::unique_ptr<game::Tilemap> map;
            std::optional<TileSelection> selection;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
        } mState;

        std::unique_ptr<MouseTool> mCurrentTool;
        std::unique_ptr<LayerModel> mModel;
        std::unordered_map<std::string,
            std::shared_ptr<LayerData>> mLayerData;
        std::vector<PaletteMaterial*> mPaletteMaterialWidgets;
        std::size_t mHash = 0;
        engine::Renderer mRenderer;
        double mCurrentTime = 0.0;
        double mViewTransformStartTime = 0.0;
        float mViewTransformRotation = 0.0f;
        bool mCameraWasLoaded = false;
    };
} // namespace
