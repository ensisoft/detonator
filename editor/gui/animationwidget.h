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
#  include "ui_animationwidget.h"
#  include <QMenu>
#include "warnpop.h"

#include <memory>
#include <string>

#include "editor/gui/mainwidget.h"
#include "gamelib/animation.h"

namespace gfx {
    class Painter;
} // gfx

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class TreeWidget;

    class AnimationWidget : public MainWidget
    {
        Q_OBJECT
    public:
        AnimationWidget(app::Workspace* workspace);
        AnimationWidget(app::Workspace* workspace, const app::Resource& resource);
       ~AnimationWidget();

        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool CanZoomIn() const override;
        virtual bool CanZoomOut() const override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Update(double secs) override;
        virtual void Render() override;
        virtual bool ConfirmClose() override;
        virtual void Refresh() override;

    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNewRect_triggered();
        void on_actionNewCircle_triggered();
        void on_actionNewIsocelesTriangle_triggered();
        void on_actionNewRightTriangle_triggered();
        void on_actionNewRoundRect_triggered();
        void on_actionNewTrapezoid_triggered();
        void on_actionNewParallelogram_triggered();
        void on_actionDeleteComponent_triggered();
        void on_tree_customContextMenuRequested(QPoint);
        void on_plus90_clicked();
        void on_minus90_clicked();
        void on_resetTransform_clicked();
        void on_cPlus90_clicked();
        void on_cMinus90_clicked();
        void on_materials_currentIndexChanged(const QString& name);
        void on_drawables_currentIndexChanged(const QString& name);
        void on_renderPass_currentIndexChanged(const QString& name);
        void on_renderStyle_currentIndexChanged(const QString& name);
        void on_layer_valueChanged(int layer);
        void on_lineWidth_valueChanged(double value);
        void on_alpha_valueChanged(double value);
        void on_nodeSizeX_valueChanged(double value);
        void on_nodeSizeY_valueChanged(double value);
        void on_nodeTranslateX_valueChanged(double value);
        void on_nodeTranslateY_valueChanged(double value);
        void on_nodeScaleX_valueChanged(double value);
        void on_nodeScaleY_valueChanged(double value);
        void on_nodeRotation_valueChanged(double value);
        void on_nodeName_textChanged(const QString& text);
        void on_chkUpdateMaterial_stateChanged(int);
        void on_chkUpdateDrawable_stateChanged(int);
        void on_chkDoesRender_stateChanged(int);
        void on_chkOverrideAlpha_stateChanged(int);
        void on_btnNewTrack_clicked();
        void on_btnEditTrack_clicked();
        void on_btnDeleteTrack_clicked();
        void on_trackList_itemSelectionChanged();

        void currentComponentRowChanged();
        void placeNewParticleSystem();
        void placeNewCustomShape();
        void newResourceAvailable(const app::Resource* resource);
        void resourceToBeDeleted(const app::Resource* resource);
        void treeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target);
        void treeClickEvent(TreeWidget::TreeItem* item);

    private:
        void paintScene(gfx::Painter& painter, double secs);
        game::AnimationNodeClass* GetCurrentNode();
        void updateCurrentNodeProperties();
        void updateCurrentNodePosition(float dx, float dy);
    private:
        class TreeModel;
        class Tool;
        class PlaceTool;
        class CameraTool;
        class MoveTool;
        class ResizeTool;
        class RotateTool;

    private:
        Ui::AnimationWidget mUI;
        // there doesn't seem to be a way to do this in the designer
        // so we create our menu for user defined drawables
        QMenu* mParticleSystems = nullptr;
        // menu for the custom shapes
        QMenu* mCustomShapes = nullptr;
    private:
        // the grid display options.
        enum class GridDensity {
            Grid10x10 = 10,
            Grid20x20 = 20,
            Grid50x50 = 50,
            Grid100x100 = 100
        };
        // current tool (if any, can be nullptr when no tool is selected).
        std::unique_ptr<Tool> mCurrentTool;
        // state shared with the tools is packed inside a single
        // struct type for convenience
        struct State {
            // shared with the track widget.
            std::shared_ptr<game::AnimationClass> animation;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;

            // current workspace we're editing.
            app::Workspace* workspace = nullptr;
            //
            gui::TreeWidget* scenegraph_tree_view = nullptr;

            TreeModel* scenegraph_tree_model = nullptr;
        } mState;

        // tree model for accessing the animations' render tree
        // data from the tree widget.
        std::unique_ptr<TreeModel> mTreeModel;
        // the identifier string for sharing the animation in the
        // cache with the animation track widget instances.
        std::string mIdentifier;
        // the original hash value that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
        // current time of the animation. accumulates when running.
        float mAnimationTime = 0.0f;
        // possible states of the animation playback.
        enum PlayState {
            Playing, Paused, Stopped
        };
        // current animation playback state.
        PlayState mPlayState = PlayState::Stopped;

        bool mCameraWasLoaded = false;

        float mCurrentTime = 0.0f;
        float mViewTransformStartTime = 0.0f;
        float mViewTransformRotation = 0.0f;
    };
} // namespace
