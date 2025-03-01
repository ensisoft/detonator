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
#include "editor/gui/tool.h"
#include "editor/gui/dlgtiletool.h"
#include "editor/gui/settings.h"

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
        virtual QImage TakeScreenshot() const override;
        virtual void InitializeSettings(const UISettings& settings) override;
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
        virtual bool OnKeyDown(QKeyEvent* key) override;
        virtual bool GetStats(Stats* stats) const override;
        virtual void Refresh() override;
        virtual void OnAddResource(const app::Resource* resource) override;
        virtual void OnRemoveResource(const app::Resource* resource) override;
        virtual void OnUpdateResource(const app::Resource* resource) override;
    private slots:
        void on_mapName_textChanged();
        void on_cmbPerspective_currentIndexChanged(int);
        void on_mapTileSize_valueChanged(int);
        void on_mapHeight_valueChanged(int);
        void on_mapWidth_valueChanged(int);
        void on_tileScaleX_valueChanged(double);
        void on_tileScaleY_valueChanged(double);
        void on_btnApplyMapSize_clicked();
        void on_actionSave_triggered();
        void on_actionTools_triggered();
        void on_actionNewLayer_triggered();
        void on_actionEditLayer_triggered();
        void on_actionDeleteLayer_triggered();
        void on_actionMoveLayerUp_triggered();
        void on_actionMoveLayerDown_triggered();
        void on_btnNewLayer_clicked();
        void on_btnDeleteLayer_clicked();
        void on_btnEditLayer_clicked();
        void on_btnViewReset_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnMoreViewportSettings_clicked();
        void on_widgetColor_colorChanged(QColor color);
        void on_layers_doubleClicked(const QModelIndex& index);
        void on_layerName_textChanged();
        void on_cmbLayerCache_currentIndexChanged(int);
        void on_layerDepth_valueChanged(int);
        void on_renderLayer_valueChanged(int);
        void on_chkLayerVisible_stateChanged(int);
        void on_chkLayerEnabled_stateChanged(int);
        void on_chkLayerReadOnly_stateChanged(int);
        void on_tilePaletteIndex_valueChanged(int);
        void on_tileValue_valueChanged(int);
        void on_layers_customContextMenuRequested(const QPoint&);

        void DeleteSelectedTileMaterial();
        void SelectSelectedTileMaterial();

        void StartToolAction();
        void LayerSelectionChanged(const QItemSelection, const QItemSelection);
        void PaletteMaterialChanged(const PaletteMaterial* material);
    private:
        void StartTool(const QString& id);
        void DisplayCurrentCameraLocation();
        void SetMapProperties();
        void SetLayerProperties();
        void DisplayMapProperties();
        void DisplayLayerProperties();
        void DisplaySelection();
        void UpdateLayerPalette();
        void PaintScene(gfx::Painter& painter, double sec);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        void MouseZoom(std::function<void(void)> zoom_function);
        bool KeyPress(QKeyEvent* key);
        game::TilemapLayerClass* GetCurrentLayer();
        game::TilemapLayer* GetCurrentLayerInstance();
        const game::TilemapLayerClass* GetCurrentLayer() const;
        const game::TilemapLayer* GetCurrentLayerInstance() const;
        size_t GetCurrentLayerIndex() const;
        void GenerateTools();
        void UpdateToolToolbar();
        void UncheckTools();
        void SetCurrentTool(const QString& id);
        void ModifyCurrentLayer();
        void ReplaceDeletedResources();
        void ClearUnusedPaletteEntries();
        bool SelectLayerOnKey(unsigned index);

        using Tool = TileTool;
        Tool* GetCurrentTool(size_t* index = nullptr);
        bool ValidateToolAgainstLayer(const Tool& tool, const game::TilemapLayer& layer);
        void ToolIntoJson(const Tool& tool, QJsonObject& json) const;
        bool ToolFromJson(Tool& tool, const QJsonObject& json) const;

        struct Tile {
            unsigned col = 0;
            unsigned row = 0;
        };
        std::optional<Tile> FindTileUnderMouse()const;

    private:
        Ui::Tilemap mUI;
        QMenu* mHamburger = nullptr;
    private:
        class LayerModel;
        class LayerData;
        class TileBrushTool;
        class TileSelectTool;
        class TileMoveTool;

        using ToolFunction = TileToolFunction;
        using ToolShape    = TileToolShape;
        std::vector<std::shared_ptr<Tool>> mTools;

        std::vector<QAction*> mToolActions;

        QString mCurrentToolId;

        // tile selection. dimensions in tiles.
        struct TileSelection {
            unsigned start_row = 0;
            unsigned start_col = 0;
            unsigned width  = 0;
            unsigned height = 0;
            game::TilemapLayerClass::Resolution resolution;
        };

        struct State {
            app::Workspace* workspace = nullptr;
            std::shared_ptr<game::TilemapClass> klass;
            std::unique_ptr<game::Tilemap> map;
            std::optional<TileSelection> selection;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
            engine::Renderer renderer;
        } mState;

        std::unique_ptr<DlgTileTool> mDlgTileTool;
        Settings mTileToolSettings;

        std::unique_ptr<MouseTool> mCurrentTool;
        std::unique_ptr<MouseTool> mCameraTool;
        std::unique_ptr<LayerModel> mModel;
        std::unordered_map<std::string,
            std::shared_ptr<LayerData>> mLayerData;
        std::vector<PaletteMaterial*> mPaletteMaterialWidgets;
        std::size_t mHash = 0;
        double mCurrentTime = 0.0;

        UIAnimator mAnimator;
    };
} // namespace
