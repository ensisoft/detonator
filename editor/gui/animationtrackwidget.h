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

#include "base/bitflag.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/treemodel.h"
#include "editor/gui/drawing.h"
#include "editor/app/workspace.h"
#include "gamelib/entity.h"
#include "gamelib/animation.h"
#include "gamelib/renderer.h"
#include "gamelib/physics.h"

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
        AnimationTrackWidget(app::Workspace* workspace, const std::shared_ptr<game::EntityClass>& entity);
        AnimationTrackWidget(app::Workspace* workspace,
                const std::shared_ptr<game::EntityClass>& entity,
                const game::AnimationTrackClass& track);
       ~AnimationTrackWidget();

        // MainWidget implementation.
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Render() override;
        virtual void Update(double secs) override;
        virtual bool ConfirmClose() override;
        virtual bool GetStats(Stats* stats) const override;

        void SetZoom(float zoom);
        void SetShowGrid(bool on_off);
        void SetShowOrigin(bool on_off);
        void SetSnapGrid(bool on_off);
        void SetGrid(GridDensity grid);
    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionReset_triggered();
        void on_actionDeleteActuator_triggered();
        void on_actionClearActuators_triggered();
        void on_btnAddActuator_clicked();
        void on_btnTransformPlus90_clicked();
        void on_btnTransformMinus90_clicked();
        void on_btnTransformReset_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnViewReset_clicked();
        void on_duration_valueChanged(double value);
        void on_delay_valueChanged(double value);
        void on_looping_stateChanged(int);
        void on_timeline_customContextMenuRequested(QPoint);
        void on_actuatorStartTime_valueChanged(double value);
        void on_actuatorEndTime_valueChanged(double value);
        void on_actuatorNode_currentIndexChanged(int index);
        void on_actuatorType_currentIndexChanged(int index);
        void on_transformInterpolation_currentIndexChanged(int index);
        void on_setvalInterpolation_currentIndexChanged(int index);
        void on_setvalParamName_currentIndexChanged(int index);
        void on_kinematicInterpolation_currentIndexChanged(int index);
        void on_transformEndPosX_valueChanged(double value);
        void on_transformEndPosY_valueChanged(double value);
        void on_transformEndSizeX_valueChanged(double value);
        void on_transformEndSizeY_valueChanged(double value);
        void on_transformEndScaleX_valueChanged(double value);
        void on_transformEndScaleY_valueChanged(double value);
        void on_transformEndRotation_valueChanged(double value);
        void on_setvalEndValue_valueChanged(double value);
        void on_kinematicEndVeloX_valueChanged(double value);
        void on_kinematicEndVeloY_valueChanged(double value);
        void on_kinematicEndVeloZ_valueChanged(double value);
        void on_itemFlags_currentIndexChanged(int);
        void on_flagAction_currentIndexChanged();
        void SelectedItemChanged(const TimelineWidget::TimelineItem* item);
        void SelectedItemDragged(const TimelineWidget::TimelineItem* item);
        void ToggleShowResource();
        void AddActuatorAction();
    private:
        void InitScene(unsigned width, unsigned height);
        void PaintScene(gfx::Painter& painter, double secs);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        bool KeyPress(QKeyEvent* key);
        void UpdateTransformActuatorUI();
        void SetSelectedActuatorProperties();
        void AddActuatorFromTimeline(game::ActuatorClass::Type type, float start_time);
        void AddActuator(const game::EntityNodeClass& node, game::ActuatorClass::Type type,
                         float start_time, float duration);
        game::EntityNode* GetCurrentNode();
    private:
        Ui::AnimationTrack mUI;
    private:
        using TreeModel = RenderTreeModel<game::EntityClass>;
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
            std::shared_ptr<game::EntityClass> entity;
            std::shared_ptr<game::AnimationTrackClass> track;
            base::bitflag<game::ActuatorClass::Type> show_flags = {~0u};
        } mState;
        // Current time accumulator.
        float mCurrentTime = 0.0f;
        // Interpolation variables for smoothing view rotations.
        float mViewTransformStartTime = 0.0f;
        float mViewTransformRotation = 0.0f;
        // Entity object is used render the animation track
        // while it's being edited. We use an entity object
        // instead of the entity class object is because there
        // should be no changes to the entity class object's
        // render tree. (i.e. node transformations)
        std::unique_ptr<game::Entity> mEntity;
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
        // Entity object that exists when playing back the animation track.
        std::unique_ptr<game::Entity> mPlaybackAnimation;
        // Original hash (when starting to edit an existing track)
        // used to compare for changes when closing.
        std::size_t mOriginalHash = 0;
        // Renderer for the animation
        game::Renderer mRenderer;
        // Physics engine for the animation
        game::PhysicsEngine mPhysics;
    };

    // Functions used to share animation class objects between AnimationTrackWidget
    // and EntityWidget instances.
    std::shared_ptr<game::EntityClass> FindSharedEntity(size_t hash);
    void ShareEntity(const std::shared_ptr<game::EntityClass>& klass);

} // namespace
