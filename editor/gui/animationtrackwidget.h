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
#  include "ui_animationtrackwidget.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>

#include "editor/gui/mainwidget.h"
#include "editor/gui/treemodel.h"
#include "editor/app/workspace.h"
#include "gamelib/animation.h"
#include "gamelib/renderer.h"

namespace gui
{
    class MouseTool;
    // User interface widget for editing animation tracks.
    // All edits are eventually stored in an animation class object,
    // which is shared with AnimationWidget instance. When a track is
    // saved it's saved in the AnimationClass object.
    class AnimationTrackWidget : public MainWidget
    {
        Q_OBJECT

    public:
        // constructors.
        AnimationTrackWidget(app::Workspace* workspace);
        AnimationTrackWidget(app::Workspace* workspace, const std::shared_ptr<game::AnimationClass>& anim);
        AnimationTrackWidget(app::Workspace* workspace,
                const std::shared_ptr<game::AnimationClass>& anim,
                const game::AnimationTrackClass& track);
       ~AnimationTrackWidget();

        // MainWidget implementation.
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
        virtual void Render() override;
        virtual void Update(double secs) override;
        virtual bool ConfirmClose() override;

    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionReset_triggered();
        void on_actionDeleteActuator_triggered();
        void on_actionClearActuators_triggered();
        void on_actionAddTransformActuator_triggered();
        void on_actionAddMaterialActuator_triggered();
        void on_btnAddActuator_clicked();
        void on_btnTransformPlus90_clicked();
        void on_btnTransformMinus90_clicked();
        void on_btnTransformReset_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnViewReset_clicked();
        void on_duration_valueChanged(double value);
        void on_looping_stateChanged(int);
        void on_timeline_customContextMenuRequested(QPoint);
        void on_actuatorStartTime_valueChanged(double value);
        void on_actuatorEndTime_valueChanged(double value);
        void on_actuatorNode_currentIndexChanged(int index);
        void on_actuatorType_currentIndexChanged(int index);
        void on_transformInterpolation_currentIndexChanged(int index);
        void on_materialInterpolation_currentIndexChanged(int index);
        void on_transformEndPosX_valueChanged(double value);
        void on_transformEndPosY_valueChanged(double value);
        void on_transformEndSizeX_valueChanged(double value);
        void on_transformEndSizeY_valueChanged(double value);
        void on_transformEndScaleX_valueChanged(double value);
        void on_transformEndScaleY_valueChanged(double value);
        void on_transformEndRotation_valueChanged(double value);
        void on_materialEndAlpha_valueChanged(double value);
        void on_chkShowMaterialActuators_stateChanged(int);
        void on_chkShowTransformActuators_stateChanged(int);
        void SelectedItemChanged(const TimelineWidget::TimelineItem* item);
        void SelectedItemDragged(const TimelineWidget::TimelineItem* item);
    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void UpdateTransformActuatorUI();
        void SetSelectedActuatorProperties();
    private:
        Ui::AnimationTrack mUI;
    private:
        using TreeModel = RenderTreeModel<game::AnimationClass>;
        class TimelineModel;
        // Tree model to display the animation's render tree.
        std::unique_ptr<TreeModel> mTreeModel;
        // Timeline model to display the actuators on a timeline.
        std::unique_ptr<TimelineModel> mTimelineModel;
        // The current tool that performs actions on user (mouse) input.
        // can be nullptr when no tool is currently being applied.
        std::unique_ptr<MouseTool> mCurrentTool;
        // The workspace object.
        app::Workspace* mWorkspace = nullptr;
        // Current state that is accessed and modified by this widget
        // object's event handlers (slots) and also by the current tool.
        struct State {
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
            std::shared_ptr<game::AnimationClass> animation;
            std::shared_ptr<game::AnimationTrackClass> track;
            bool show_material_actuators = true;
            bool show_transform_actuators = true;
        } mState;
        // Current time accumulator.
        float mCurrentTime = 0.0f;
        // Interpolation variables for smoothing view rotations.
        float mViewTransformStartTime = 0.0f;
        float mViewTransformRotation = 0.0f;
        // Animation object is used render the animation track
        // while it's being edited. We use an animation object
        // instead of the animation class object is because there
        // should be no changes to the animation class object's
        // render tree. (i.e. node transformations)
        std::unique_ptr<game::Animation> mAnimation;
        // State enumeration for the playback state.
        enum class PlayState {
            // Currently playing back an animation track.
            Playing,
            // Have an animation track but the time accumulation
            // and updates are paused.
            Paused,
            // No animation track. Editing actuators state.
            Stopped
        };
        // The current playback/editing state.
        PlayState mPlayState = PlayState::Stopped;
        // Animation object that exists when playing back the
        // animation track.
        std::unique_ptr<game::Animation> mPlaybackAnimation;
        // Original hash (when starting to edit an existing track)
        // used to compare for changes when closing.
        std::size_t mOriginalHash = 0;
        // Renderer for the animation
        game::Renderer mRenderer;
    };

    // Functions used to share animation class objects between AnimationTrackWidget
    // and AnimationWidget instances.
    std::shared_ptr<game::AnimationClass> FindSharedAnimation(size_t hash);
    void ShareAnimation(const std::shared_ptr<game::AnimationClass>& klass);

} // namespace
