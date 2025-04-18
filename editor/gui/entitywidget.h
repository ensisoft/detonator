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
#  include "ui_entitywidget.h"
#  include <QMenu>
#  include <QVariantMap>
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "editor/gui/mainwidget.h"
#include "editor/gui/treemodel.h"
#include "editor/gui/tool.h"
#include "game/entity.h"
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
    class PlayWindow;
    class DlgAnimator;

    class EntityWidget : public MainWidget
    {
        Q_OBJECT
    public:
        EntityWidget(app::Workspace* workspace);
        EntityWidget(app::Workspace* workspace, const app::Resource& resource);
       ~EntityWidget();

        virtual QString GetId() const override;
        virtual QImage TakeScreenshot() const override;
        virtual void InitializeSettings(const UISettings& settings) override;
        virtual void SetViewerMode() override;
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
        virtual void RunGameLoopOnce() override;
        virtual bool HasUnsavedChanges() const override;
        virtual void Refresh() override;
        virtual bool GetStats(Stats* stats) const override;
        virtual bool OnEscape() override;
        virtual bool LaunchScript(const app::AnyString& id) override;
        virtual void OnAddResource(const app::Resource* resource) override;
        virtual void OnRemoveResource(const app::Resource* resource) override;
        virtual void OnUpdateResource(const app::Resource* resource) override;

        std::string GetEntityId() const
        { return mState.entity->GetId(); }

        void SaveAnimation(const game::AnimationClass& track, const QVariantMap& properties);
        void SaveAnimator(const game::EntityStateControllerClass& animator, const QVariantMap& properties);
    private slots:
        void on_widgetColor_colorChanged(const QColor& color);
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionPreview_triggered();
        void on_actionNewJoint_triggered();
        void on_actionNewRect_triggered();
        void on_actionNewCircle_triggered();
        void on_actionNewSemiCircle_triggered();
        void on_actionNewIsoscelesTriangle_triggered();
        void on_actionNewRightTriangle_triggered();
        void on_actionNewRoundRect_triggered();
        void on_actionNewTrapezoid_triggered();
        void on_actionNewCapsule_triggered();
        void on_actionNewParallelogram_triggered();
        void on_actionNewAmbientLight_triggered();
        void on_actionNewDirectionalLight_triggered();
        void on_actionNewPointLight_triggered();
        void on_actionNewSpotlight_triggered();
        void on_actionNewText_triggered();
        void on_actionNewStaticRigidBody_triggered();

        void on_actionNodeDelete_triggered();
        void on_actionNodeCut_triggered();
        void on_actionNodeCopy_triggered();
        void on_actionNodeVarRef_triggered();
        void on_actionNodeMoveUpLayer_triggered();
        void on_actionNodeMoveDownLayer_triggered();
        void on_actionNodeDuplicate_triggered();
        void on_actionNodeComment_triggered();
        void on_actionNodeRename_triggered();
        void on_actionNodeRenameAll_triggered();
        void on_actionScriptVarAdd_triggered();
        void on_actionScriptVarDel_triggered();
        void on_actionScriptVarEdit_triggered();
        void on_actionJointAdd_triggered();
        void on_actionJointDel_triggered();
        void on_actionJointEdit_triggered();
        void on_actionAnimationAdd_triggered();
        void on_actionAnimationDel_triggered();
        void on_actionAnimationEdit_triggered();
        void on_actionAddPresetParticle_triggered();
        void on_entityName_textChanged(const QString& text);
        void on_entityTag_textChanged(const QString& text);
        void on_entityLifetime_valueChanged(double value);
        void on_chkKillAtLifetime_stateChanged(int);
        void on_chkKillAtBoundary_stateChanged(int);
        void on_chkTickEntity_stateChanged(int);
        void on_chkUpdateEntity_stateChanged(int);
        void on_chkPostUpdate_stateChanged(int);
        void on_chkUpdateNodes_stateChanged(int);
        void on_chkKeyEvents_stateChanged(int);
        void on_chkMouseEvents_stateChanged(int);
        void on_btnAddIdleTrack_clicked();
        void on_btnResetIdleTrack_clicked();
        void on_btnAddScript_clicked();
        void on_btnEditScript_clicked();
        void on_btnResetScript_clicked();
        void on_btnEditAnimator_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnViewReset_clicked();
        void on_btnNewTrack_clicked();
        void on_btnEditTrack_clicked();
        void on_btnDeleteTrack_clicked();
        void on_btnNewScriptVar_clicked();
        void on_btnEditScriptVar_clicked();
        void on_btnDeleteScriptVar_clicked();
        void on_btnResetLifetime_clicked();
        void on_btnNewJoint_clicked();
        void on_btnEditJoint_clicked();
        void on_btnDeleteJoint_clicked();
        void on_btnSelectMaterial_clicked();
        void on_btnSetMaterialParams_clicked();
        void on_btnEditDrawable_clicked();
        void on_btnEditMaterial_clicked();
        void on_btnMoreViewportSettings_clicked();
        void on_trackList_itemSelectionChanged();
        void on_idleTrack_currentIndexChanged(int);
        void on_scriptFile_currentIndexChanged(int);
        void on_nodeName_textChanged(const QString& text);
        void on_nodeComment_textChanged(const QString& text);
        void on_nodeTag_textChanged(const QString& text);
        void on_nodeIndex_valueChanged(int);
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
        void on_dsRenderView_currentIndexChanged(int);
        void on_dsRenderProj_currentIndexChanged(int);
        void on_dsCoordinateSpace_currentIndexChanged(int);
        void on_dsLayer_valueChanged(int layer);
        void on_dsLineWidth_valueChanged(double value);
        void on_dsTimeScale_valueChanged(double value);
        void on_dsDepth_valueChanged(double value);
        void on_dsXRotation_valueChanged(double value);
        void on_dsYRotation_valueChanged(double value);
        void on_dsZRotation_valueChanged(double value);
        void on_dsXOffset_valueChanged(double value);
        void on_dsYOffset_valueChanged(double value);
        void on_dsZOffset_valueChanged(double value);
        void on_dsVisible_stateChanged(int);
        void on_dsUpdateDrawable_stateChanged(int);
        void on_dsUpdateMaterial_stateChanged(int);
        void on_dsRestartDrawable_stateChanged(int);
        void on_dsFlipHorizontally_stateChanged(int);
        void on_dsFlipVertically_stateChanged(int);
        void on_dsBloom_stateChanged(int);
        void on_dsDoubleSided_stateChanged(int);
        void on_dsDepthTest_stateChanged(int);
        void on_rbSimulation_currentIndexChanged(const QString&);
        void on_rbShape_currentIndexChanged(const QString&);
        void on_rbPolygon_currentIndexChanged(const QString&);
        void on_rbFriction_valueChanged(double value);
        void on_rbRestitution_valueChanged(double value);
        void on_rbAngularDamping_valueChanged(double value);
        void on_rbLinearDamping_valueChanged(double value);
        void on_rbDensity_valueChanged(double value);
        void on_rbIsBullet_stateChanged(int);
        void on_rbIsSensor_stateChanged(int);
        void on_rbIsEnabled_stateChanged(int);
        void on_rbCanSleep_stateChanged(int);
        void on_rbDiscardRotation_stateChanged(int);
        void on_tiFontName_currentIndexChanged(int);
        void on_tiFontSize_currentIndexChanged(int);
        void on_tiVAlign_currentIndexChanged(int);
        void on_tiHAlign_currentIndexChanged(int);
        void on_tiTextColor_colorChanged(QColor color);
        void on_tiLineHeight_valueChanged(double value);
        void on_tiLayer_valueChanged(int);
        void on_tiRasterWidth_valueChanged(int);
        void on_tiRasterHeight_valueChanged(int);
        void on_tiCoordinateSpace_currentIndexChanged(int);
        void on_tiText_textChanged();
        void on_tiVisible_stateChanged(int);
        void on_tiUnderline_stateChanged(int);
        void on_tiBlink_stateChanged(int);
        void on_tiStatic_stateChanged(int);
        void on_tiBloom_stateChanged(int);
        void on_spnShape_currentIndexChanged(const QString&);
        void on_spnEnabled_stateChanged(int);
        void on_fxShape_currentIndexChanged(const QString&);
        void on_fxBody_currentIndexChanged(const QString&);
        void on_fxPolygon_currentIndexChanged(const QString&);
        void on_fxFriction_valueChanged(double);
        void on_fxDensity_valueChanged(double);
        void on_fxBounciness_valueChanged(double);
        void on_fxIsSensor_stateChanged(int);
        void on_btnResetFxFriction_clicked();
        void on_btnResetFxDensity_clicked();
        void on_btnResetFxBounciness_clicked();
        void on_btnSelectFont_clicked();
        void on_btnSelectFontFile_clicked();
        void on_btnResetTextRasterWidth_clicked();
        void on_btnResetTextRasterHeight_clicked();
        void on_mnVCenter_valueChanged(double);
        void on_mnHCenter_valueChanged(double);
        void on_tfIntegrator_currentIndexChanged(int);
        void on_tfVelocityX_valueChanged(double);
        void on_tfVelocityY_valueChanged(double);
        void on_tfVelocityA_valueChanged(double);
        void on_tfAccelX_valueChanged(double);
        void on_tfAccelY_valueChanged(double);
        void on_tfAccelA_valueChanged(double);
        void on_tfEnabled_stateChanged(int);
        void on_ltType_currentIndexChanged(int);
        void on_ltAmbient_colorChanged(const QColor&);
        void on_ltDiffuse_colorChanged(const QColor&);
        void on_ltSpecular_colorChanged(const QColor&);
        void on_ltConstantAttenuation_valueChanged(double);
        void on_ltLinearAttenuation_valueChanged(double);
        void on_ltQuadraticAttenuation_valueChanged(double);
        void on_ltTranslation_ValueChanged(const Vector3*);
        void on_ltDirection_ValueChanged(const Vector3*);
        void on_ltSpotHalfAngle_valueChanged(double);
        void on_ltLayer_valueChanged(int);
        void on_ltEnabled_stateChanged(int);

        void on_btnDelDrawable_clicked();
        void on_btnDelTextItem_clicked();
        void on_btnDelRigidBody_clicked();
        void on_btnDelFixture_clicked();
        void on_btnDelTilemapNode_clicked();
        void on_btnDelSpatialNode_clicked();
        void on_btnDelTransformer_clicked();
        void on_btnDelLight_clicked();
        void on_actionAddLight_triggered();
        void on_actionAddDrawable_triggered();
        void on_actionAddTextItem_triggered();
        void on_actionAddRigidBody_triggered();
        void on_actionAddFixture_triggered();
        void on_actionAddTilemapNode_triggered();
        void on_actionAddSpatialNode_triggered();
        void on_actionAddTransformer_triggered();
        void on_actionEditEntityScript_triggered();
        void on_actionEditControllerScript_triggered();

        void on_animator_toggled(bool on);
        void on_tree_customContextMenuRequested(QPoint);
        void on_scriptVarList_customContextMenuRequested(QPoint);
        void on_jointList_customContextMenuRequested(QPoint);
        void on_trackList_customContextMenuRequested(QPoint);

        void TreeCurrentNodeChangedEvent();
        void TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target);
        void TreeClickEvent(TreeWidget::TreeItem* item);
        void PlaceNewParticleSystem();
        void PlaceNewCustomShape();

    private:
        void ToggleLight(bool on);
        void ToggleDrawable(bool on);
        void ToggleRigidBody(bool on);
        void ToggleTextItem(bool on);
        void ToggleSpatialNode(bool on);
        void ToggleFixture(bool on);
        void ToggleTilemapNode(bool on);
        void ToggleTransformer(bool on);
        void ScrollEntityNodeArea();

    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void MouseZoom(std::function<void(void)> zoom_function);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);
        void DisplayEntityProperties();
        void DisplayCurrentCameraLocation();
        void DisplayCurrentNodeProperties();
        void UncheckPlacementActions();
        void TranslateCamera(float dx, float dy);
        void TranslateCurrentNode(float dx, float dy);
        void UpdateCurrentNodeProperties();
        void RebuildMenus();
        void RebuildCombos();
        void RebuildCombosInternal();
        void UpdateDeletedResourceReferences();
        game::EntityNodeClass* GetCurrentNode();
        const game::EntityNodeClass* GetCurrentNode() const;
        size_t ComputeHash() const;

        glm::vec2 MapMouseCursorToWorld() const;
    private:
        Ui::EntityWidget mUI;
        // there doesn't seem to be a way to do this in the designer
        // so we create our menu for user defined drawables
        QMenu* mParticleSystems = nullptr;
        // menu for the custom shapes
        QMenu* mCustomShapes = nullptr;
        QMenu* mBasicShapes = nullptr;
        QMenu* mBasicLights = nullptr;
        QMenu* mTextItems = nullptr;
        QMenu* mPhysItems = nullptr;
        QMenu* mAttachments = nullptr;
        QMenu* mHamburger = nullptr;
        QToolBar* mButtonBar = nullptr;

        UIAnimator mAnimator;
    private:
        class JointTool;
        class PlaceRigidBodyTool;
        class PlaceTextTool;
        class PlaceLightTool;
        class PlaceShapeTool;
        class ScriptVarModel;
        class JointModel;
        enum class PlayState {
            Playing, Paused, Stopped
        };
        struct State {
            // shared with the animation track widget.
            std::shared_ptr<game::EntityClass> entity;
            engine::Renderer renderer;
            app::Workspace* workspace = nullptr;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
            Ui::EntityWidget* view = nullptr;
        } mState;

        // Tree model impl for displaying scene's render tree
        // in the tree widget.
        using TreeModel = RenderTreeModel<game::EntityClass>;
        std::size_t mOriginalHash = 0;
        std::unique_ptr<TreeModel> mRenderTree;
        std::unique_ptr<MouseTool> mCurrentTool;
        std::unique_ptr<ScriptVarModel> mScriptVarModel;
        std::unique_ptr<JointModel> mJointModel;
        PlayState mPlayState = PlayState::Stopped;
        double mCurrentTime = 0.0;
        double mEntityTime   = 0.0;
        // map animator to some properties
        std::unordered_map<std::string, QVariantMap> mAnimatorProperties;
        // map animation to some properties
        std::unordered_map<std::string, QVariantMap> mTrackProperties;
        // Undo "stack" with fixed capacity that begins
        // overwrite old items when space is exceeded
        boost::circular_buffer<game::EntityClass> mUndoStack;
        // map entity nodes to associated comments (if any)
        std::unordered_map<std::string, QString> mComments;
        // the entity preview window if any.
        std::unique_ptr<PlayWindow> mPreview;
        bool mViewerMode = false;
    };

    QString GenerateEntityScriptSource(QString entity);
    QString GenerateAnimatorScriptSource();

} // namespace
