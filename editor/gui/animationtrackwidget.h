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
#  include "ui_animationtrackwidget.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <unordered_map>

#include "base/bitflag.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/treemodel.h"
#include "editor/gui/drawing.h"
#include "editor/app/workspace.h"
#include "game/entity.h"
#include "game/animation.h"
#include "engine/renderer.h"
#include "engine/physics.h"

namespace gui
{
    class EntityWidget;
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
                const game::AnimationClass& track,
                const QVariantMap& properties);
       ~AnimationTrackWidget();

        // MainWidget implementation.
        virtual QString GetId() const override;
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
        virtual void Save() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool GetStats(Stats* stats) const override;
        virtual bool ShouldClose() const override;

        void SetZoom(float zoom);
        void SetShowGrid(bool on_off);
        void SetShowOrigin(bool on_off);
        void SetShowViewport(bool on_off);
        void SetSnapGrid(bool on_off);
        void SetGrid(GridDensity grid);
        void RealizeEntityChange(std::shared_ptr<const game::EntityClass> klass);
    private slots:
        void on_widgetColor_colorChanged(QColor color);
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionReset_triggered();
        void on_actionDeleteActuator_triggered();
        void on_actionDeleteActuators_triggered();
        void on_actionDeleteTimeline_triggered();
        void on_btnAddActuator_clicked();
        void on_btnTransformPlus90_clicked();
        void on_btnTransformMinus90_clicked();
        void on_btnTransformReset_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnViewReset_clicked();
        void on_trackName_textChanged(const QString&);
        void on_duration_valueChanged(double value);
        void on_delay_valueChanged(double value);
        void on_looping_stateChanged(int);
        void on_timeline_customContextMenuRequested(QPoint);
        void on_actuatorIsStatic_stateChanged(int);
        void on_actuatorName_textChanged(const QString&);
        void on_actuatorStartTime_valueChanged(double value);
        void on_actuatorEndTime_valueChanged(double value);
        void on_actuatorNode_currentIndexChanged(int index);
        void on_actuatorType_currentIndexChanged(int index);
        void on_transformInterpolation_currentIndexChanged(int index);
        void on_setvalInterpolation_currentIndexChanged(int index);
        void on_setvalName_currentIndexChanged(int index);
        void on_kinematicInterpolation_currentIndexChanged(int index);
        void on_transformEndPosX_valueChanged(double value);
        void on_transformEndPosY_valueChanged(double value);
        void on_transformEndSizeX_valueChanged(double value);
        void on_transformEndSizeY_valueChanged(double value);
        void on_transformEndScaleX_valueChanged(double value);
        void on_transformEndScaleY_valueChanged(double value);
        void on_transformEndRotation_valueChanged(double value);
        void on_setvalEndValue_ValueChanged();
        void on_kinematicEndVeloX_valueChanged(double value);
        void on_kinematicEndVeloY_valueChanged(double value);
        void on_kinematicEndVeloZ_valueChanged(double value);
        void on_itemFlags_currentIndexChanged(int);
        void on_flagAction_currentIndexChanged(int index);
        void SelectedItemChanged(const TimelineWidget::TimelineItem* item);
        void SelectedItemDragged(const TimelineWidget::TimelineItem* item);
        void ToggleShowResource();
        void AddActuatorAction();
        void AddNodeTimelineAction();
    private:
        void InitScene(unsigned width, unsigned height);
        void PaintScene(gfx::Painter& painter, double secs);
        void MouseZoom(std::function<void(void)> zoom_function);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        bool KeyPress(QKeyEvent* key);
        void UpdateTransformActuatorUI();
        void UpdateTrackUI();
        void SetSelectedActuatorProperties();
        void SetActuatorUIEnabled(bool enabled);
        void SetActuatorUIDefaults(const std::string& nodeId);
        void AddActuatorFromTimeline(game::ActuatorClass::Type type, float start_time);
        void AddActuatorFromUI(const std::string& timelineId,
                               const std::string& nodeId,
                               const std::string& name,
                               game::ActuatorClass::Type type,
                               float start_time,
                               float duration);
        void DisplayCurrentCameraLocation();
        void CreateTimelines();
        void RemoveDeletedItems();
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
        struct Timeline {
            std::string selfId;
            std::string nodeId;
        };
        // Current state that is accessed and modified by this widget
        // object's event handlers (slots) and also by the current tool.
        struct State {
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
            std::shared_ptr<game::EntityClass> entity;
            std::shared_ptr<game::AnimationClass> track;
            base::bitflag<game::ActuatorClass::Type> show_flags = {~0u};
            std::vector<Timeline> timelines;
            std::unordered_map<std::string, std::string> actuator_to_timeline;
        } mState;
        bool mCameraWasLoaded = false;
        // Current time accumulator.
        float mCurrentTime = 0.0f;
        // Interpolation variables for smoothing view rotations.
        float mViewTransformStartTime = 0.0f;
        float mViewTransformRotation = 0.0f;
        // Entity object is used render the animation track
        // while it's being edited. We use an entity object
        // instead of the entity class object because there
        // should be no changes in the entity class object.
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
        engine::Renderer mRenderer;
        // Physics engine for the animation
        engine::PhysicsEngine mPhysics;
    };

    // Functions used to share state between AnimationTrackWidget
    // and EntityWidget instances.
    std::shared_ptr<game::EntityClass> FindSharedEntity(size_t hash);
    void ShareEntity(const std::shared_ptr<game::EntityClass>& klass);

    void RegisterEntityWidget(EntityWidget* widget);
    void DeleteEntityWidget(EntityWidget* widget);
    void RegisterTrackWidget(AnimationTrackWidget* widget);
    void DeleteTrackWidget(AnimationTrackWidget* widget);
    void RealizeEntityChange(std::shared_ptr<const game::EntityClass> klass);

} // namespace
