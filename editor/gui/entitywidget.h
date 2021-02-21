// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_entitywidget.h"
#  include <QMenu>
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <memory>
#include <string>

#include "editor/gui/mainwidget.h"
#include "editor/gui/treemodel.h"
#include "engine/entity.h"
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

    class EntityWidget : public MainWidget
    {
        Q_OBJECT
    public:
        EntityWidget(app::Workspace* workspace);
        EntityWidget(app::Workspace* workspace, const app::Resource& resource);
       ~EntityWidget();

        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipboard) const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual void Save() override;
        virtual void Undo() override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Update(double secs) override;
        virtual void Render() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool ConfirmClose() override;
        virtual void Refresh() override;
        virtual bool GetStats(Stats* stats) const override;
    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNewRect_triggered();
        void on_actionNewCircle_triggered();
        void on_actionNewSemiCircle_triggered();
        void on_actionNewIsoscelesTriangle_triggered();
        void on_actionNewRightTriangle_triggered();
        void on_actionNewRoundRect_triggered();
        void on_actionNewTrapezoid_triggered();
        void on_actionNewCapsule_triggered();
        void on_actionNewParallelogram_triggered();
        void on_actionNodeDelete_triggered();
        void on_actionNodeMoveUpLayer_triggered();
        void on_actionNodeMoveDownLayer_triggered();
        void on_actionNodeDuplicate_triggered();
        void on_entityName_textChanged(const QString& text);
        void on_entityLifetime_valueChanged(double value);
        void on_chkEntityLifetime_stateChanged(int);
        void on_chkKillAtLifetime_stateChanged(int);
        void on_btnAddIdleTrack_clicked();
        void on_btnResetIdleTrack_clicked();
        void on_btnAddScript_clicked();
        void on_btnResetScript_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnResetTransform_clicked();
        void on_btnNewTrack_clicked();
        void on_btnEditTrack_clicked();
        void on_btnDeleteTrack_clicked();
        void on_btnNewScriptVar_clicked();
        void on_btnEditScriptVar_clicked();
        void on_btnDeleteScriptVar_clicked();
        void on_btnSelectMaterial_clicked();
        void on_trackList_itemSelectionChanged();
        void on_idleTrack_currentIndexChanged(int);
        void on_scriptFile_currentIndexChanged(int);
        void on_nodeName_textChanged(const QString& text);
        void on_nodeSizeX_valueChanged(double value);
        void on_nodeSizeY_valueChanged(double value);
        void on_nodeTranslateX_valueChanged(double value);
        void on_nodeTranslateY_valueChanged(double value);
        void on_nodeScaleX_valueChanged(double value);
        void on_nodeScaleY_valueChanged(double value);
        void on_nodeRotation_valueChanged(double value);
        void on_nodePlus90_clicked();
        void on_nodeMinus90_clicked();
        void on_dsDrawable_currentIndexChanged(const QString& name);
        void on_dsMaterial_currentIndexChanged(const QString& name);
        void on_dsRenderPass_currentIndexChanged(const QString&);
        void on_dsRenderStyle_currentIndexChanged(const QString&);
        void on_dsLayer_valueChanged(int layer);
        void on_dsLineWidth_valueChanged(double value);
        void on_dsTimeScale_valueChanged(double value);
        void on_dsVisible_stateChanged(int);
        void on_dsUpdateDrawable_stateChanged(int);
        void on_dsUpdateMaterial_stateChanged(int);
        void on_dsRestartDrawable_stateChanged(int);
        void on_dsOverrideAlpha_stateChanged(int);
        void on_dsFlipVertically_stateChanged(int);
        void on_dsAlpha_valueChanged();
        void on_rbSimulation_currentIndexChanged(const QString&);
        void on_rbShape_currentIndexChanged(const QString&);
        void on_rbPolygon_currentIndexChanged(const QString&);
        void on_rbFriction_valueChanged(double value);
        void on_rbRestitution_valueChanged(double value);
        void on_rbAngularDamping_valueChanged(double value);
        void on_rbLinearDamping_valueChanged(double value);
        void on_rbDensity_valueChanged(double value);
        void on_rbLinearVeloX_valueChanged(double value);
        void on_rbLinearVeloY_valueChanged(double value);
        void on_rbAngularVelo_valueChanged(double value);
        void on_rbIsBullet_stateChanged(int);
        void on_rbIsSensor_stateChanged(int);
        void on_rbIsEnabled_stateChanged(int);
        void on_rbCanSleep_stateChanged(int);
        void on_rbDiscardRotation_stateChanged(int);
        void on_drawableItem_toggled(bool on);
        void on_rigidBodyItem_toggled(bool on);
        void on_tree_customContextMenuRequested(QPoint);

        void TreeCurrentNodeChangedEvent();
        void TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target);
        void TreeClickEvent(TreeWidget::TreeItem* item);
        void NewResourceAvailable(const app::Resource* resource);
        void ResourceToBeDeleted(const app::Resource* resource);
        void ResourceUpdated(const app::Resource* resource);
        void PlaceNewParticleSystem();
        void PlaceNewCustomShape();
    private:
        void InitScene(unsigned width, unsigned height);
        void PaintScene(gfx::Painter& painter, double secs);
        void MouseZoom(std::function<void(void)> zoom_function);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        bool KeyPress(QKeyEvent* key);
        void DisplayCurrentCameraLocation();
        void DisplayCurrentNodeProperties();
        void UncheckPlacementActions();
        void UpdateCurrentNodePosition(float dx, float dy);
        void UpdateCurrentNodeProperties();
        void RebuildMenus();
        void RebuildCombos();
        void UpdateDeletedResourceReferences();
        game::EntityNodeClass* GetCurrentNode();
        const game::EntityNodeClass* GetCurrentNode() const;
    private:
        Ui::EntityWidget mUI;
        // there doesn't seem to be a way to do this in the designer
        // so we create our menu for user defined drawables
        QMenu* mParticleSystems = nullptr;
        // menu for the custom shapes
        QMenu* mCustomShapes = nullptr;
    private:
        class PlaceShapeTool;
        class ScriptVarModel;
        enum class PlayState {
            Playing, Paused, Stopped
        };
        struct State {
            // shared with the animation trackw widget.
            std::shared_ptr<game::EntityClass> entity;
            game::Renderer renderer;
            app::Workspace* workspace = nullptr;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
            TreeWidget* view = nullptr;
        } mState;

        // Tree model impl for displaying scene's render tree
        // in the tree widget.
        using TreeModel = RenderTreeModel<game::EntityClass>;
        std::size_t mOriginalHash = 0;
        std::unique_ptr<TreeModel> mRenderTree;
        std::unique_ptr<MouseTool> mCurrentTool;
        std::unique_ptr<ScriptVarModel> mScriptVarModel;
        PlayState mPlayState = PlayState::Stopped;
        double mCurrentTime = 0.0;
        double mEntityTime   = 0.0;
        double mViewTransformStartTime = 0.0;
        float mViewTransformRotation = 0.0f;
        bool mCameraWasLoaded = false;

        // Undo "stack" with fixed capacity that begins
        // overwrite old items when space is exceeded
        boost::circular_buffer<game::EntityClass> mUndoStack;
    };
}
