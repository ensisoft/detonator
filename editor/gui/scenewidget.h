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

#include "config.h"

#include "warnpush.h"
#  include "ui_scenewidget.h"
#  include <QMenu>
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <memory>
#include <string>

#include "editor/gui/mainwidget.h"
#include "editor/gui/treemodel.h"
#include "game/scene.h"
#include "game/fwd.h"
#include "engine/renderer.h"

namespace gfx {
    class Painter;
} // gfx

namespace app {
    class Resource;
    class Workspace;
};

namespace gui
{
    class TreeWidget;
    class MouseTool;

    class SceneWidget : public MainWidget
    {
        Q_OBJECT
    public:
        SceneWidget(app::Workspace* workspace);
        SceneWidget(app::Workspace* workspace, const app::Resource& resource);
       ~SceneWidget();

        virtual QString GetId() const override;
        virtual void Initialize(const UISettings& settings) override;
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipboard) const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual void Undo() override;
        virtual void Save() override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Update(double secs) override;
        virtual void Render() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool OnEscape() override;
        virtual void Refresh() override;
        virtual bool GetStats(Stats* stats) const override;
    private slots:
        void on_widgetColor_colorChanged(QColor color);
        void on_name_textChanged(const QString&);
        void on_cmbScripts_currentIndexChanged(int);
        void on_cmbTilemaps_currentIndexChanged(int);
        void on_cmbSpatialIndex_currentIndexChanged(int);
        void on_spRectX_valueChanged(double value);
        void on_spRectY_valueChanged(double value);
        void on_spRectW_valueChanged(double value);
        void on_spRectH_valueChanged(double value);
        void on_spQuadMaxLevels_valueChanged(int);
        void on_spQuadMaxItems_valueChanged(int);
        void on_spDenseGridRows_valueChanged(int);
        void on_spDenseGridCols_valueChanged(int);
        void on_chkLeftBoundary_stateChanged(int);
        void on_chkRightBoundary_stateChanged(int);
        void on_chkTopBoundary_stateChanged(int);
        void on_chkBottomBoundary_stateChanged(int);
        void on_spinLeftBoundary_valueChanged(double value);
        void on_spinRightBoundary_valueChanged(double value);
        void on_spinTopBoundary_valueChanged(double value);
        void on_spinBottomBoundary_valueChanged(double value);
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNodeEdit_triggered();
        void on_actionNodeDelete_triggered();
        void on_actionNodeBreakLink_triggered();
        void on_actionNodePlace_triggered();
        void on_actionNodeDuplicate_triggered();
        void on_actionNodeMoveUpLayer_triggered();
        void on_actionNodeMoveDownLayer_triggered();
        void on_actionEntityVarRef_triggered();
        void on_btnEditScript_clicked();
        void on_btnResetScript_clicked();
        void on_btnAddScript_clicked();
        void on_btnEditMap_clicked();
        void on_btnAddMap_clicked();
        void on_btnResetMap_clicked();
        void on_btnNewScriptVar_clicked();
        void on_btnEditScriptVar_clicked();
        void on_btnDeleteScriptVar_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnViewReset_clicked();
        void on_nodeName_textChanged(const QString& text);
        void on_nodeEntity_currentIndexChanged(const QString& name);
        void on_nodeLayer_valueChanged(int layer);
        void on_nodeLink_currentIndexChanged(const QString& text);
        void on_nodeIsVisible_stateChanged(int);
        void on_nodeTranslateX_valueChanged(double value);
        void on_nodeTranslateY_valueChanged(double value);
        void on_nodeScaleX_valueChanged(double value);
        void on_nodeScaleY_valueChanged(double value);
        void on_nodeRotation_valueChanged(double value);
        void on_btnEditEntity_clicked();
        void on_btnEntityParams_clicked();
        void on_btnNodePlus90_clicked();
        void on_btnNodeMinus90_clicked();
        void on_tree_customContextMenuRequested(QPoint);

        void PlaceNewEntity();
        void TreeCurrentNodeChangedEvent();
        void TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target);
        void TreeClickEvent(TreeWidget::TreeItem* item);
        void NewResourceAvailable(const app::Resource* resource);
        void ResourceToBeDeleted(const app::Resource* resource);
        void ResourceUpdated(const app::Resource* resource);
    private:
        void InitScene(unsigned width, unsigned height);
        void PaintScene(gfx::Painter& painter, double secs);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);
        void DisplayCurrentCameraLocation();
        void DisplayCurrentNodeProperties();
        void DisplaySceneProperties();
        void UncheckPlacementActions();
        void TranslateCurrentNode(float dx, float dy);
        void TranslateCamera(float dx, float dy);
        void RebuildMenus();
        void RebuildCombos();
        void UpdateResourceReferences();
        void SetSpatialIndexParams();
        void SetSceneBoundary();
        game::SceneNodeClass* SelectNode(const QPoint& click_point, glm::vec2* hitpos);
        game::SceneNodeClass* GetCurrentNode();
        const game::SceneNodeClass* GetCurrentNode() const;
    private:
        Ui::SceneWidget mUI;
        // there doesn't seem to be a way to do this in the designer
        // so we create our menu for Entities.
        QMenu* mEntities = nullptr;
    private:
        class PlaceEntityTool;
        class ScriptVarModel;
        enum class PlayState {
            Playing, Paused, Stopped
        };
        struct State {
            game::SceneClass scene;
            engine::Renderer renderer;
            app::Workspace* workspace = nullptr;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
            TreeWidget* view = nullptr;
            QString last_placed_entity;
        } mState;

        // Tree model impl for displaying scene's render tree
        // in the tree widget.
        using TreeModel = RenderTreeModel<game::SceneClass>;
        std::size_t mOriginalHash = 0;
        std::unique_ptr<TreeModel> mRenderTree;
        std::unique_ptr<MouseTool> mCurrentTool;
        std::unique_ptr<ScriptVarModel> mScriptVarModel;
        PlayState mPlayState = PlayState::Stopped;
        double mCurrentTime = 0.0;
        double mSceneTime   = 0.0;
        double mViewTransformStartTime = 0.0;
        float mViewTransformRotation = 0.0f;
        bool mCameraWasLoaded = false;
        std::unique_ptr<game::Tilemap> mTilemap;
        // Undo "stack" with fixed capacity that begins
        // overwrite old items when space is exceeded
        boost::circular_buffer<game::SceneClass> mUndoStack;
    };
}
