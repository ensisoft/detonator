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

#define LOGTAG "entity"

#include "warnpush.h"
#  include <QMessageBox>
#  include <QVector2D>
#  include <QMenu>
#  include <base64/base64.h>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <unordered_map>
#include <unordered_set>

#include "base/math.h"
#include "data/json.h"
#include "base/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/gui/dlgmaterialparams.h"
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/entitywidget.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/tool.h"
#include "editor/gui/nerd.h"
#include "editor/gui/translation.h"
#include "game/animation.h"
#include "game/animator.h"
#include "game/transform_animator.h"
#include "game/kinematic_animator.h"
#include "game/material_animator.h"
#include "game/property_animator.h"
#include "graphics/transform.h"
#include "graphics/painter.h"

namespace {
    // shared entity class objects.
    // shared between entity widget and this track widget
    // whenever an editor session is restored.
    std::unordered_map<size_t,
            std::weak_ptr<game::EntityClass>> SharedAnimations;
    std::unordered_set<gui::EntityWidget*> EntityWidgets;
    std::unordered_set<gui::AnimationTrackWidget*> TrackWidgets;
} // namespace

namespace gui
{

class AnimationTrackWidget::TimelineModel : public TimelineWidget::TimelineModel
{
public:
    TimelineModel(AnimationTrackWidget::State& state) : mState(state)
    {}
    virtual void Fetch(std::vector<TimelineWidget::Timeline>* list) override
    {
        const auto& track = *mState.track;
        const auto& anim  = *mState.entity;
        // map node ids to indices in the animation's list of nodes.
        std::unordered_map<std::string, size_t> id_to_index_map;

        // setup all timelines with empty item vectors.
        for (const auto& item : mState.timelines)
        {
            const auto* node = mState.entity->FindNodeById(item.nodeId);
            const auto& name = node->GetName();
            // map timeline objects to widget timeline indices
            id_to_index_map[item.selfId] = list->size();
            TimelineWidget::Timeline line;
            line.SetName(app::FromUtf8(name));
            list->push_back(line);
        }
        // go over the existing actuators and create timeline items
        // for visual representation of each actuator.
        for (size_t i=0; i< track.GetNumAnimators(); ++i)
        {
            const auto& actuator = track.GetAnimatorClass(i);
            const auto type  = actuator.GetType();
            if (!mState.show_flags.test(type))
                continue;

            const auto& nodeId = actuator.GetNodeId();
            const auto& lineId = mState.actuator_to_timeline[actuator.GetId()];
            const auto* node = mState.entity->FindNodeById(nodeId);
            const auto& node_name = app::FromUtf8(node->GetName());
            const auto& actuator_name = app::FromUtf8(actuator.GetName());
            const auto index = id_to_index_map[lineId];
            const auto num   = (*list)[index].GetNumItems();

            // pastel color palette.
            // https://colorhunt.co/palette/226038

            TimelineWidget::TimelineItem item;
            item.text      = app::toString("%1 (%2)", node_name, actuator_name);
            item.id        = app::FromUtf8(actuator.GetId());
            item.starttime = actuator.GetStartTime();
            item.duration  = actuator.GetDuration();
            if (type == game::AnimatorClass::Type::BooleanPropertyAnimator)
                item.color = QColor(0xa3, 0xdd, 0xcb, 150);
            else if (type == game::AnimatorClass::Type::TransformAnimator)
                item.color = QColor(0xe8, 0xe9, 0xa1, 150);
            else if (type == game::AnimatorClass::Type::KinematicAnimator)
                item.color = QColor(0xe6, 0xb5, 0x66, 150);
            else if (type == game::AnimatorClass::Type::PropertyAnimator)
                item.color = QColor(0xe5, 0x70, 0x7e, 150);
            else if (type == game::AnimatorClass::Type::MaterialAnimator)
                item.color = QColor(0xe7, 0x80, 0x7e, 150);
            else BUG("Unhandled type for item colorization.");

            (*list)[index].AddItem(item);
        }
        for (auto& timeline : (*list))
        {
            if (timeline.GetNumItems())
                timeline.SetName("");
        }
    }
private:
    AnimationTrackWidget::State& mState;
};


AnimationTrackWidget::AnimationTrackWidget(app::Workspace* workspace)
    : mWorkspace(workspace)
{
    DEBUG("Create AnimationTrackWidget");
    mTimelineModel = std::make_unique<TimelineModel>(mState);
    mRenderer.SetClassLibrary(workspace);
    mRenderer.SetEditingMode(true);

    mUI.setupUi(this);
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.timeline->SetModel(mTimelineModel.get());

    const auto& settings = workspace->GetProjectSettings();
    PopulateFromEnum<engine::RenderingStyle>(mUI.cmbStyle);
    PopulateFromEnum<game::TransformAnimatorClass::Interpolation>(mUI.transformInterpolation);
    PopulateFromEnum<game::PropertyAnimatorClass::Interpolation>(mUI.setvalInterpolation);
    PopulateFromEnum<game::PropertyAnimatorClass::PropertyName>(mUI.setvalName);
    PopulateFromEnum<game::KinematicAnimatorClass::Interpolation>(mUI.kinematicInterpolation);
    PopulateFromEnum<game::KinematicAnimatorClass::Target>(mUI.kinematicTarget);
    PopulateFromEnum<game::BooleanPropertyAnimatorClass::PropertyName>(mUI.itemFlags, true, true);
    PopulateFromEnum<game::BooleanPropertyAnimatorClass::PropertyAction>(mUI.flagAction, true, true);
    PopulateFromEnum<game::MaterialAnimatorClass::Interpolation >(mUI.materialInterpolation);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.actionUsePhysics, settings.enable_physics);
    SetValue(mUI.zoom, 1.0f);
    SetVisible(mUI.transform, false);
    setFocusPolicy(Qt::StrongFocus);

    mUI.widget->onZoomIn       = [this]() { MouseZoom(std::bind(&AnimationTrackWidget::ZoomIn, this)); };
    mUI.widget->onZoomOut      = [this]() { MouseZoom(std::bind(&AnimationTrackWidget::ZoomOut, this)); };
    mUI.widget->onMouseMove    = std::bind(&AnimationTrackWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&AnimationTrackWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&AnimationTrackWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onPaintScene   = std::bind(&AnimationTrackWidget::PaintScene, this,
                                           std::placeholders::_1, std::placeholders::_2);

    connect(mUI.timeline, &TimelineWidget::SelectedItemChanged,
        this, &AnimationTrackWidget::SelectedItemChanged);
    connect(mUI.timeline, &TimelineWidget::SelectedItemDragged,
            this, &AnimationTrackWidget::SelectedItemDragged);

    SetActuatorUIDefaults();
    SetActuatorUIEnabled(false);
    RegisterTrackWidget(this);
}

AnimationTrackWidget::AnimationTrackWidget(app::Workspace* workspace, const std::shared_ptr<game::EntityClass>& entity)
    : AnimationTrackWidget(workspace)
{
    // create a new animation track for the given entity.
    mState.entity = entity;
    mState.track  = std::make_shared<game::AnimationClass>();
    mState.track->SetDuration(10.0f);
    mState.track->SetName("My Track");
    mState.track->SetLooping(false);
    mOriginalHash = mState.track->GetHash();
    mEntity = game::CreateEntityInstance(mState.entity);

    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();

    RemoveDeletedItems();
    CreateTimelines();
    mUI.timeline->SetDuration(mState.track->GetDuration());
    mUI.timeline->Rebuild();

    UpdateTrackUI();
}

AnimationTrackWidget::AnimationTrackWidget(app::Workspace* workspace,
        const std::shared_ptr<game::EntityClass>& entity,
        const game::AnimationClass& track,
        const QVariantMap& properties)
        : AnimationTrackWidget(workspace)
{
    // Edit an existing animation track for the given animation.
    mState.entity = entity;
    mState.track  = std::make_shared<game::AnimationClass>(track); // edit a copy.
    mOriginalHash = mState.track->GetHash();
    mEntity = game::CreateEntityInstance(mState.entity);

    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();

    // Create timelines based on the existing properties.
    ASSERT(!properties.isEmpty());
    const int num_timelines = properties["num_timelines"].toInt();
    for (int i = 0; i < num_timelines; ++i)
    {
        Timeline tl;
        tl.selfId = app::ToUtf8(properties[QString("timeline_%1_self_id").arg(i)].toString());
        tl.nodeId = app::ToUtf8(properties[QString("timeline_%1_node_id").arg(i)].toString());
        mState.timelines.push_back(std::move(tl));
    }
    for (size_t i = 0; i < mState.track->GetNumAnimators(); ++i)
    {
        const auto& actuator = mState.track->GetAnimatorClass(i);
        const auto& timeline = properties[app::FromUtf8(actuator.GetId())].toString();
        mState.actuator_to_timeline[actuator.GetId()] = app::ToUtf8(timeline);
    }

    if (properties.contains("main_splitter"))
        mUI.mainSplitter->restoreState(QByteArray::fromBase64(properties["main_splitter"].toString().toLatin1()));
    if (properties.contains("center_splitter"))
        mUI.centerSplitter->restoreState(QByteArray::fromBase64(properties["center_splitter"].toString().toLatin1()));
    if (properties.contains("timeline_splitter"))
        mUI.timelineSplitter->restoreState(QByteArray::fromBase64(properties["timeline_splitter"].toString().toLatin1()));

    RemoveDeletedItems();
    CreateTimelines();

    mUI.timeline->SetDuration(track.GetDuration());
    mUI.timeline->Rebuild();

    UpdateTrackUI();

    if (properties.contains("use_physics"))
    {
        SetValue(mUI.actionUsePhysics, properties["use_physics"].toBool());
    }
}
AnimationTrackWidget::~AnimationTrackWidget()
{
    DEBUG("Destroy AnimationTrackWidget");

    DeleteTrackWidget(this);
}

QString AnimationTrackWidget::GetId() const
{
    return GetValue(mUI.trackID);
}

void AnimationTrackWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionUsePhysics);
    bar.addSeparator();
    bar.addAction(mUI.actionReset);
}
void AnimationTrackWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionUsePhysics);
    menu.addSeparator();
    menu.addAction(mUI.actionReset);
}
bool AnimationTrackWidget::SaveState(Settings& settings) const
{
    data::JsonObject entity;
    data::JsonObject track;
    mState.entity->IntoJson(entity);
    mState.track->IntoJson(track);
    settings.SetValue("TrackWidget", "entity", entity);
    settings.SetValue("TrackWidget", "track", track);
    settings.SetValue("TrackWidget", "hash", mOriginalHash);
    settings.SetValue("TrackWidget", "show_bits", mState.show_flags.value());
    settings.SetValue("TrackWidget", "camera_offset_x", mState.camera_offset_x);
    settings.SetValue("TrackWidget", "camera_offset_y", mState.camera_offset_y);
    settings.SetValue("TrackWidget", "num_timelines", (unsigned)mState.timelines.size());

    for (int i=0; i<(unsigned)mState.timelines.size(); ++i)
    {
        const auto& timeline = mState.timelines[i];
        settings.SetValue("TrackWidget", QString("timeline_%1_self_id").arg(i), timeline.selfId);
        settings.SetValue("TrackWidget", QString("timeline_%1_node_id").arg(i), timeline.nodeId);
    }
    for (const auto& p : mState.actuator_to_timeline)
    {
        settings.SetValue("TrackWidget", app::FromUtf8(p.first), p.second);
    }

    settings.SaveWidget("TrackWidget", mUI.scaleX);
    settings.SaveWidget("TrackWidget", mUI.scaleY);
    settings.SaveWidget("TrackWidget", mUI.rotation);
    settings.SaveWidget("TrackWidget", mUI.zoom);
    settings.SaveWidget("TrackWidget", mUI.cmbGrid);
    settings.SaveWidget("TrackWidget", mUI.cmbStyle);
    settings.SaveWidget("TrackWidget", mUI.chkShowOrigin);
    settings.SaveWidget("TrackWidget", mUI.chkShowGrid);
    settings.SaveWidget("TrackWidget", mUI.chkShowViewport);
    settings.SaveWidget("TrackWidget", mUI.chkSnap);
    settings.SaveWidget("TrackWidget", mUI.widget);
    settings.SaveWidget("TrackWidget", mUI.mainSplitter);
    settings.SaveWidget("TrackWidget", mUI.timelineSplitter);
    settings.SaveWidget("TrackWidget", mUI.centerSplitter);
    settings.SaveAction("TrackWidget", mUI.actionUsePhysics);
    return true;
}
bool AnimationTrackWidget::LoadState(const Settings& settings)
{
    data::JsonObject entity;
    data::JsonObject track;
    unsigned num_timelines = 0;
    unsigned show_bits = ~0u;

    settings.GetValue("TrackWidget", "entity", &entity);
    settings.GetValue("TrackWidget", "track", &track);
    settings.GetValue("TrackWidget", "hash", &mOriginalHash);
    settings.GetValue("TrackWidget", "show_bits", &show_bits);
    settings.GetValue("TrackWidget", "num_timelines", &num_timelines);
    settings.GetValue("TrackWidget", "camera_offset_x", &mState.camera_offset_x);
    settings.GetValue("TrackWidget", "camera_offset_y", &mState.camera_offset_y);

    for (unsigned i=0; i<num_timelines; ++i)
    {
        Timeline tl;
        settings.GetValue("TrackWidget", QString("timeline_%1_self_id").arg(i), &tl.selfId);
        settings.GetValue("TrackWidget", QString("timeline_%1_node_id").arg(i), &tl.nodeId);
        mState.timelines.push_back(std::move(tl));
    }
    mState.show_flags.set_from_value(show_bits);

    settings.LoadWidget("TrackWidget", mUI.scaleX);
    settings.LoadWidget("TrackWidget", mUI.scaleY);
    settings.LoadWidget("TrackWidget", mUI.rotation);
    settings.LoadWidget("TrackWidget", mUI.zoom);
    settings.LoadWidget("TrackWidget", mUI.cmbGrid);
    settings.LoadWidget("TrackWidget", mUI.cmbStyle);
    settings.LoadWidget("TrackWidget", mUI.chkShowOrigin);
    settings.LoadWidget("TrackWidget", mUI.chkShowGrid);
    settings.LoadWidget("TrackWidget", mUI.chkShowViewport);
    settings.LoadWidget("TrackWidget", mUI.chkSnap);
    settings.LoadWidget("TrackWidget", mUI.widget);
    settings.LoadWidget("TrackWidget", mUI.mainSplitter);
    settings.LoadWidget("TrackWidget", mUI.timelineSplitter);
    settings.LoadWidget("TrackWidget", mUI.centerSplitter);
    settings.LoadAction("TrackWidget", mUI.actionUsePhysics);

    // restore entity
    {
        game::EntityClass klass;
        if (!klass.FromJson(entity))
            WARN("Failed to restore entity class state.");

        auto hash = klass.GetHash();
        mState.entity = FindSharedEntity(hash);
        if (!mState.entity)
        {
            auto shared = std::make_shared<game::EntityClass>(std::move(klass));
            mState.entity = shared;
            ShareEntity(shared);
        }
    }
    // restore the track state.
    {
        mState.track = std::make_shared<game::AnimationClass>();
        if (!mState.track->FromJson(track))
            WARN("Failed to restore entity animation track state.");
    }

    mEntity = game::CreateEntityInstance(mState.entity);
    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();

    for (size_t i=0; i< mState.track->GetNumAnimators(); ++i)
    {
        const auto& actuator = mState.track->GetAnimatorClass(i);
        std::string trackId;
        settings.GetValue("TrackWidget", app::FromUtf8(actuator.GetId()), &trackId);
        mState.actuator_to_timeline[actuator.GetId()] = trackId;
    }
    mUI.timeline->SetDuration(mState.track->GetDuration());
    mUI.timeline->Rebuild();

    RemoveDeletedItems();

    UpdateTrackUI();
    return true;
}

bool AnimationTrackWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanCut:
        case Actions::CanCopy:
        case Actions::CanPaste:
            return false;
        case Actions::CanUndo:
            return false;
        case Actions::CanReloadTextures:
        case Actions::CanReloadShaders:
            return true;
        case Actions::CanZoomIn: {
            const float max = mUI.zoom->maximum();
            const float val = GetValue(mUI.zoom);
            return val < max;
        } break;
        case Actions::CanZoomOut: {
            const float min = mUI.zoom->minimum();
            const float val = GetValue(mUI.zoom);
            return val > min;
        } break;
    }
    return false;
}

void AnimationTrackWidget::ZoomIn()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value + 0.1f);
}

void AnimationTrackWidget::ZoomOut()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value - 0.1f);
}

void AnimationTrackWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void AnimationTrackWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void AnimationTrackWidget::Shutdown()
{
    mUI.widget->dispose();
}
void AnimationTrackWidget::Render()
{
    mUI.widget->triggerPaint();
}
void AnimationTrackWidget::Update(double secs)
{
    mCurrentTime += secs;

    mAnimator.Update(mUI, mState);

    // Playing back an animation track ?
    if (mPlayState != PlayState::Playing)
    {
        if (mPlayState == PlayState::Stopped)
        {
            mRenderer.UpdateRendererState(*mEntity);
        }
        return;
    }

    ASSERT(mPlayState == PlayState::Playing);

    mPlaybackAnimation->Update(secs);

    if (mPhysics.HaveWorld())
    {
        mPhysics.UpdateWorld(*mPlaybackAnimation);
        mPhysics.Step();
        mPhysics.UpdateEntity(*mPlaybackAnimation);
    }
    mRenderer.UpdateRendererState(*mPlaybackAnimation);
    mRenderer.Update(*mPlaybackAnimation, mCurrentTime, secs);

    if (mPlaybackAnimation->IsAnimating())
    {
        const auto* track = mPlaybackAnimation->GetCurrentAnimation(0);
        mAnimationTime = track->GetCurrentTime();
        mUI.timeline->SetCurrentTime(mAnimationTime);
        mUI.timeline->Repaint();
        return;
    }

    mAnimationTime = mState.track->GetDuration();

    mPhysics.DeleteAll();
    mPhysics.DestroyWorld();
    // don't reset the animation entity here but leave it so that the user
    // can see the end state of the animation.
    //mPlaybackAnimation.reset();
    mUI.timeline->SetCurrentTime(mAnimationTime);
    mUI.timeline->Update();
    mUI.timeline->SetFreezeItems(false);

    SetEnabled(mUI.actionPlay,    true);
    SetEnabled(mUI.actionPause,   false);
    SetEnabled(mUI.actionStop,    false);
    SetEnabled(mUI.actionReset,   true);
    SetEnabled(mUI.actuatorGroup, true);
    SetEnabled(mUI.baseGroup,     true);

    mPlayState = PlayState::Stopped;
    mPlaybackAnimation.reset();
    ReturnToDefault();
    SelectedItemChanged(mUI.timeline->GetSelectedItem());

    mRenderer.ClearPaintState();
    NOTE("Animation finished.");
    DEBUG("Animation finished.");
}

void AnimationTrackWidget::Save()
{
    on_actionSave_triggered();
}

bool AnimationTrackWidget::HasUnsavedChanges() const
{
    if (!mOriginalHash)
        return false;
    const auto hash = mState.track->GetHash();
    return hash != mOriginalHash;
}

bool AnimationTrackWidget::GetStats(Stats* stats) const
{
    if (mPlaybackAnimation)
    {
        stats->time = mAnimationTime;
    }
    stats->graphics.valid = true;
    stats->graphics.fps   = mUI.widget->getCurrentFPS();
    stats->graphics.vsync = mUI.widget->haveVSYNC();
    const auto& dev_stats = mUI.widget->getDeviceResourceStats();
    stats->device.static_vbo_mem_alloc    = dev_stats.static_vbo_mem_alloc;
    stats->device.static_vbo_mem_use      = dev_stats.static_vbo_mem_use;
    stats->device.dynamic_vbo_mem_alloc   = dev_stats.dynamic_vbo_mem_alloc;
    stats->device.dynamic_vbo_mem_use     = dev_stats.dynamic_vbo_mem_use;
    stats->device.streaming_vbo_mem_use   = dev_stats.streaming_vbo_mem_use;
    stats->device.streaming_vbo_mem_alloc = dev_stats.streaming_vbo_mem_alloc;
    return true;
}

bool AnimationTrackWidget::ShouldClose() const
{
    // these 2 widget types are basically very tightly coupled
    // and they share information using these global data structures.
    // When the entity widget that is used to edit this animation track
    // has been closed (i.e. no longer found in list of entity widgets)
    // this track widget should also close.
    for (auto* widget : EntityWidgets)
    {
        if (widget->GetEntityId() == mState.entity->GetId())
            return false;
    }
    return true;
}

void AnimationTrackWidget::SetZoom(float zoom)
{
    SetValue(mUI.zoom, zoom);
}
void AnimationTrackWidget::SetShowGrid(bool on_off)
{
    SetValue(mUI.chkShowGrid, on_off);
}
void AnimationTrackWidget::SetShowOrigin(bool on_off)
{
    SetValue(mUI.chkShowOrigin, on_off);
}
void AnimationTrackWidget::SetSnapGrid(bool on_off)
{
    SetValue(mUI.chkSnap, on_off);
}
void AnimationTrackWidget::SetGrid(GridDensity grid)
{
    SetValue(mUI.cmbGrid, grid);
}
void AnimationTrackWidget::SetShowViewport(bool on_off)
{
    SetValue(mUI.chkShowViewport, on_off);
}

void AnimationTrackWidget::SetRenderingStyle(engine::RenderingStyle style)
{
    SetValue(mUI.cmbStyle, style);
}

void AnimationTrackWidget::RealizeEntityChange(std::shared_ptr<const game::EntityClass> klass)
{
    if (klass->GetId() != mState.entity->GetId())
        return;

    on_actionStop_triggered();

    RemoveDeletedItems();
    CreateTimelines();

    UpdateTrackUI();
    SetActuatorUIEnabled(false);
    SetActuatorUIDefaults();

    mEntity = game::CreateEntityInstance(mState.entity);
    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();
    mUI.timeline->ClearSelection();
    mUI.timeline->Rebuild();
    mRenderer.ClearPaintState();
}

void AnimationTrackWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void AnimationTrackWidget::on_actionPlay_triggered()
{
    if (mPlayState == PlayState::Paused)
    {
        mPlayState = PlayState::Playing;
        SetEnabled(mUI.actionPause, true);
        return;
    }

    const auto& settings = mWorkspace->GetProjectSettings();
    const bool use_physics = GetValue(mUI.actionUsePhysics);

    // create new animation instance and play the animation track.
    auto track = game::CreateAnimationInstance(mState.track);
    mPlaybackAnimation = game::CreateEntityInstance(mState.entity);
    mPlaybackAnimation->PlayAnimation(std::move(track));
    mPhysics.SetClassLibrary(mWorkspace);
    mPhysics.SetScale(settings.physics_scale);
    mPhysics.SetGravity(settings.physics_gravity);
    mPhysics.SetNumVelocityIterations(settings.num_velocity_iterations);
    mPhysics.SetNumPositionIterations(settings.num_position_iterations);
    mPhysics.SetTimestep(1.0f / settings.updates_per_second);
    mPhysics.DestroyWorld();
    if (use_physics)
        mPhysics.CreateWorld(*mPlaybackAnimation);

    mPlayState = PlayState::Playing;
    mAnimationTime = 0.0f;

    SetEnabled(mUI.actionPlay,    false);
    SetEnabled(mUI.actionPause,   true);
    SetEnabled(mUI.actionStop,    true);
    SetEnabled(mUI.actionReset,   false);
    SetEnabled(mUI.actuatorGroup, false);
    SetEnabled(mUI.baseGroup,     false);

    mUI.timeline->SetFreezeItems(true);
    mUI.timeline->SetCurrentTime(0.0f);
    mUI.timeline->Repaint();

    mRenderer.ClearPaintState();
}

void AnimationTrackWidget::on_actionPause_triggered()
{
    mPlayState = PlayState::Paused;
    SetEnabled(mUI.actionPlay, true);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, true);
}

void AnimationTrackWidget::on_actionStop_triggered()
{
    mPlayState = PlayState::Stopped;
    SetEnabled(mUI.actionPlay,  true);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop,  false);
    SetEnabled(mUI.actionReset, true);
    SetEnabled(mUI.actuatorGroup, true);
    SetEnabled(mUI.baseGroup,     true);

    mUI.timeline->SetFreezeItems(false);
    mUI.timeline->SetCurrentTime(0.0f);
    mUI.timeline->Update();
    mPlaybackAnimation.reset();

    mRenderer.ClearPaintState();
}

void AnimationTrackWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.trackName))
        return;
    mState.track->SetName(GetValue(mUI.trackName));
    mOriginalHash = mState.track->GetHash();
    EntityWidget* parent = nullptr;
    for (auto* widget : EntityWidgets)
    {
        if (widget->GetEntityId() == mState.entity->GetId())
        {
            parent = widget;
            break;
        }
    }
    ASSERT(parent);

    QVariantMap properties;
    properties["main_splitter"]     = QString::fromLatin1(mUI.mainSplitter->saveState().toBase64());
    properties["center_splitter"]   = QString::fromLatin1(mUI.centerSplitter->saveState().toBase64());
    properties["timeline_splitter"] = QString::fromLatin1(mUI.timelineSplitter->saveState().toBase64());
    properties["use_physics"]   = (bool)GetValue(mUI.actionUsePhysics);
    properties["num_timelines"] = (int)mState.timelines.size();
    for (int i=0; i<(int)mState.timelines.size(); ++i)
    {
        const auto& timeline = mState.timelines[i];
        properties[QString("timeline_%1_self_id").arg(i)] = app::FromUtf8(timeline.selfId);
        properties[QString("timeline_%1_node_id").arg(i)] = app::FromUtf8(timeline.nodeId);
    }
    for (const auto& p : mState.actuator_to_timeline)
    {
        properties[app::FromUtf8(p.first)] = app::FromUtf8(p.second);
    }

    parent->SaveAnimation(*mState.track, properties);

    setWindowTitle(GetValue(mUI.trackName));
}

void AnimationTrackWidget::on_actionReset_triggered()
{
    if (mPlayState != PlayState::Stopped)
        return;
    mPlaybackAnimation.reset();
    mEntity = game::CreateEntityInstance(mState.entity);
    mUI.tree->Rebuild();
    mUI.timeline->Rebuild();
    mUI.timeline->ClearSelection();
    SetActuatorUIDefaults();
    SetActuatorUIEnabled(false);
}

void AnimationTrackWidget::on_actionDeleteActuator_triggered()
{
    if (auto* actuator = GetCurrentActuator())
    {
        mState.track->DeleteAnimatorById(actuator->GetId());
        mUI.timeline->ClearSelection();
        mUI.timeline->Rebuild();
        SetActuatorUIDefaults();
        SetActuatorUIEnabled(false);
        ReturnToDefault();
    }
}

void AnimationTrackWidget::on_actionDeleteActuators_triggered()
{
    mState.track->Clear();
    mUI.timeline->ClearSelection();
    mUI.timeline->Rebuild();
    SetActuatorUIDefaults();
    SetActuatorUIEnabled(false);
    ReturnToDefault();
}

void AnimationTrackWidget::on_actionDeleteTimeline_triggered()
{
    if (!mUI.timeline->GetCurrentTimeline())
        return;
    const auto index = mUI.timeline->GetCurrentTimelineIndex();
    ASSERT(index < mState.timelines.size());
    auto tl = mState.timelines[index];

    size_t count = 0;
    for (size_t i=0; i< mState.track->GetNumAnimators(); ++i)
    {
        const auto& actuator = mState.track->GetAnimatorClass(i);
        const auto& timeline = mState.actuator_to_timeline[actuator.GetId()];
        if (timeline == tl.selfId)
            ++count;
    }

    if (count > 0)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle("Delete Timeline ?");
        msg.setText("Timeline contains multiple actuators. Are you sure you want to delete?");
        msg.setStandardButtons(QMessageBox::StandardButton::Yes |
                               QMessageBox::StandardButton::No);
        if (msg.exec() == QMessageBox::StandardButton::No)
            return;
    }

    for (size_t i=0; i< mState.track->GetNumAnimators();)
    {
        const auto& actuator = mState.track->GetAnimatorClass(i);
        const auto& timeline = mState.actuator_to_timeline[actuator.GetId()];
        if (timeline == tl.selfId) {
            mState.track->DeleteAnimator(i);
        } else {
            ++i;
        }
    }
    mState.timelines.erase(mState.timelines.begin() + index);
    mUI.timeline->Rebuild();
    SetActuatorUIDefaults();
    SetActuatorUIEnabled(false);
    ReturnToDefault();
}

void AnimationTrackWidget::on_trackName_textChanged(const QString&)
{
    mState.track->SetName(GetValue(mUI.trackName));
}

void AnimationTrackWidget::on_duration_valueChanged(double value)
{
    if (auto* item = GetCurrentTimelineItem())
    {
        const float duration = mState.track->GetDuration();
        const auto start_lo_bound = mUI.actuatorStartTime->minimum();
        const auto start_hi_bound = mUI.actuatorStartTime->maximum();
        const auto end_lo_bound = mUI.actuatorEndTime->minimum();
        const auto end_hi_bound = mUI.actuatorEndTime->maximum();
        // important, must get the current value *before* setting new bounds
        // since setting the bounds will adjust the value.
        const float start = GetValue(mUI.actuatorStartTime);
        const float end = GetValue(mUI.actuatorEndTime);
        SetMinMax(mUI.actuatorStartTime, start_lo_bound / duration * value,
                  start_hi_bound / duration * value);
        SetMinMax(mUI.actuatorEndTime, end_lo_bound / duration * value,
                  end_hi_bound / duration * value);
        SetValue(mUI.actuatorStartTime, start / duration * value);
        SetValue(mUI.actuatorEndTime, end / duration * value);
    }

    mUI.timeline->SetDuration(value);
    mUI.timeline->Update();
    mState.track->SetDuration(value);
}

void AnimationTrackWidget::on_delay_valueChanged(double value)
{
    mState.track->SetDelay(value);
}

void AnimationTrackWidget::on_looping_stateChanged(int)
{
    mState.track->SetLooping(GetValue(mUI.looping));
}

void AnimationTrackWidget::on_actuatorIsStatic_stateChanged(int)
{
    SetSelectedActuatorProperties();
}

void AnimationTrackWidget::on_actuatorName_textChanged(const QString&)
{
    SetSelectedActuatorProperties();
}

void AnimationTrackWidget::on_actuatorStartTime_valueChanged(double value)
{
    if (auto* actuator = GetCurrentActuator())
    {
        const auto duration = mState.track->GetDuration();
        const auto old_start = actuator->GetStartTime();
        const auto new_start = value / duration;
        const auto change = old_start - new_start;
        actuator->SetStartTime(new_start);
        actuator->SetDuration(actuator->GetDuration() + change);
        mUI.timeline->Rebuild();
    }
}
void AnimationTrackWidget::on_actuatorEndTime_valueChanged(double value)
{
    if (auto* actuator = GetCurrentActuator())
    {
        const auto duration = mState.track->GetDuration();
        const auto start = actuator->GetStartTime();
        const auto end = value / duration;
        actuator->SetDuration(end - start);
        mUI.timeline->Rebuild();
    }
}

void AnimationTrackWidget::on_transformInterpolation_currentIndexChanged(int index)
{
    SetSelectedActuatorProperties();
}
void AnimationTrackWidget::on_setvalInterpolation_currentIndexChanged(int index)
{
    SetSelectedActuatorProperties();
}
void AnimationTrackWidget::on_setvalName_currentIndexChanged(int index)
{
    SetSelectedActuatorProperties();
}
void AnimationTrackWidget::on_kinematicInterpolation_currentIndexChanged(int index)
{
    SetSelectedActuatorProperties();
}
void AnimationTrackWidget::on_kinematicTarget_currentIndexChanged(int index)
{
    SetSelectedActuatorProperties();
    UpdateKinematicUnits();
}

void AnimationTrackWidget::on_timeline_customContextMenuRequested(QPoint)
{
    const auto* selected = mUI.timeline->GetSelectedItem();
    mUI.actionDeleteActuator->setEnabled(selected != nullptr);
    mUI.actionDeleteActuators->setEnabled(mState.track->GetNumAnimators());

    // map the click point to a position in the timeline.
    const auto* timeline = mUI.timeline->GetCurrentTimeline();
    mUI.actionDeleteTimeline->setEnabled(timeline != nullptr);

    // build menu for adding timelines
    QMenu add_timeline(this);
    add_timeline.setEnabled(true);
    add_timeline.setIcon(QIcon("icons:add.png"));
    add_timeline.setTitle(tr("New Timeline On ..."));
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        QAction* action = add_timeline.addAction(app::FromUtf8(node.GetName()));
        action->setEnabled(true);
        action->setData(app::FromUtf8(node.GetId()));
        connect(action, &QAction::triggered, this, &AnimationTrackWidget::AddNodeTimelineAction);
    }

    QMenu show(this);
    show.setTitle(tr("Show Animators ..."));
    for (const auto val : magic_enum::enum_values<game::AnimatorClass::Type>())
    {
        QAction* action = show.addAction(app::FromUtf8(TranslateEnum(val)));
        connect(action, &QAction::toggled, this, &AnimationTrackWidget::ToggleShowResource);
        action->setData(magic_enum::enum_integer(val));
        action->setCheckable(true);
        SetValue(action, mState.show_flags.test(val));
    }

    QMenu menu(this);
    QMenu actuators(this);
    actuators.setEnabled(selected == nullptr);
    actuators.setTitle("New Animator ...");
    actuators.setIcon(QIcon("icons:add.png"));

    struct Actuator {
        game::AnimatorClass::Type type;
        QString name;
    };
    std::vector<Actuator> actuator_list;

    actuator_list.push_back( {game::AnimatorClass::Type::TransformAnimator,       "Transform Animator" });
    actuator_list.push_back( {game::AnimatorClass::Type::KinematicAnimator,       "Kinematic Animator on Rigid Body" });
    actuator_list.push_back( {game::AnimatorClass::Type::KinematicAnimator,       "Kinematic Animator on Node Transformer" });
    actuator_list.push_back( {game::AnimatorClass::Type::MaterialAnimator,        "Material Animator" });
    actuator_list.push_back( {game::AnimatorClass::Type::PropertyAnimator,        "Property Animator" });
    actuator_list.push_back( {game::AnimatorClass::Type::BooleanPropertyAnimator, "Property Animator (Boolean))" });

    // build menu for adding actuators.
    for (const auto& act : actuator_list)
    {
        QAction* action = actuators.addAction(act.name);
        action->setIcon(QIcon("icons:add.png"));
        action->setProperty("type", static_cast<int>(act.type));
        action->setEnabled(false);
        if (act.name == "Rigid Body")
            action->setProperty("kinematic_target", static_cast<int>(game::KinematicAnimatorClass::Target::RigidBody));
        else if (act.name == "Transformer")
            action->setProperty("kinematic_target", static_cast<int>(game::KinematicAnimatorClass::Target::Transformer));

        connect(action, &QAction::triggered, this, &AnimationTrackWidget::AddActuatorAction);
        if (timeline)
        {
            const auto widget_coord = mUI.timeline->mapFromGlobal(QCursor::pos());
            const auto seconds = mUI.timeline->MapToSeconds(widget_coord);
            const auto duration = mState.track->GetDuration();
            if (seconds > 0.0f && seconds < duration)
            {
                action->setEnabled(true);
            }
            action->setData(seconds);
        }
    }
    menu.addMenu(&actuators);
    menu.addSeparator();
    menu.addMenu(&add_timeline);
    menu.addSeparator();
    menu.addAction(mUI.actionDeleteActuator);
    menu.addAction(mUI.actionDeleteActuators);
    menu.addSeparator();
    menu.addAction(mUI.actionDeleteTimeline);
    menu.addSeparator();
    menu.addMenu(&show);
    menu.exec(QCursor::pos());
}

void AnimationTrackWidget::on_transformEndPosX_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto pos = node->GetTranslation();
        pos.x = value;
        node->SetTranslation(pos);
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndPosY_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto pos = node->GetTranslation();
        pos.y = value;
        node->SetTranslation(pos);
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndSizeX_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto size = node->GetSize();
        size.x = value;
        node->SetSize(size);
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndSizeY_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto size = node->GetSize();
        size.y = value;
        node->SetSize(size);
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndScaleX_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto scale = node->GetScale();
        scale.x = value;
        node->SetScale(scale);
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndScaleY_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto scale = node->GetScale();
        scale.y = value;
        node->SetScale(scale);
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndRotation_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        node->SetRotation(qDegreesToRadians(value));
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_setvalEndValue_ValueChanged()
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_setvalJoint_currentIndexChanged(int index)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_kinematicEndVeloX_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_kinematicEndVeloY_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_kinematicEndVeloZ_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_kinematicEndAccelX_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_kinematicEndAccelY_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_kinematicEndAccelZ_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_itemFlags_currentIndexChanged(int)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_flagAction_currentIndexChanged(int)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_flagTime_valueChanged(double)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_flagJoint_currentIndexChanged(int)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_materialInterpolation_currentIndexChanged(int)
{
    if (auto* node = GetCurrentEntityNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_btnMaterialParameters_clicked()
{
    auto* node = GetCurrentEntityNode();
    auto* klass = mState.entity->FindNodeById(node->GetClassId());
    auto* drawable = klass ? klass->GetDrawable() : nullptr;
    auto* actuator = dynamic_cast<game::MaterialAnimatorClass*>(GetCurrentActuator());
    if (!node || !drawable || !actuator)
        return;

    const auto& material = mWorkspace->GetMaterialClassById(node->GetDrawable()->GetMaterialId());
    DlgMaterialParams dlg(this, drawable, actuator);
    dlg.AdaptInterface(mWorkspace, material.get());
    dlg.exec();
}

void AnimationTrackWidget::SetActuatorUIEnabled(bool enabled)
{
    SetEnabled(mUI.actuatorName,       enabled);
    SetEnabled(mUI.actuatorID,         enabled);
    SetEnabled(mUI.actuatorType,       enabled);
    SetEnabled(mUI.actuatorNode,       enabled);
    SetEnabled(mUI.actuatorStartTime,  enabled);
    SetEnabled(mUI.actuatorEndTime,    enabled);
    SetEnabled(mUI.actuatorIsStatic,   enabled);
    SetEnabled(mUI.actuatorProperties, enabled);
}

void AnimationTrackWidget::SetActuatorUIDefaults()
{
    SetValue(mUI.actuatorGroup, QString("Animator (Nothing selected)"));
    SetMinMax(mUI.actuatorStartTime, 0.0, 0.0f);
    SetMinMax(mUI.actuatorEndTime, 0.0, 0.0);
    SetValue(mUI.actuatorName, QString(""));
    SetValue(mUI.actuatorID, QString(""));
    SetValue(mUI.actuatorType, QString(""));
    SetValue(mUI.actuatorNode, QString(""));
    SetValue(mUI.actuatorStartTime, 0.0f);
    SetValue(mUI.actuatorEndTime, 0.0f);
    SetValue(mUI.actuatorIsStatic, false);
    SetValue(mUI.transformInterpolation, game::TransformAnimatorClass::Interpolation::Cosine);
    SetValue(mUI.transformEndPosX, 0.0f);
    SetValue(mUI.transformEndPosY, 0.0f);
    SetValue(mUI.transformEndSizeX, 0.0f);
    SetValue(mUI.transformEndSizeY, 0.0f);
    SetValue(mUI.transformEndScaleX, 0.0f);
    SetValue(mUI.transformEndScaleY, 0.0f);
    SetValue(mUI.transformEndRotation, 0.0f);
    SetValue(mUI.setvalInterpolation, game::PropertyAnimatorClass::Interpolation::Cosine);
    SetValue(mUI.setvalName, game::PropertyAnimatorClass::PropertyName::Drawable_TimeScale);
    SetValue(mUI.setvalEndValue, 0.0f);
    SetValue(mUI.setvalJoint, -1);
    SetValue(mUI.kinematicTarget, game::KinematicAnimatorClass::Target::RigidBody);
    SetValue(mUI.kinematicInterpolation, game::KinematicAnimatorClass::Interpolation::Cosine);
    SetValue(mUI.kinematicEndVeloX, 0.0f);
    SetValue(mUI.kinematicEndVeloY, 0.0f);
    SetValue(mUI.kinematicEndVeloZ, 0.0f);
    SetValue(mUI.kinematicEndAccelX, 0.0f);
    SetValue(mUI.kinematicEndAccelY, 0.0f);
    SetValue(mUI.kinematicEndAccelZ, 0.0f);
    SetValue(mUI.materialInterpolation, game::MaterialAnimatorClass::Interpolation::Cosine);
    SetValue(mUI.itemFlags, game::BooleanPropertyAnimatorClass::PropertyName::Drawable_VisibleInGame);
    SetValue(mUI.flagAction, game::BooleanPropertyAnimatorClass::PropertyAction::On);
    SetValue(mUI.flagTime, 1.0f);
    SetValue(mUI.flagJoint, -1);


    mUI.curve->ClearFunction();

    if (!mState.track)
        return;

    const auto duration = mState.track->GetDuration();
    SetMinMax(mUI.actuatorStartTime, 0.0, duration);
    SetMinMax(mUI.actuatorEndTime,   0.0, duration);
}

void AnimationTrackWidget::SetSelectedActuatorProperties()
{
    if (mPlayState != PlayState::Stopped)
        return;

    auto* actuator = GetCurrentActuator();
    if (!actuator)
        return;

    actuator->SetName(GetValue(mUI.actuatorName));
    actuator->SetFlag(game::AnimatorClass::Flags::StaticInstance, GetValue(mUI.actuatorIsStatic));

    if (auto* transform = dynamic_cast<game::TransformAnimatorClass*>(actuator))
    {
        transform->SetInterpolation(GetValue(mUI.transformInterpolation));
        transform->SetEndPosition(GetValue(mUI.transformEndPosX), GetValue(mUI.transformEndPosY));
        transform->SetEndSize(GetValue(mUI.transformEndSizeX), GetValue(mUI.transformEndSizeY));
        transform->SetEndScale(GetValue(mUI.transformEndScaleX), GetValue(mUI.transformEndScaleY));
        transform->SetEndRotation(qDegreesToRadians((float) GetValue(mUI.transformEndRotation)));

        mUI.curve->SetFunction(transform->GetInterpolation());
    }
    else if (auto* setter = dynamic_cast<game::PropertyAnimatorClass*>(actuator))
    {
        using Name = game::PropertyAnimatorClass::PropertyName;
        const auto name = (Name)GetValue(mUI.setvalName);
        if (name == Name::Drawable_TimeScale)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Drawable_RotationX)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " °");
            setter->SetEndValue((float)qDegreesToRadians(mUI.setvalEndValue->GetAsFloat()));
        }
        else if (name == Name::Drawable_RotationY)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " °");
            setter->SetEndValue((float)qDegreesToRadians(mUI.setvalEndValue->GetAsFloat()));
        }
        else if (name == Name::Drawable_RotationZ)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " °");
            setter->SetEndValue((float)qDegreesToRadians(mUI.setvalEndValue->GetAsFloat()));
        }
        else if (name == Name::Drawable_TranslationX)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Drawable_TranslationY)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Drawable_TranslationZ)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Drawable_SizeZ)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::RigidBody_LinearVelocityX)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " m/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::RigidBody_LinearVelocityY)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " m/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::RigidBody_AngularVelocity)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " rad/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::RigidBody_LinearVelocity)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Vec2, " m/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsVec2());
        }
        else if (name == Name::Transformer_LinearVelocity)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Vec2, " u/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsVec2());
        }
        else if (name == Name::Transformer_LinearVelocityX)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " u/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Transformer_LinearVelocityY)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " u/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Transformer_LinearAcceleration)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Vec2, " u/s²");
            setter->SetEndValue(mUI.setvalEndValue->GetAsVec2());
        }
        else if (name == Name::Transformer_LinearAccelerationX)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " u/s²");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Transformer_LinearAccelerationY)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " u/s²");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Transformer_AngularVelocity)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " rad/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::Transformer_AngularAcceleration)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " rad/s²");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::TextItem_Text)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::String);
            setter->SetEndValue(app::ToUtf8(mUI.setvalEndValue->GetAsString()));
        }
        else if (name == Name::TextItem_Color)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Color);
            setter->SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        }
        else if (name == Name::RigidBodyJoint_MotorTorque)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::RigidBodyJoint_MotorSpeed)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::RigidBodyJoint_MotorForce)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::RigidBodyJoint_Stiffness)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::RigidBodyJoint_Damping)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::BasicLight_Direction)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Vec3);
            setter->SetEndValue(mUI.setvalEndValue->GetAsVec3());
        }
        else if (name == Name::BasicLight_Translation)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Vec3);
            setter->SetEndValue(mUI.setvalEndValue->GetAsVec3());
        }
        else if (name == Name::BasicLight_AmbientColor)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Color);
            setter->SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        }
        else if (name == Name::BasicLight_DiffuseColor)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Color);
            setter->SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        }
        else if (name == Name::BasicLight_SpecularColor)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Color);
            setter->SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        }
        else if (name == Name::BasicLight_SpotHalfAngle)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::BasicLight_LinearAttenuation)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::BasicLight_ConstantAttenuation)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::BasicLight_QuadraticAttenuation)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else BUG("Unhandled value actuator value type.");

        setter->SetInterpolation(GetValue(mUI.setvalInterpolation));
        setter->SetPropertyName(name);
        setter->SetJointId(GetItemId(mUI.setvalJoint));

        SetEnabled(mUI.setvalJoint, setter->RequiresJoint());

        mUI.curve->SetFunction(setter->GetInterpolation());
    }
    else if (auto* kinematic = dynamic_cast<game::KinematicAnimatorClass*>(actuator))
    {
        glm::vec2 linear_velocity;
        glm::vec2 linear_acceleration;
        linear_velocity.x = GetValue(mUI.kinematicEndVeloX);
        linear_velocity.y = GetValue(mUI.kinematicEndVeloY);
        linear_acceleration.x = GetValue(mUI.kinematicEndAccelX);
        linear_acceleration.y = GetValue(mUI.kinematicEndAccelY);

        kinematic->SetTarget(GetValue(mUI.kinematicTarget));
        kinematic->SetInterpolation(GetValue(mUI.kinematicInterpolation));
        kinematic->SetEndLinearVelocity(linear_velocity);
        kinematic->SetEndLinearAcceleration(linear_acceleration);
        kinematic->SetEndAngularVelocity(GetValue(mUI.kinematicEndVeloZ));
        kinematic->SetEndAngularAcceleration(GetValue(mUI.kinematicEndAccelZ));

        mUI.curve->SetFunction(kinematic->GetInterpolation());
    }
    else if (auto* setflag = dynamic_cast<game::BooleanPropertyAnimatorClass*>(actuator))
    {
        setflag->SetFlagAction(GetValue(mUI.flagAction));
        setflag->SetFlagName(GetValue(mUI.itemFlags));
        setflag->SetTime(GetValue(mUI.flagTime));
        setflag->SetJointId(GetItemId(mUI.flagJoint));

        SetEnabled(mUI.flagJoint, setflag->RequiresJoint());

        mUI.curve->ClearFunction();
    }
    else if (auto* material = dynamic_cast<game::MaterialAnimatorClass*>(actuator))
    {
        material->SetInterpolation(GetValue(mUI.materialInterpolation));

        mUI.curve->SetFunction(material->GetInterpolation());
    }

    //DEBUG("Updated actuator '%1' (%2)", actuator->GetName(), actuator->GetId());

    mUI.timeline->Rebuild();
}

void AnimationTrackWidget::on_btnTransformPlus90_clicked()
{
    const auto value = mUI.transformEndRotation->value();
    mUI.transformEndRotation->setValue(value + 90.0f);
}
void AnimationTrackWidget::on_btnTransformMinus90_clicked()
{
    const auto value = mUI.transformEndRotation->value();
    mUI.transformEndRotation->setValue(value - 90.0f);
}
void AnimationTrackWidget::on_btnTransformReset_clicked()
{
    if (auto* node = GetCurrentEntityNode())
    {
        // reset the properties of the actuator to the original node class values
        // first set the values to the UI widgets and then ask the actuator's
        // state to be updated from the UI.
        const auto& klass   = node->GetClass();
        const auto& pos     = klass.GetTranslation();
        const auto& size    = klass.GetSize();
        const auto& scale   = klass.GetScale();
        const auto rotation = klass.GetRotation();
        SetValue(mUI.transformEndPosX,     pos.x);
        SetValue(mUI.transformEndPosY,     pos.y);
        SetValue(mUI.transformEndSizeX,    size.x);
        SetValue(mUI.transformEndSizeY,    size.y);
        SetValue(mUI.transformEndScaleX,   scale.x);
        SetValue(mUI.transformEndScaleY,   scale.y);
        SetValue(mUI.transformEndRotation, qRadiansToDegrees(rotation));

        // apply the reset to the visualization entity and to its node instance.
        node->SetTranslation(pos);
        node->SetSize(size);
        node->SetScale(scale);
        node->SetRotation(rotation);

        // Set the values from the UI to the actual actuator class object.
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_btnViewPlus90_clicked()
{
    mAnimator.Plus90(mUI, mState);
}
void AnimationTrackWidget::on_btnViewMinus90_clicked()
{
    mAnimator.Minus90(mUI, mState);
}
void AnimationTrackWidget::on_btnViewReset_clicked()
{
    mAnimator.Reset(mUI, mState);
    SetValue(mUI.scaleX,     1.0f);
    SetValue(mUI.scaleY,     1.0f);
}

void AnimationTrackWidget::on_btnMoreViewportSettings_clicked()
{
    const auto visible = mUI.transform->isVisible();
    SetVisible(mUI.transform, !visible);
    if (!visible)
        mUI.btnMoreViewportSettings->setArrowType(Qt::ArrowType::DownArrow);
    else mUI.btnMoreViewportSettings->setArrowType(Qt::ArrowType::UpArrow);
}

void AnimationTrackWidget::SelectedItemChanged(const TimelineWidget::TimelineItem* item)
{
    if (item == nullptr)
    {
        SetActuatorUIEnabled(false);
        SetActuatorUIDefaults();
        ReturnToDefault();
        return;
    }

    SetActuatorUIEnabled(true);

    const auto* actuator = mState.track->FindAnimatorById(app::ToUtf8(item->id));
    const auto duration = mState.track->GetDuration();
    const auto start = actuator->GetStartTime() * duration;
    const auto end = actuator->GetDuration() * duration + start;
    const auto node = mEntity->FindNodeByClassId(actuator->GetNodeId());

    // figure out the hi/lo (left right) limits for the spinbox start
    // and time values for this actuator.
    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i = 0; i < mState.track->GetNumAnimators(); ++i)
    {
        const auto& other = mState.track->GetAnimatorClass(i);
        if (other.GetId() == actuator->GetId())
            continue;
        else if (other.GetNodeId() != actuator->GetNodeId())
            continue;

        // if the actuators are not on the same timeline then discard
        // from this check
        ASSERT(base::Contains(mState.actuator_to_timeline, actuator->GetId()));
        ASSERT(base::Contains(mState.actuator_to_timeline, other.GetId()));
        if (mState.actuator_to_timeline[actuator->GetId()] !=
            mState.actuator_to_timeline[other.GetId()])
            continue;

        const auto start = other.GetStartTime();
        const auto end = other.GetStartTime() + other.GetDuration();
        if (start >= actuator->GetStartTime())
            hi_bound = std::min(hi_bound, start);
        if (end <= actuator->GetStartTime())
            lo_bound = std::max(lo_bound, end);
    }
    const auto& len = QString::number(end - start, 'f', 2);
    SetValue(mUI.actuatorName, actuator->GetName());
    SetValue(mUI.actuatorID, actuator->GetId());
    SetValue(mUI.actuatorNode, node->GetName());
    SetValue(mUI.actuatorType, app::toString(actuator->GetType()));
    SetValue(mUI.actuatorIsStatic, actuator->TestFlag(game::AnimatorClass::Flags::StaticInstance));
    SetMinMax(mUI.actuatorStartTime, lo_bound * duration, hi_bound * duration);
    SetMinMax(mUI.actuatorEndTime, lo_bound * duration, hi_bound * duration);
    SetValue(mUI.actuatorStartTime, start);
    SetValue(mUI.actuatorEndTime, end);
    SetValue(mUI.actuatorGroup, app::toString("Animator - %1, %2s", item->text, len));

    if (const auto* ptr = dynamic_cast<const game::TransformAnimatorClass*>(actuator))
    {
        const auto& pos = ptr->GetEndPosition();
        const auto& size = ptr->GetEndSize();
        const auto& scale = ptr->GetEndScale();
        const auto rotation = ptr->GetEndRotation();
        SetValue(mUI.transformInterpolation, ptr->GetInterpolation());
        SetValue(mUI.transformEndPosX, pos.x);
        SetValue(mUI.transformEndPosY, pos.y);
        SetValue(mUI.transformEndSizeX, size.x);
        SetValue(mUI.transformEndSizeY, size.y);
        SetValue(mUI.transformEndScaleX, scale.x);
        SetValue(mUI.transformEndScaleY, scale.y);
        SetValue(mUI.transformEndRotation, qRadiansToDegrees(rotation));
        mUI.actuatorProperties->setCurrentWidget(mUI.transformActuator);
        mUI.curve->SetFunction(ptr->GetInterpolation());

        node->SetTranslation(pos);
        node->SetSize(size);
        node->SetScale(scale);
        node->SetRotation(rotation);
    }
    else if (const auto* ptr = dynamic_cast<const game::PropertyAnimatorClass*>(actuator))
    {
        const auto name = ptr->GetPropertyName();
        using Name = game::PropertyAnimatorClass::PropertyName;
        if (name == Name::Drawable_TimeScale)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::Drawable_RotationX)
            SetValue(mUI.setvalEndValue, qRadiansToDegrees(*ptr->GetEndValue<float>()), " °");
        else if (name == Name::Drawable_RotationY)
            SetValue(mUI.setvalEndValue, qRadiansToDegrees(*ptr->GetEndValue<float>()), " °");
        else if (name == Name::Drawable_RotationZ)
            SetValue(mUI.setvalEndValue, qRadiansToDegrees(*ptr->GetEndValue<float>()), " °");
        else if (name == Name::Drawable_TranslationX)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::Drawable_TranslationY)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::Drawable_TranslationZ)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::Drawable_SizeZ)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::RigidBody_LinearVelocity)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<glm::vec2>(), " m/s");
        else if (name == Name::RigidBody_LinearVelocityX)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " m/s");
        else if (name == Name::RigidBody_LinearVelocityY)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " m/s");
        else if (name == Name::RigidBody_AngularVelocity)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " m/s");
        else if (name == Name::Transformer_LinearVelocity)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<glm::vec2>(), " u/s");
        else if (name == Name::Transformer_LinearVelocityX)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " u/s");
        else if (name == Name::Transformer_LinearVelocityY)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " u/s");
        else if (name == Name::Transformer_LinearAcceleration)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<glm::vec2>(), " u/s²");
        else if (name == Name::Transformer_LinearAccelerationX)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " u/s²");
        else if (name == Name::Transformer_LinearAccelerationY)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " u/s²");
        else if (name == Name::Transformer_AngularVelocity)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " rad/s");
        else if (name == Name::Transformer_AngularAcceleration)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " rad/s²");
        else if (name == Name::TextItem_Color)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<base::Color4f>());
        else if (name == Name::TextItem_Text)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<std::string>());
        else if (name == Name::RigidBodyJoint_MotorTorque)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::RigidBodyJoint_MotorSpeed)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::RigidBodyJoint_MotorForce)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::RigidBodyJoint_Stiffness)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::RigidBodyJoint_Damping)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::BasicLight_Direction)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<glm::vec3>());
        else if (name == Name::BasicLight_Translation)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<glm::vec3>());
        else if (name == Name::BasicLight_AmbientColor)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<base::Color4f>());
        else if (name == Name::BasicLight_DiffuseColor)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<base::Color4f>());
        else if (name == Name::BasicLight_SpecularColor)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<base::Color4f>());
        else if (name == Name::BasicLight_SpotHalfAngle)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " °");
        else if (name == Name::BasicLight_ConstantAttenuation)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::BasicLight_LinearAttenuation)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else if (name == Name::BasicLight_QuadraticAttenuation)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
        else BUG("Unhandled set value actuator value type.");

        SetValue(mUI.setvalInterpolation, ptr->GetInterpolation());
        SetValue(mUI.setvalName, ptr->GetPropertyName());
        SetValue(mUI.setvalJoint, ListItemId(ptr->GetJointId()));
        SetEnabled(mUI.setvalJoint, ptr->RequiresJoint());

        mUI.actuatorProperties->setCurrentWidget(mUI.setvalActuator);

        mUI.curve->SetFunction(ptr->GetInterpolation());
    }
    else if (const auto* ptr = dynamic_cast<const game::KinematicAnimatorClass*>(actuator))
    {
        const auto& linear_velocity = ptr->GetEndLinearVelocity();
        const auto& linear_acceleration = ptr->GetEndLinearAcceleration();
        SetValue(mUI.kinematicTarget,        ptr->GetTarget());
        SetValue(mUI.kinematicInterpolation, ptr->GetInterpolation());
        SetValue(mUI.kinematicEndVeloZ,      ptr->GetEndAngularVelocity());
        SetValue(mUI.kinematicEndAccelZ,     ptr->GetEndAngularAcceleration());
        SetValue(mUI.kinematicEndVeloX,      linear_velocity.x);
        SetValue(mUI.kinematicEndVeloY,      linear_velocity.y);
        SetValue(mUI.kinematicEndAccelX,     linear_acceleration.x);
        SetValue(mUI.kinematicEndAccelY,     linear_acceleration.y);
        UpdateKinematicUnits();

        mUI.curve->SetFunction(ptr->GetInterpolation());

        mUI.actuatorProperties->setCurrentWidget(mUI.kinematicActuator);
    }
    else if (const auto* ptr = dynamic_cast<const game::BooleanPropertyAnimatorClass*>(actuator))
    {
        SetValue(mUI.itemFlags, ptr->GetFlagName());
        SetValue(mUI.flagAction, ptr->GetFlagAction());
        SetValue(mUI.flagTime, ptr->GetTime());
        SetValue(mUI.flagJoint, ListItemId(ptr->GetJointId()));
        SetEnabled(mUI.flagJoint, ptr->RequiresJoint());

        mUI.actuatorProperties->setCurrentWidget(mUI.setflagActuator);

        mUI.curve->ClearFunction();
    }
    else if (const auto* ptr = dynamic_cast<const game::MaterialAnimatorClass*>(actuator))
    {
        SetValue(mUI.materialInterpolation, ptr->GetInterpolation());
        mUI.actuatorProperties->setCurrentWidget(mUI.materialActuator);

        mUI.curve->SetFunction(ptr->GetInterpolation());
    }
    else
    {
        mUI.actuatorProperties->setEnabled(false);
    }
    VERBOSE("Selected timeline item '%1' (%2)", item->text, item->id);
}

void AnimationTrackWidget::SelectedItemDragged(const TimelineWidget::TimelineItem* item)
{
    auto* actuator = mState.track->FindAnimatorById(app::ToUtf8(item->id));
    actuator->SetStartTime(item->starttime);
    actuator->SetDuration(item->duration);

    const auto duration = mState.track->GetDuration();
    const auto start = actuator->GetStartTime() * duration;
    const auto end   = actuator->GetDuration() * duration + start;
    const auto len   = QString::number(end-start, 'f', 2);
    SetValue(mUI.actuatorStartTime, start);
    SetValue(mUI.actuatorEndTime, end);
    SetValue(mUI.actuatorGroup, tr("Animator - %1, %2s").arg(item->text).arg(len));
}

void AnimationTrackWidget::ToggleShowResource()
{
    QAction* action = qobject_cast<QAction*>(sender());
    const auto payload = action->data().toInt();
    const auto type = magic_enum::enum_cast<game::AnimatorClass::Type>(payload);
    ASSERT(type.has_value());
    mState.show_flags.set(type.value(), action->isChecked());
    mUI.timeline->Rebuild();
}

void AnimationTrackWidget::AddActuatorAction()
{
    // Extract the data for adding a new actuator from the action
    // that is created and when the timeline custom context menu is opened.
    QAction* action = qobject_cast<QAction*>(sender());
    // the seconds (seconds into the duration of the animation)
    // is set when the context menu with this QAction is opened.
    const auto seconds = action->data().toFloat();
    // the name of the action carries the type
    const auto type = static_cast<game::AnimatorClass::Type>(action->property("type").toInt());

    AddActuatorFromTimeline(type, seconds);

    // hack for now
    if (type == game::AnimatorClass::Type::KinematicAnimator)
    {
        const auto target = static_cast<game::KinematicAnimatorClass::Target>(action->property("kinematic_target").toInt());

        if (auto* selected = GetCurrentActuator()) {
            auto* ptr = dynamic_cast<game::KinematicAnimatorClass*>(selected);
            ptr->SetTarget(target);
            SetValue(mUI.kinematicTarget, target);
        }
    }
}

void AnimationTrackWidget::AddNodeTimelineAction()
{
    QAction* action = qobject_cast<QAction*>(sender());

    const QString& nodeId = action->data().toString();
    size_t index = 0;
    if (!mUI.timeline->GetCurrentTimeline())
    {
        if (!mState.timelines.empty())
            index = mState.timelines.size();
    }
    else
    {
        index = mUI.timeline->GetCurrentTimelineIndex();
    }
    Timeline tl;
    tl.selfId = base::RandomString(10);
    tl.nodeId = app::ToUtf8(nodeId);
    mState.timelines.insert(mState.timelines.begin() + index, tl);
    mUI.timeline->Rebuild();
}

void AnimationTrackWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto view   = engine::GameView::AxisAligned;

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    gfx::Device* device = painter.GetDevice();
    gfx::Painter entity_painter(device);
    entity_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, engine::GameView::AxisAligned));
    entity_painter.SetProjectionMatrix(CreateProjectionMatrix(mUI,  engine::Projection::Orthographic));
    entity_painter.SetPixelRatio(glm::vec2{xs*zoom, ys*zoom});
    entity_painter.SetViewport(0, 0, width, height);
    entity_painter.SetSurfaceSize(width, height);
    entity_painter.SetEditingMode(true);

    const auto camera_position = glm::vec2{mState.camera_offset_x, mState.camera_offset_y};
    const auto camera_scale    = glm::vec2{xs, ys};
    const auto camera_rotation = (float)GetValue(mUI.rotation);

    LowLevelRenderHook low_level_render_hook(
            camera_position,
            camera_scale,
            view,
            camera_rotation,
            width, height,
            zoom,
            grid,
            GetValue(mUI.chkShowGrid));

    engine::Renderer::Camera camera;
    camera.clear_color = mUI.widget->GetCurrentClearColor();
    camera.position    = camera_position;
    camera.rotation    = camera_rotation;
    camera.scale       = camera_scale * zoom;
    camera.viewport    = game::FRect(-width*0.5f, -height*0.5f, width, height);
    camera.ppa         = engine::ComputePerspectiveProjection(camera.viewport);
    mRenderer.SetCamera(camera);

    engine::Renderer::Surface surface;
    surface.viewport = gfx::IRect(0, 0, width, height);
    surface.size     = gfx::USize(width, height);
    mRenderer.SetSurface(surface);

    mRenderer.SetEditingMode(true);
    mRenderer.SetStyle(GetValue(mUI.cmbStyle));
    mRenderer.SetClassLibrary(mWorkspace);
    mRenderer.SetName("AnimationWidgetRenderer/" + mState.entity->GetId());
    mRenderer.SetLowLevelRendererHook(&low_level_render_hook);

    if (mPlaybackAnimation)
    {
        mRenderer.BeginFrame();
        mRenderer.CreateFrame(*mPlaybackAnimation);
        mRenderer.DrawFrame(*device);
        mRenderer.EndFrame();
    }
    else
    {
        DrawHook hook(GetCurrentEntityNode());
        hook.SetIsPlaying(mPlayState == PlayState::Playing);
        hook.SetDrawVectors(false);

        mRenderer.BeginFrame();
        mRenderer.CreateFrame(*mEntity, &hook);
        mRenderer.DrawFrame(*device);
        mRenderer.EndFrame();
    }

    // basis vectors
    if (GetValue(mUI.chkShowOrigin))
    {
        gfx::Transform view;
        DrawBasisVectors(entity_painter, view);
    }

    // viewport
    if (GetValue(mUI.chkShowViewport))
    {
        gfx::Transform view;
        MakeViewTransform(mUI, mState, view);
        const auto& settings = mWorkspace->GetProjectSettings();
        const float game_width = settings.viewport_width;
        const float game_height = settings.viewport_height;
        DrawViewport(painter, view, game_width, game_height, width, height);
    }

    PrintMousePos(mUI, mState, painter,
                  engine::GameView::AxisAligned,
                  engine::Projection::Orthographic);
}

void AnimationTrackWidget::MouseZoom(std::function<void(void)> zoom_function)
{
    // where's the mouse in the widget
    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    // can't use underMouse here because of the way the gfx widget
    // is constructed i.e QWindow and Widget as container
    if (mickey.x() < 0 || mickey.y() < 0 ||
        mickey.x() > mUI.widget->width() ||
        mickey.y() > mUI.widget->height())
        return;

    const auto view = engine::GameView::AxisAligned;
    const auto proj = engine::Projection::Orthographic;
    Point2Df mickey_world_before_zoom;
    Point2Df mickey_world_after_zoom;

    {
        mickey_world_before_zoom = MapWindowCoordinateToWorld(mUI, mState, mickey, view, proj);
    }

    zoom_function();

    {
        mickey_world_after_zoom = MapWindowCoordinateToWorld(mUI, mState, mickey, view, proj);
    }
    glm::vec2 before = mickey_world_before_zoom;
    glm::vec2 after  = mickey_world_after_zoom;
    mState.camera_offset_x += (before.x - after.x);
    mState.camera_offset_y += (before.y - after.y);
    DisplayCurrentCameraLocation();
}

void AnimationTrackWidget::MouseMove(QMouseEvent* event)
{
    if (mCurrentTool)
    {
        const MouseEvent mickey(event, mUI, mState);

        mCurrentTool->MouseMove(mickey);

        // update the properties that might have changed as the result of application
        // of the current tool.
        DisplayCurrentCameraLocation();
        UpdateTransformActuatorUI();
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::MousePress(QMouseEvent* event)
{
    const MouseEvent mickey(event, mUI, mState);

    if (!mCurrentTool &&
        (mPlayState == PlayState::Stopped) &&
        (mickey->button() == Qt::LeftButton))
    {
        const auto snap = (bool)GetValue(mUI.chkSnap);
        const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
        const auto grid_size = static_cast<unsigned>(grid);
        const auto click_point = mickey->pos();

        // don't allow node selection to change through mouse clicking since it's
        // confusing. Rather only allow a timeline actuator to be selected which then
        // selects the entity node it applies on and only select an entity node
        // transformation tool here.
        if (auto* current = GetCurrentEntityNode())
        {
            const auto node_box_size = current->GetSize();
            const auto node_to_world = mEntity->FindNodeTransform(current);
            gfx::FRect box;
            box.Resize(node_box_size.x, node_box_size.y);
            box.Translate(-node_box_size.x*0.5f, -node_box_size.y*0.5f);
            const auto hotspot = TestToolHotspot(mUI, mState, node_to_world, box, click_point);
            if (hotspot == ToolHotspot::Resize)
                mCurrentTool.reset(new ResizeRenderTreeNodeTool(*mEntity, current));
            else if (hotspot == ToolHotspot::Rotate)
                mCurrentTool.reset(new RotateRenderTreeNodeTool(*mEntity, current));
            else if (hotspot == ToolHotspot::Remove)
                mCurrentTool.reset(new MoveRenderTreeNodeTool(*mEntity, current, snap, grid_size));
        }
    }
    else if (!mCurrentTool && (mickey->button() == Qt::RightButton))
    {
        mCurrentTool.reset(new PerspectiveCorrectCameraTool(mUI, mState));
    }

    if (mCurrentTool)
        mCurrentTool->MousePress(mickey);
}
void AnimationTrackWidget::MouseRelease(QMouseEvent* event)
{
    if (!mCurrentTool)
        return;

    const MouseEvent mickey(event, mUI, mState);

    if (mCurrentTool->MouseRelease(mickey))
        mCurrentTool.reset();
}
bool AnimationTrackWidget::KeyPress(QKeyEvent* key)
{
    return false;
}

void AnimationTrackWidget::UpdateTrackUI()
{
    const float duration = mState.track->GetDuration();
    SetValue(mUI.trackName, mState.track->GetName());
    SetValue(mUI.trackID,   mState.track->GetId());
    SetValue(mUI.looping,   mState.track->IsLooping());
    SetValue(mUI.duration,  mState.track->GetDuration());
    SetValue(mUI.delay,     mState.track->GetDelay());
    SetMinMax(mUI.actuatorStartTime, 0.0, duration);
    SetMinMax(mUI.actuatorEndTime, 0.0, duration);

    setWindowTitle(GetValue(mUI.trackName));
}

void AnimationTrackWidget::UpdateTransformActuatorUI()
{
    if (mPlayState != PlayState::Stopped)
        return;
    if (const auto* node = GetCurrentEntityNode())
    {
        const auto& pos      = node->GetTranslation();
        const auto& size     = node->GetSize();
        const auto& rotation = node->GetRotation();
        const auto& scale    = node->GetScale();
        SetValue(mUI.transformEndPosX, pos.x);
        SetValue(mUI.transformEndPosY, pos.y);
        SetValue(mUI.transformEndSizeX, size.x);
        SetValue(mUI.transformEndSizeY, size.y);
        SetValue(mUI.transformEndScaleX, scale.x);
        SetValue(mUI.transformEndScaleY, scale.y);
        SetValue(mUI.transformEndRotation, qRadiansToDegrees(rotation));
    }
}

void AnimationTrackWidget::AddActuatorFromTimeline(game::AnimatorClass::Type type, float seconds)
{
    if (!mUI.timeline->GetCurrentTimeline())
        return;
    const auto timeline_index = mUI.timeline->GetCurrentTimelineIndex();
    if (timeline_index >= mState.timelines.size())
        return;

    const auto& timeline = mState.timelines[timeline_index];
    const auto* node     = mState.entity->FindNodeById(timeline.nodeId);
    const auto duration  = mState.track->GetDuration();
    const auto position  = seconds / duration;

    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i=0; i< mState.track->GetNumAnimators(); ++i)
    {
        const auto& klass = mState.track->GetAnimatorClass(i);
        if (mState.actuator_to_timeline[klass.GetId()] != timeline.selfId)
            continue;
        const auto start = klass.GetStartTime();
        const auto end   = klass.GetStartTime() + klass.GetDuration();
        if (start >= position)
            hi_bound = std::min(hi_bound, start);
        if (end <= position)
            lo_bound = std::max(lo_bound, end);
    }
    const auto& name = base::FormatString("Animator_%1", mState.track->GetNumAnimators());
    const auto node_duration   = hi_bound - lo_bound;
    const auto node_start_time = lo_bound;
    const float node_end_time  = node_start_time + duration;

    QString actuator;

    if (type == game::AnimatorClass::Type::TransformAnimator)
    {
        const auto& pos     = node->GetTranslation();
        const auto& size    = node->GetSize();
        const auto& scale   = node->GetScale();
        const auto rotation = node->GetRotation();
        SetValue(mUI.transformEndPosX,   pos.x);
        SetValue(mUI.transformEndPosY,   pos.y);
        SetValue(mUI.transformEndSizeX,  size.x);
        SetValue(mUI.transformEndSizeY,  size.y);
        SetValue(mUI.transformEndScaleX, scale.x);
        SetValue(mUI.transformEndScaleY, scale.y);
        SetValue(mUI.transformEndRotation, qRadiansToDegrees(rotation));

        game::TransformAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(node->GetId());
        klass.SetFlag(game::AnimatorClass::Flags::StaticInstance, GetValue(mUI.actuatorIsStatic));
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        klass.SetEndPosition(pos);
        klass.SetEndSize(size);
        klass.SetEndScale(scale);
        klass.SetInterpolation(GetValue(mUI.transformInterpolation));
        klass.SetEndRotation(qDegreesToRadians((float) GetValue(mUI.transformEndRotation)));
        actuator = app::FromUtf8(klass.GetId());
        mState.actuator_to_timeline[klass.GetId()] = timeline.selfId;
        mState.track->AddAnimator(klass);
    }
    else if (type == game::AnimatorClass::Type::PropertyAnimator)
    {
        using ValName = game::PropertyAnimatorClass::PropertyName;
        const auto value = (ValName)GetValue(mUI.setvalName);
        game::PropertyAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(node->GetId());
        klass.SetFlag(game::AnimatorClass::Flags::StaticInstance, GetValue(mUI.actuatorIsStatic));
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        klass.SetPropertyName(GetValue(mUI.setvalName));
        klass.SetInterpolation(GetValue(mUI.setvalInterpolation));
        klass.SetJointId(GetItemId(mUI.setvalJoint));

        if (value == ValName::Drawable_TimeScale)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::Drawable_RotationX)
            klass.SetEndValue(qDegreesToRadians(mUI.setvalEndValue->GetAsFloat()));
        else if (value == ValName::Drawable_RotationY)
            klass.SetEndValue(qDegreesToRadians(mUI.setvalEndValue->GetAsFloat()));
        else if (value == ValName::Drawable_RotationZ)
            klass.SetEndValue(qDegreesToRadians(mUI.setvalEndValue->GetAsFloat()));
        else if (value == ValName::Drawable_TranslationX)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::Drawable_TranslationY)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::Drawable_TranslationZ)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::Drawable_SizeZ)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::RigidBody_LinearVelocityX)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::RigidBody_LinearVelocityY)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::RigidBody_AngularVelocity)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::RigidBody_LinearVelocity)
            klass.SetEndValue(mUI.setvalEndValue->GetAsVec2());
        else if (value == ValName::TextItem_Text)
            klass.SetEndValue(app::ToUtf8(mUI.setvalEndValue->GetAsString()));
        else if (value == ValName::TextItem_Color)
            klass.SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        else if (value == ValName::RigidBodyJoint_MotorTorque)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::RigidBodyJoint_MotorSpeed)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::RigidBodyJoint_MotorForce)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::RigidBodyJoint_Stiffness)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::RigidBodyJoint_Damping)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::BasicLight_Direction)
            klass.SetEndValue(mUI.setvalEndValue->GetAsVec3());
        else if (value == ValName::BasicLight_Translation)
            klass.SetEndValue(mUI.setvalEndValue->GetAsVec3());
        else if (value == ValName::BasicLight_AmbientColor)
            klass.SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        else if (value == ValName::BasicLight_DiffuseColor)
            klass.SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        else if (value == ValName::BasicLight_SpecularColor)
            klass.SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        else if (value == ValName::BasicLight_SpotHalfAngle)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::BasicLight_ConstantAttenuation)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::BasicLight_LinearAttenuation)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (value == ValName::BasicLight_QuadraticAttenuation)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else BUG("Unhandled value actuator value type.");

        actuator = app::FromUtf8(klass.GetId());
        mState.actuator_to_timeline[klass.GetId()] = timeline.selfId;
        mState.track->AddAnimator(klass);
    }
    else if (type == game::AnimatorClass::Type::KinematicAnimator)
    {
        game::KinematicAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(node->GetId());
        klass.SetFlag(game::AnimatorClass::Flags::StaticInstance, GetValue(mUI.actuatorIsStatic));
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        klass.SetEndAngularVelocity(GetValue(mUI.kinematicEndVeloZ));
        glm::vec2 velocity;
        velocity.x = GetValue(mUI.kinematicEndVeloX);
        velocity.y = GetValue(mUI.kinematicEndVeloY);
        klass.SetEndLinearVelocity(velocity);
        actuator = app::FromUtf8(klass.GetId());
        mState.actuator_to_timeline[klass.GetId()] = timeline.selfId;
        mState.track->AddAnimator(klass);
    }
    else if (type == game::AnimatorClass::Type::BooleanPropertyAnimator)
    {
        game::BooleanPropertyAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(node->GetId());
        klass.SetFlag(game::AnimatorClass::Flags::StaticInstance, GetValue(mUI.actuatorIsStatic));
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        klass.SetFlagName(GetValue(mUI.itemFlags));
        klass.SetFlagAction(GetValue(mUI.flagAction));
        klass.SetJointId(GetItemId(mUI.flagJoint));
        actuator = app::FromUtf8(klass.GetId());
        mState.actuator_to_timeline[klass.GetId()] = timeline.selfId;
        mState.track->AddAnimator(klass);
    }
    else if (type ==game::AnimatorClass::Type::MaterialAnimator)
    {
        game::MaterialAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(node->GetId());
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        actuator = app::FromUtf8(klass.GetId());
        mState.actuator_to_timeline[klass.GetId()] = timeline.selfId;
        mState.track->AddAnimator(klass);
    }

    mUI.timeline->Rebuild();
    SelectedItemChanged(mUI.timeline->SelectItem(actuator));

    DEBUG("New %1 actuator on entity node '%2' from %3s to %4s", type, node->GetName(),
          node_start_time * duration, node_end_time * duration);
}

void AnimationTrackWidget::CreateTimelines()
{
    // create timelines for nodes that don't have a timeline yet.
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& id = mState.entity->GetNode(i).GetId();
        auto it = std::find_if(mState.timelines.begin(),
                               mState.timelines.end(), [&id](const auto& timeline) {
            return timeline.nodeId == id;
        });
        if (it != mState.timelines.end())
            continue;

        Timeline timeline;
        timeline.selfId = base::RandomString(10);
        timeline.nodeId = id;
        mState.timelines.push_back(std::move(timeline));
    }
}

void AnimationTrackWidget::RemoveDeletedItems()
{
    // delete animators that refer to a node/joint that no longer exists
    std::vector<std::string> dead_animators;
    for (size_t i=0; i< mState.track->GetNumAnimators(); ++i)
    {
        const auto& animator = mState.track->GetAnimatorClass(i);
        const auto& node = animator.GetNodeId();
        if (!mState.entity->FindNodeById(node))
        {
            dead_animators.push_back(animator.GetId());
            DEBUG("Deleting animator because the entity node is no longer available. [animator='%1']", animator.GetName());
        }
        else if (const auto* ptr = game::AsPropertyAnimatorClass(&animator))
        {
            const auto* joint = mState.entity->FindJointById(ptr->GetJointId());
            if (!joint && ptr->RequiresJoint())
            {
                dead_animators.push_back(animator.GetId());
                DEBUG("Deleting animator because the joint is no longer available. [animator='%1']", animator.GetName());
            }
        }
        else if (const auto* ptr = game::AsBooleanPropertyAnimatorClass(&animator))
        {
            const auto* joint = mState.entity->FindJointById(ptr->GetJointId());
            if (!joint && ptr->RequiresJoint())
            {
                dead_animators.push_back(animator.GetId());
                DEBUG("Deleting animator because the joint is no longer available. [animator='%1']", animator.GetName());
            }
        }
    }
    for (const auto& id : dead_animators)
    {
        mState.track->DeleteAnimatorById(id);
        mState.actuator_to_timeline.erase(id);
    }

    // remove orphaned timelines.
    for (auto it = mState.timelines.begin();  it != mState.timelines.end();)
    {
        const auto& timeline = *it;
        if (mState.entity->FindNodeById(timeline.nodeId))
        {
            ++it;
            continue;
        }
        else
        {
            it = mState.timelines.erase(it);
        }
    }


    std::vector<ListItem> joints;
    for (size_t i=0; i<mState.entity->GetNumJoints(); ++i)
    {
        const auto& joint = mState.entity->GetJoint(i);
        ListItem li;
        li.id   = joint.GetId();
        li.name = joint.GetName();
        joints.push_back(li);
    }
    SetList(mUI.setvalJoint, joints);
    SetList(mUI.flagJoint, joints);
}

void AnimationTrackWidget::ReturnToDefault()
{
    // put the nodes that were transformed by the animation visualization/playback
    // back to their original positions based on the entity class defaults.
    // quickest and simplest way to do this is to simply create a new
    // entity instance and then all state is properly set.
    mEntity = game::CreateEntityInstance(mState.entity);
}

void AnimationTrackWidget::UpdateKinematicUnits()
{
    const game::KinematicAnimatorClass::Target target = GetValue(mUI.kinematicTarget);
    if (target == game::KinematicAnimatorClass::Target::RigidBody)
    {
        SetSuffix(mUI.kinematicEndVeloX, " m/s");
        SetSuffix(mUI.kinematicEndVeloY, " m/s");
        SetSuffix(mUI.kinematicEndVeloZ, " rad/s");
        SetSuffix(mUI.kinematicEndAccelX, " m/s²");
        SetSuffix(mUI.kinematicEndAccelY, " m/s²");
        SetSuffix(mUI.kinematicEndAccelZ, " rad/s²");
        SetEnabled(mUI.kinematicEndAccelX, false);
        SetEnabled(mUI.kinematicEndAccelY, false);
        SetEnabled(mUI.kinematicEndAccelZ, false);
    }
    else if (target == game::KinematicAnimatorClass::Target::Transformer)
    {
        SetSuffix(mUI.kinematicEndVeloX, " u/s");
        SetSuffix(mUI.kinematicEndVeloY, " u/s");
        SetSuffix(mUI.kinematicEndVeloZ, " rad/s");
        SetSuffix(mUI.kinematicEndAccelX, " u/s²");
        SetSuffix(mUI.kinematicEndAccelY, " u/s²");
        SetSuffix(mUI.kinematicEndAccelZ, " rad/s²");
        SetEnabled(mUI.kinematicEndAccelX, true);
        SetEnabled(mUI.kinematicEndAccelY, true);
        SetEnabled(mUI.kinematicEndAccelZ, true);
    } else BUG("Missing kinematic actuator target type.");
}

void AnimationTrackWidget::DisplayCurrentCameraLocation()
{
    SetValue(mUI.translateX, -mState.camera_offset_x);
    SetValue(mUI.translateY, -mState.camera_offset_y);
}

game::EntityNode* AnimationTrackWidget::GetCurrentEntityNode()
{
    if (const auto* item = mUI.timeline->GetSelectedItem())
    {
        const auto* actuator = mState.track->FindAnimatorById(app::ToUtf8(item->id));
        return mEntity->FindNodeByClassId(actuator->GetNodeId());
    }
    return nullptr;
}

game::AnimatorClass* AnimationTrackWidget::GetCurrentActuator()
{
    if (auto* item = mUI.timeline->GetSelectedItem())
    {
        return mState.track->FindAnimatorById(app::ToUtf8(item->id));
    }
    return nullptr;
}

gui::TimelineWidget::TimelineItem* AnimationTrackWidget::GetCurrentTimelineItem()
{
    return mUI.timeline->GetSelectedItem();
}

std::shared_ptr<game::EntityClass> FindSharedEntity(size_t hash)
{
    std::shared_ptr<game::EntityClass> ret;
    auto it = SharedAnimations.find(hash);
    if (it == SharedAnimations.end())
        return ret;
    ret = it->second.lock();
    return ret;
}
void ShareEntity(const std::shared_ptr<game::EntityClass>& klass)
{
    const auto hash = klass->GetHash();
    SharedAnimations[hash] = klass;
}

void RegisterEntityWidget(EntityWidget* widget)
{
    EntityWidgets.insert(widget);
}
void DeleteEntityWidget(EntityWidget* widget)
{
    auto it = EntityWidgets.find(widget);
    ASSERT(it != EntityWidgets.end());
    EntityWidgets.erase(it);
}
void RegisterTrackWidget(AnimationTrackWidget* widget)
{
    TrackWidgets.insert(widget);
}

void DeleteTrackWidget(AnimationTrackWidget* widget)
{
    auto it = TrackWidgets.find(widget);
    ASSERT(it != TrackWidgets.end());
    TrackWidgets.erase(it);
}

void RealizeEntityChange(std::shared_ptr<const game::EntityClass> klass)
{
    for (auto* widget : TrackWidgets)
    {
        widget->RealizeEntityChange(klass);
    }
}

} // namespace
