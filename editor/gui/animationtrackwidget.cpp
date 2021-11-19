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
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/entitywidget.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/tool.h"
#include "game/animation.h"
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
        for (size_t i=0; i<track.GetNumActuators(); ++i)
        {
            const auto& actuator = track.GetActuatorClass(i);
            const auto type  = actuator.GetType();
            if (!mState.show_flags.test(type))
                continue;

            const auto& nodeId = actuator.GetNodeId();
            const auto& lineId = mState.actuator_to_timeline[actuator.GetId()];
            const auto* node = mState.entity->FindNodeById(nodeId);
            const auto& name = app::FromUtf8(node->GetName());
            const auto index = id_to_index_map[lineId];
            const auto num   = (*list)[index].GetNumItems();

            // pastel color palette.
            // https://colorhunt.co/palette/226038

            TimelineWidget::TimelineItem item;
            item.text      = QString("%1 (%2)").arg(name).arg(num+1);
            item.id        = app::FromUtf8(actuator.GetId());
            item.starttime = actuator.GetStartTime();
            item.duration  = actuator.GetDuration();
            if (type == game::ActuatorClass::Type::SetFlag)
                item.color = QColor(0xa3, 0xdd, 0xcb, 150);
            else if (type == game::ActuatorClass::Type::Transform)
                item.color = QColor(0xe8, 0xe9, 0xa1, 150);
            else if (type == game::ActuatorClass::Type::Kinematic)
                item.color = QColor(0xe6, 0xb5, 0x66, 150);
            else if (type == game::ActuatorClass::Type::SetValue)
                item.color = QColor(0xe5, 0x70, 0x7e, 150);
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

    PopulateFromEnum<game::ActuatorClass::Type>(mUI.actuatorType);
    PopulateFromEnum<game::TransformActuatorClass::Interpolation>(mUI.transformInterpolation);
    PopulateFromEnum<game::SetValueActuatorClass::Interpolation>(mUI.setvalInterpolation);
    PopulateFromEnum<game::SetValueActuatorClass::ParamName>(mUI.setvalName);
    PopulateFromEnum<game::KinematicActuatorClass::Interpolation>(mUI.kinematicInterpolation);
    PopulateFromEnum<game::SetFlagActuatorClass::FlagName>(mUI.itemFlags);
    PopulateFromEnum<game::SetFlagActuatorClass::FlagAction>(mUI.flagAction);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);

    setFocusPolicy(Qt::StrongFocus);

    mUI.widget->onZoomIn       = [this]() { MouseZoom(std::bind(&AnimationTrackWidget::ZoomIn, this)); };
    mUI.widget->onZoomOut      = [this]() { MouseZoom(std::bind(&AnimationTrackWidget::ZoomOut, this)); };
    mUI.widget->onMouseMove    = std::bind(&AnimationTrackWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&AnimationTrackWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&AnimationTrackWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onInitScene    = std::bind(&AnimationTrackWidget::InitScene, this,
                                           std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onPaintScene   = std::bind(&AnimationTrackWidget::PaintScene, this,
                                           std::placeholders::_1, std::placeholders::_2);

    connect(mUI.timeline, &TimelineWidget::SelectedItemChanged,
        this, &AnimationTrackWidget::SelectedItemChanged);
    connect(mUI.timeline, &TimelineWidget::SelectedItemDragged,
            this, &AnimationTrackWidget::SelectedItemDragged);

    SetActuatorUIDefaults("");
    SetActuatorUIEnabled(false);
    RegisterTrackWidget(this);
}

AnimationTrackWidget::AnimationTrackWidget(app::Workspace* workspace, const std::shared_ptr<game::EntityClass>& entity)
    : AnimationTrackWidget(workspace)
{
    // create a new animation track for the given entity.
    mState.entity = entity;
    mState.track  = std::make_shared<game::AnimationTrackClass>();
    mState.track->SetDuration(10.0f);
    mState.track->SetName("My Track");
    mState.track->SetLooping(false);
    mOriginalHash = mState.track->GetHash();
    mEntity = game::CreateEntityInstance(mState.entity);

    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();

    CreateTimelines();
    mUI.timeline->SetDuration(mState.track->GetDuration());
    mUI.timeline->Rebuild();

    UpdateTrackUI();
}

AnimationTrackWidget::AnimationTrackWidget(app::Workspace* workspace,
        const std::shared_ptr<game::EntityClass>& entity,
        const game::AnimationTrackClass& track,
        const QVariantMap& properties)
        : AnimationTrackWidget(workspace)
{
    // Edit an existing animation track for the given animation.
    mState.entity = entity;
    mState.track  = std::make_shared<game::AnimationTrackClass>(track); // edit a copy.
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
    for (size_t i = 0; i < mState.track->GetNumActuators(); ++i)
    {
        const auto& actuator = mState.track->GetActuatorClass(i);
        const auto& timeline = properties[app::FromUtf8(actuator.GetId())].toString();
        mState.actuator_to_timeline[actuator.GetId()] = app::ToUtf8(timeline);
    }

    RemoveDeletedItems();
    CreateTimelines();

    mUI.timeline->SetDuration(track.GetDuration());
    mUI.timeline->Rebuild();

    UpdateTrackUI();

}
AnimationTrackWidget::~AnimationTrackWidget()
{
    DEBUG("Destroy AnimationTrackWidget");

    DeleteTrackWidget(this);
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
    settings.SaveWidget("TrackWidget", mUI.viewScaleX);
    settings.SaveWidget("TrackWidget", mUI.viewScaleY);
    settings.SaveWidget("TrackWidget", mUI.viewRotation);
    settings.SaveWidget("TrackWidget", mUI.zoom);
    settings.SaveWidget("TrackWidget", mUI.cmbGrid);
    settings.SaveWidget("TrackWidget", mUI.chkShowOrigin);
    settings.SaveWidget("TrackWidget", mUI.chkShowGrid);
    settings.SaveWidget("TrackWidget", mUI.chkShowViewport);
    settings.SaveWidget("TrackWidget", mUI.chkSnap);
    settings.SaveAction("TrackWidget", mUI.actionUsePhysics);
    settings.SetValue("TrackWidget", "show_bits", mState.show_flags.value());
    settings.SetValue("TrackWidget", "camera_offset_x", mState.camera_offset_x);
    settings.SetValue("TrackWidget", "camera_offset_y", mState.camera_offset_y);

    settings.SetValue("TrackWidget", "num_timelines", (unsigned) mState.timelines.size());
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

    // use the entity JSON serialization to save the state.
    {
        data::JsonObject json;
        mState.entity->IntoJson(json);
        settings.SetValue("TrackWidget", "entity", base64::Encode(json.ToString()));
    }

    {
        data::JsonObject json;
        mState.track->IntoJson(json);
        settings.SetValue("TrackWidget", "track", base64::Encode(json.ToString()));
    }
    return true;
}
bool AnimationTrackWidget::LoadState(const Settings& settings)
{
    settings.LoadWidget("TrackWidget", mUI.viewScaleX);
    settings.LoadWidget("TrackWidget", mUI.viewScaleY);
    settings.LoadWidget("TrackWidget", mUI.viewRotation);
    settings.LoadWidget("TrackWidget", mUI.zoom);
    settings.LoadWidget("TrackWidget", mUI.cmbGrid);
    settings.LoadWidget("TrackWidget", mUI.chkShowOrigin);
    settings.LoadWidget("TrackWidget", mUI.chkShowGrid);
    settings.LoadWidget("TrackWidget", mUI.chkShowViewport);
    settings.LoadWidget("TrackWidget", mUI.chkSnap);
    settings.LoadAction("TrackWidget", mUI.actionUsePhysics);
    settings.GetValue("TrackWidget", "camera_offset_x", &mState.camera_offset_x);
    settings.GetValue("TrackWidget", "camera_offset_y", &mState.camera_offset_y);
    mCameraWasLoaded = true;

    unsigned num_timelines = 0;
    unsigned show_bits = ~0u;
    settings.GetValue("TrackWidget", "show_bits", &show_bits);
    settings.GetValue("TrackWidget", "num_timelines", &num_timelines);
    mState.show_flags.set_from_value(show_bits);

    for (unsigned i=0; i<num_timelines; ++i)
    {
        Timeline tl;
        settings.GetValue("TrackWidget", QString("timeline_%1_self_id").arg(i), &tl.selfId);
        settings.GetValue("TrackWidget", QString("timeline_%1_node_id").arg(i), &tl.nodeId);
        mState.timelines.push_back(std::move(tl));
    }

    // try to restore the shared entity class object
    {
        std::string base64;
        settings.GetValue("TrackWidget", "entity", &base64);

        data::JsonObject json;
        auto [ok, error] = json.ParseString(base64::Decode(base64));
        if (!ok)
        {
            ERROR("Failed to parse content JSON. '%1'", error);
            return false;
        }

        auto ret = game::EntityClass::FromJson(json);
        if (!ret.has_value())
        {
            ERROR("Failed to load animation track widget state.");
            return false;
        }
        auto klass = std::move(ret.value());
        auto hash  = klass.GetHash();
        mState.entity = FindSharedEntity(hash);
        if (!mState.entity)
        {
            auto entity   = std::make_shared<game::EntityClass>(std::move(klass));
            mState.entity = entity;
            ShareEntity(entity);
        }
    }

    // restore the track state.
    {
        std::string base64;
        settings.GetValue("TrackWidget", "track", &base64);

        data::JsonObject json;
        auto [ok, error] = json.ParseString(base64::Decode(base64));
        if (!ok)
        {
            ERROR("Failed to parse content JSON. '%1'", error);
            return false;
        }
        auto ret = game::AnimationTrackClass::FromJson(json);
        if (!ret.has_value())
        {
            ERROR("Failed to load animation track state.");
            return false;
        }
        auto klass    = std::move(ret.value());
        mState.track  = std::make_shared<game::AnimationTrackClass>(std::move(klass));
        mOriginalHash = mState.track->GetHash();
    }

    mEntity = game::CreateEntityInstance(mState.entity);

    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();

    for (size_t i=0; i<mState.track->GetNumActuators(); ++i)
    {
        const auto& actuator = mState.track->GetActuatorClass(i);
        std::string trackId;
        settings.GetValue("TrackWidget", app::FromUtf8(actuator.GetId()), &trackId);
        mState.actuator_to_timeline[actuator.GetId()] = trackId;
    }
    mUI.timeline->SetDuration(mState.track->GetDuration());
    mUI.timeline->Rebuild();

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
            const auto max = mUI.zoom->maximum();
            const auto val = mUI.zoom->value();
            return val < max;
        } break;
        case Actions::CanZoomOut: {
            const auto min = mUI.zoom->minimum();
            const auto val = mUI.zoom->value();
            return val > min;
        } break;
    }
    BUG("Unhandled action query.");
    return false;
}

void AnimationTrackWidget::ZoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}

void AnimationTrackWidget::ZoomOut()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value - 0.1);
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

    // Playing back an animation track ?
    if (mPlayState != PlayState::Playing)
        return;

    mPlaybackAnimation->Update(secs);
    if (GetValue(mUI.actionUsePhysics))
    {
        mPhysics.Step();
        mPhysics.UpdateEntity(*mPlaybackAnimation);
    }
    mRenderer.Update(*mPlaybackAnimation, mCurrentTime, secs);

    if (!mPlaybackAnimation->IsPlaying())
    {
        mPhysics.DeleteAll();
        mPlaybackAnimation.reset();
        mUI.timeline->SetCurrentTime(0.0f);
        mUI.timeline->Update();
        mUI.timeline->SetFreezeItems(false);
        mUI.actionPlay->setEnabled(true);
        mUI.actionPause->setEnabled(false);
        mUI.actionStop->setEnabled(false);
        mUI.actionReset->setEnabled(true);
        mUI.actuatorGroup->setEnabled(true);
        mUI.baseGroup->setEnabled(true);
        mPlayState = PlayState::Stopped;
        NOTE("Animation finished.");
        DEBUG("Animation finished.");
    }
    else
    {
        const auto* track = mPlaybackAnimation->GetCurrentTrack();
        const auto time = track->GetCurrentTime();
        if (time >= 0)
        {
            mUI.timeline->SetCurrentTime(time);
            mUI.timeline->Repaint();
        }
    }
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

bool AnimationTrackWidget::ConfirmClose()
{
    const auto hash = mState.track->GetHash();
    if (hash == mOriginalHash)
        return true;

    QMessageBox msg(this);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    msg.setIcon(QMessageBox::Question);
    msg.setText(tr("Looks like you have unsaved changes. Would you like to save them?"));
    const auto ret = msg.exec();
    if (ret == QMessageBox::Cancel)
        return false;
    else if (ret == QMessageBox::No)
        return true;

    on_actionSave_triggered();
    return true;
}
bool AnimationTrackWidget::GetStats(Stats* stats) const
{
    if (mPlaybackAnimation)
    {
        const auto* track = mPlaybackAnimation->GetCurrentTrack();
        stats->time = track->GetCurrentTime();
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

void AnimationTrackWidget::RealizeEntityChange(std::shared_ptr<const game::EntityClass> klass)
{
    if (klass->GetId() != mState.entity->GetId())
        return;

    on_actionStop_triggered();

    RemoveDeletedItems();
    CreateTimelines();

    UpdateTrackUI();
    SetActuatorUIEnabled(false);
    SetActuatorUIDefaults("");
    SelectedItemChanged(nullptr);

    mEntity = game::CreateEntityInstance(mState.entity);
    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();
    mUI.timeline->ClearSelection();
    mUI.timeline->Rebuild();
    mRenderer.ClearPaintState();
}

void AnimationTrackWidget::on_actionPlay_triggered()
{
    if (mPlayState == PlayState::Paused)
    {
        mPlayState = PlayState::Playing;
        mUI.actionPause->setEnabled(true);
        return;
    }

    const auto& settings = mWorkspace->GetProjectSettings();

    // create new animation instance and play the animation track.
    auto track = game::CreateAnimationTrackInstance(mState.track);
    mPlaybackAnimation = game::CreateEntityInstance(mState.entity);
    mPlaybackAnimation->Play(std::move(track));
    mPhysics.SetClassLibrary(mWorkspace);
    mPhysics.SetScale(settings.physics_scale);
    mPhysics.SetGravity(settings.gravity);
    mPhysics.SetNumVelocityIterations(settings.num_velocity_iterations);
    mPhysics.SetNumPositionIterations(settings.num_position_iterations);
    mPhysics.SetTimestep(1.0f / settings.updates_per_second);
    mPhysics.CreateWorld(*mPlaybackAnimation);
    mPlayState = PlayState::Playing;

    mUI.actionPlay->setEnabled(false);
    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
    mUI.actionReset->setEnabled(false);
    mUI.actuatorGroup->setEnabled(false);
    mUI.baseGroup->setEnabled(false);
    mUI.timeline->SetFreezeItems(true);
}

void AnimationTrackWidget::on_actionPause_triggered()
{
    mPlayState = PlayState::Paused;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(true);
}

void AnimationTrackWidget::on_actionStop_triggered()
{
    mPlayState = PlayState::Stopped;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.actionReset->setEnabled(true);
    mUI.timeline->SetFreezeItems(false);
    mUI.timeline->SetCurrentTime(0.0f);
    mUI.timeline->Update();
    mUI.actuatorGroup->setEnabled(true);
    mUI.baseGroup->setEnabled(true);
    mPlaybackAnimation.reset();
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

    parent->SaveAnimationTrack(*mState.track, properties);
}

void AnimationTrackWidget::on_actionReset_triggered()
{
    if (mPlayState != PlayState::Stopped)
        return;
    mEntity = game::CreateEntityInstance(mState.entity);
    mUI.timeline->Rebuild();
    mUI.tree->Rebuild();
}

void AnimationTrackWidget::on_actionDeleteActuator_triggered()
{
    const auto* selected = mUI.timeline->GetSelectedItem();
    if (selected == nullptr)
        return;
    mState.track->DeleteActuatorById(app::ToUtf8(selected->id));
    mUI.timeline->ClearSelection();
    mUI.timeline->Rebuild();
    SelectedItemChanged(nullptr);
}

void AnimationTrackWidget::on_actionDeleteActuators_triggered()
{
    mState.track->Clear();
    mUI.timeline->ClearSelection();
    mUI.timeline->Rebuild();
    SelectedItemChanged(nullptr);
}

void AnimationTrackWidget::on_actionDeleteTimeline_triggered()
{
    if (!mUI.timeline->GetCurrentTimeline())
        return;
    const auto index = mUI.timeline->GetCurrentTimelineIndex();
    ASSERT(index < mState.timelines.size());
    auto tl = mState.timelines[index];

    for (size_t i=0; i<mState.track->GetNumActuators();)
    {
        const auto& actuator = mState.track->GetActuatorClass(i);
        const auto& timeline = mState.actuator_to_timeline[actuator.GetId()];
        if (timeline == tl.selfId) {
            mState.track->DeleteActuator(i);
        } else {
            ++i;
        }
    }
    mState.timelines.erase(mState.timelines.begin() + index);
    mUI.timeline->Rebuild();
}

void AnimationTrackWidget::on_trackName_textChanged(const QString&)
{
    mState.track->SetName(GetValue(mUI.trackName));
}

void AnimationTrackWidget::on_duration_valueChanged(double value)
{
    // adjust the actuator start/end bounds by scaling based
    // on the how growth co-efficient for the duration value.
    const float duration = mState.track->GetDuration();
    const auto start_lo_bound = mUI.actuatorStartTime->minimum();
    const auto start_hi_bound = mUI.actuatorStartTime->maximum();
    const auto end_lo_bound = mUI.actuatorEndTime->minimum();
    const auto end_hi_bound = mUI.actuatorEndTime->maximum();
    // important, must get the current value *before* setting new bounds
    // since setting the bounds will adjust the value.
    const float start = GetValue(mUI.actuatorStartTime);
    const float end   = GetValue(mUI.actuatorEndTime);
    SetMinMax(mUI.actuatorStartTime, start_lo_bound / duration * value,
                                     start_hi_bound / duration * value);
    SetMinMax(mUI.actuatorEndTime, end_lo_bound / duration * value,
                                   end_hi_bound / duration * value);
    SetValue(mUI.actuatorStartTime, start / duration * value);
    SetValue(mUI.actuatorEndTime, end / duration * value);

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

void AnimationTrackWidget::on_actuatorStartTime_valueChanged(double value)
{
    const auto* selected = mUI.timeline->GetSelectedItem();
    if (!selected)
        return;

    auto* node = mState.track->FindActuatorById(app::ToUtf8(selected->id));
    const auto duration  = mState.track->GetDuration();
    const auto old_start = node->GetStartTime();
    const auto new_start = value / duration;
    const auto change    = old_start - new_start;
    node->SetStartTime(new_start);
    node->SetDuration(node->GetDuration() + change);
    mUI.timeline->Rebuild();
}
void AnimationTrackWidget::on_actuatorEndTime_valueChanged(double value)
{
    const auto* selected = mUI.timeline->GetSelectedItem();
    if (!selected)
        return;

    auto* node = mState.track->FindActuatorById(app::ToUtf8(selected->id));
    const auto duration = mState.track->GetDuration();
    const auto start = node->GetStartTime();
    const auto end   = value / duration;
    node->SetDuration(end - start);
    mUI.timeline->Rebuild();
}

void AnimationTrackWidget::on_actuatorNode_currentIndexChanged(int index)
{
    SetActuatorUIDefaults(GetItemId(mUI.actuatorNode));
    SetActuatorUIEnabled(true);
}

void AnimationTrackWidget::on_actuatorType_currentIndexChanged(int index)
{
    SetEnabled(mUI.transformActuator, false);
    SetEnabled(mUI.setvalActuator,  false);
    SetEnabled(mUI.kinematicActuator, false);
    SetEnabled(mUI.setflagActuator,   false);

    const game::Actuator::Type type = GetValue(mUI.actuatorType);
    if (type == game::Actuator::Type::Transform)
    {
        SetEnabled(mUI.transformActuator, true);
        mUI.actuatorProperties->setCurrentWidget(mUI.transformActuator);
    }
    else if (type == game::Actuator::Type::SetValue)
    {
        SetEnabled(mUI.setvalActuator,  true);
        mUI.actuatorProperties->setCurrentWidget(mUI.setvalActuator);
    }
    else if (type == game::Actuator::Type::Kinematic)
    {
        SetEnabled(mUI.kinematicActuator, true);
        mUI.actuatorProperties->setCurrentWidget(mUI.kinematicActuator);
    }
    else if (type == game::Actuator::Type::SetFlag)
    {
        SetEnabled(mUI.setflagActuator,   true);
        mUI.actuatorProperties->setCurrentWidget(mUI.setflagActuator);
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

void AnimationTrackWidget::on_timeline_customContextMenuRequested(QPoint)
{
    const auto* selected = mUI.timeline->GetSelectedItem();
    mUI.actionDeleteActuator->setEnabled(selected != nullptr);
    mUI.actionDeleteActuators->setEnabled(mState.track->GetNumActuators());

    // map the click point to a position in the timeline.
    const auto* timeline = mUI.timeline->GetCurrentTimeline();
    mUI.actionDeleteTimeline->setEnabled(timeline != nullptr);

    // build menu for adding timelines
    QMenu add_timeline(this);
    add_timeline.setEnabled(true);
    add_timeline.setIcon(QIcon("icons:add.png"));
    add_timeline.setTitle(tr("New Timeline ..."));
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        QAction* action = add_timeline.addAction(app::FromUtf8(node.GetName()));
        action->setEnabled(true);
        action->setData(app::FromUtf8(node.GetId()));
        connect(action, &QAction::triggered, this, &AnimationTrackWidget::AddNodeTimelineAction);
    }

    QMenu show(this);
    show.setTitle(tr("Show Actuators ..."));
    for (const auto val : magic_enum::enum_values<game::ActuatorClass::Type>())
    {
        const std::string name(magic_enum::enum_name(val));
        QAction* action = show.addAction(app::FromUtf8(name));
        connect(action, &QAction::toggled, this, &AnimationTrackWidget::ToggleShowResource);
        action->setData(magic_enum::enum_integer(val));
        action->setCheckable(true);
        action->setChecked(mState.show_flags.test(val));
    }

    QMenu menu(this);
    // build menu for adding actuators.
    for (const auto val : magic_enum::enum_values<game::ActuatorClass::Type>())
    {
        const std::string name(magic_enum::enum_name(val));
        QAction* action = menu.addAction(QIcon("icons:add.png"),
                QString("New %1 Actuator").arg(app::FromUtf8(name)));
        action->setEnabled(false);
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
    menu.addMenu(&add_timeline);
    menu.addSeparator();
    menu.addAction(mUI.actionDeleteActuator);
    menu.addAction(mUI.actionDeleteActuators);
    menu.addAction(mUI.actionDeleteTimeline);
    menu.addSeparator();
    menu.addMenu(&show);
    menu.exec(QCursor::pos());
}

void AnimationTrackWidget::on_transformEndPosX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.x = value;
        node->SetTranslation(pos);
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndPosY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.y = value;
        node->SetTranslation(pos);
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndSizeX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto size = node->GetSize();
        size.x = value;
        node->SetSize(size);
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndSizeY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto size = node->GetSize();
        size.y = value;
        node->SetSize(size);
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndScaleX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto scale = node->GetScale();
        scale.x = value;
        node->SetScale(scale);
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndScaleY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto scale = node->GetScale();
        scale.y = value;
        node->SetScale(scale);
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndRotation_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetRotation(qDegreesToRadians(value));
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_setvalEndValue_ValueChanged()
{
    if (auto* node = GetCurrentNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_kinematicEndVeloX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if  (auto* body = node->GetRigidBody())
        {
            auto velo = body->GetLinearVelocity();
            velo.x = value;
            body->SetLinearVelocity(velo);
            SetSelectedActuatorProperties();
        }
    }
}
void AnimationTrackWidget::on_kinematicEndVeloY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if  (auto* body = node->GetRigidBody())
        {
            auto velo = body->GetLinearVelocity();
            velo.y = value;
            body->SetLinearVelocity(velo);
            SetSelectedActuatorProperties();
        }
    }
}
void AnimationTrackWidget::on_kinematicEndVeloZ_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if  (auto* body = node->GetRigidBody())
        {
            body->SetAngularVelocity(value);
            SetSelectedActuatorProperties();
        }
    }
}

void AnimationTrackWidget::on_itemFlags_currentIndexChanged(int)
{
    if (auto* node = GetCurrentNode())
    {

        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_flagAction_currentIndexChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        SetSelectedActuatorProperties();
    }
}

void AnimationTrackWidget::on_btnAddActuator_clicked()
{
    // the combobox defines which node is selected as the
    // target node of the actuator
    const auto* node = mState.entity->FindNodeById(GetItemId(mUI.actuatorNode));

    // for now simply find the first timeline that matches if any.
    size_t timeline_index = mState.timelines.size();
    for (size_t i=0; i<mState.timelines.size(); ++i)
    {
        if (mState.timelines[i].nodeId == node->GetId())
        {
            timeline_index = i;
            break;
        }
    }
    // no such timeline at all? then create a new one.
    if (timeline_index == mState.timelines.size())
    {
        Timeline tl;
        tl.selfId = base::RandomString(10);
        tl.nodeId = node->GetId();
        mState.timelines.push_back(std::move(tl));
    }

    ASSERT(timeline_index < mState.timelines.size());
    const auto& timeline = mState.timelines[timeline_index];

    // get the animation duration in seconds and normalize the actuator times.
    const float animation_duration = GetValue(mUI.duration);
    const float actuator_start = GetValue(mUI.actuatorStartTime);
    const float actuator_end   = GetValue(mUI.actuatorEndTime);
    if (actuator_start >= actuator_end)
    {
        NOTE("Actuator start time must come before end time.");
        mUI.actuatorStartTime->setFocus();
        return;
    }
    const auto norm_start = actuator_start / animation_duration;
    const auto norm_end   = actuator_end / animation_duration;

    const auto& id = node->GetId();
    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i=0; i<mState.track->GetNumActuators(); ++i)
    {
        const auto& klass = mState.track->GetActuatorClass(i);
        if (mState.actuator_to_timeline[klass.GetId()] != timeline.selfId)
            continue;
        const auto start = klass.GetStartTime();
        const auto end   = klass.GetStartTime() + klass.GetDuration();
        if (start >= norm_start)
            hi_bound = std::min(hi_bound, start);
        if (end <= norm_start)
            lo_bound = std::max(lo_bound, end);
        // this isn't a free slot actually.
        if (norm_start >= start && norm_start <= end)
        {
            NOTE("No available time slot found.");
            return;
        }
    }
    const auto start = std::max(lo_bound, norm_start);
    const auto end   = std::min(hi_bound, norm_end);
    const game::Actuator::Type type = GetValue(mUI.actuatorType);
    AddActuatorFromUI(timeline.selfId, node->GetId(), type, start, end - start);
}

void AnimationTrackWidget::SetActuatorUIEnabled(bool enabled)
{
    if (enabled)
    {
        SetEnabled(mUI.actuatorType,       true);
        SetEnabled(mUI.actuatorStartTime,  true);
        SetEnabled(mUI.actuatorEndTime,    true);
        SetEnabled(mUI.actuatorProperties, true);
        SetEnabled(mUI.btnAddActuator,     true);
        SetEnabled(mUI.transformActuator,  true);
        SetValue(mUI.actuatorType, game::ActuatorClass::Type::Transform);
        mUI.actuatorProperties->setCurrentWidget(mUI.transformActuator);
    }
    else
    {
        SetEnabled(mUI.actuatorType,       false);
        SetEnabled(mUI.actuatorStartTime,  false);
        SetEnabled(mUI.actuatorEndTime,    false);
        SetEnabled(mUI.actuatorProperties, false);
        SetEnabled(mUI.btnAddActuator,     false);
        SetEnabled(mUI.transformActuator,  false);
        SetValue(mUI.actuatorType, game::ActuatorClass::Type::Transform);
        mUI.actuatorProperties->setCurrentWidget(mUI.transformActuator);
    }
}

void AnimationTrackWidget::SetActuatorUIDefaults(const std::string& nodeId)
{
    SetMinMax(mUI.actuatorStartTime, 0.0, 0.0);
    SetMinMax(mUI.actuatorEndTime, 0.0, 0.0);

    SetValue(mUI.transformInterpolation, game::TransformActuatorClass::Interpolation::Cosine);
    SetValue(mUI.transformEndPosX, 0.0f);
    SetValue(mUI.transformEndPosY, 0.0f);
    SetValue(mUI.transformEndSizeX, 0.0f);
    SetValue(mUI.transformEndSizeY, 0.0f);
    SetValue(mUI.transformEndScaleX, 0.0f);
    SetValue(mUI.transformEndScaleY, 0.0f);
    SetValue(mUI.transformEndRotation, 0.0f);
    SetValue(mUI.setvalInterpolation, game::SetValueActuatorClass::Interpolation::Cosine);
    SetValue(mUI.setvalName, game::SetValueActuatorClass::ParamName::DrawableTimeScale);
    SetValue(mUI.setvalEndValue, 0.0f);
    SetValue(mUI.kinematicInterpolation, game::KinematicActuatorClass::Interpolation::Cosine);
    SetValue(mUI.kinematicEndVeloX, 0.0f);
    SetValue(mUI.kinematicEndVeloY, 0.0f);
    SetValue(mUI.kinematicEndVeloZ, 0.0f);
    SetValue(mUI.actuatorStartTime, 0.0f);
    SetValue(mUI.actuatorEndTime, 0.0f);

    if (!mState.track)
        return;
    // there could be multiple slots where the next actuator is
    // to placed. the limits would be difficult to express
    // with the spinboxes, so we just reset the min/max to the
    // whole animation duration and then clamp on add if needed.
    const auto duration = mState.track->GetDuration();
    SetMinMax(mUI.actuatorStartTime, 0.0, duration);
    SetMinMax(mUI.actuatorEndTime, 0.0, duration);
    SetValue(mUI.actuatorEndTime, mState.track->GetDuration());

    if (nodeId.empty())
        return;

    const auto* node = mState.entity->FindNodeById(nodeId);
    // using the node's current transformation data
    // as the default end transformation. I.e. "no transformation"
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
    if (const auto* draw = node->GetDrawable())
    {
        SetValue(mUI.setvalEndValue, draw->GetTimeScale());
    }
}

void AnimationTrackWidget::SetSelectedActuatorProperties()
{
    if (mPlayState != PlayState::Stopped)
        return;
    // what's the current actuator? The one that is selected in the timeline
    // (if any).
    const auto* item = mUI.timeline->GetSelectedItem();
    if (item == nullptr)
        return;

    auto* klass = mState.track->FindActuatorById(app::ToUtf8(item->id));
    if (auto* transform = dynamic_cast<game::TransformActuatorClass*>(klass))
    {
        transform->SetInterpolation(GetValue(mUI.transformInterpolation));
        transform->SetEndPosition(GetValue(mUI.transformEndPosX), GetValue(mUI.transformEndPosY));
        transform->SetEndSize(GetValue(mUI.transformEndSizeX), GetValue(mUI.transformEndSizeY));
        transform->SetEndScale(GetValue(mUI.transformEndScaleX), GetValue(mUI.transformEndScaleY));
        transform->SetEndRotation(qDegreesToRadians((float) GetValue(mUI.transformEndRotation)));
    }
    else if (auto* setter = dynamic_cast<game::SetValueActuatorClass*>(klass))
    {
        using Name = game::SetValueActuatorClass::ParamName;
        const auto name = (Name)GetValue(mUI.setvalName);
        if (name == Name::DrawableTimeScale)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::LinearVelocityX)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::LinearVelocityY)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::AngularVelocity)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::LinearVelocity)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Vec2);
            setter->SetEndValue(mUI.setvalEndValue->GetAsVec2());
        }
        else if (name == Name::TextItemText)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::String);
            setter->SetEndValue(app::ToUtf8(mUI.setvalEndValue->GetAsString()));
        }
        else if (name == Name::TextItemColor)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Color);
            setter->SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        }
        else BUG("Unhandled value actuator value type.");
        setter->SetInterpolation(GetValue(mUI.setvalInterpolation));
        setter->SetParamName(name);
    }
    else if (auto* kinematic = dynamic_cast<game::KinematicActuatorClass*>(klass))
    {
        glm::vec2 velocity;
        velocity.x = GetValue(mUI.kinematicEndVeloX);
        velocity.y = GetValue(mUI.kinematicEndVeloY);
        kinematic->SetInterpolation(GetValue(mUI.kinematicInterpolation));
        kinematic->SetEndLinearVelocity(velocity);
        kinematic->SetEndAngularVelocity(GetValue(mUI.kinematicEndVeloZ));
    }
    else if (auto* setflag = dynamic_cast<game::SetFlagActuatorClass*>(klass))
    {
        setflag->SetFlagAction(GetValue(mUI.flagAction));
        setflag->SetFlagName(GetValue(mUI.itemFlags));
    }
    DEBUG("Updated actuator '%1' (%2)", item->text, item->id);

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
    const auto index = mUI.actuatorNode->currentIndex();
    if (index == -1)
        return;
    // reset the properties of the actuator to the original node class values
    // first set the values to the UI widgets and then ask the actuator's
    // state to be updated from the UI.
    const auto* klass   = mState.entity->FindNodeById(GetItemId(mUI.actuatorNode));
    const auto& pos     = klass->GetTranslation();
    const auto& size    = klass->GetSize();
    const auto& scale   = klass->GetScale();
    const auto rotation = klass->GetRotation();
    SetValue(mUI.transformEndPosX, pos.x);
    SetValue(mUI.transformEndPosY, pos.y);
    SetValue(mUI.transformEndSizeX, size.x);
    SetValue(mUI.transformEndSizeY, size.y);
    SetValue(mUI.transformEndScaleX, scale.x);
    SetValue(mUI.transformEndScaleY, scale.y);
    SetValue(mUI.transformEndRotation, qRadiansToDegrees(rotation));

    // apply the reset to the visualization entity and to its node instance.
    auto* node = mEntity->FindNodeByClassId(GetItemId(mUI.actuatorNode));
    node->SetTranslation(pos);
    node->SetSize(size);
    node->SetScale(scale);
    node->SetRotation(rotation);

    SetSelectedActuatorProperties();
}

void AnimationTrackWidget::on_btnViewPlus90_clicked()
{
    const auto value = mUI.viewRotation->value();
    mUI.viewRotation->setValue(math::clamp(-180.0, 180.0, value + 90.0f));
    mViewTransformRotation = value;
    mViewTransformStartTime = mCurrentTime;
}
void AnimationTrackWidget::on_btnViewMinus90_clicked()
{
    const auto value = mUI.viewRotation->value();
    mUI.viewRotation->setValue(math::clamp(-180.0, 180.0, value - 90.0f));
    mViewTransformRotation = value;
    mViewTransformStartTime = mCurrentTime;
}
void AnimationTrackWidget::on_btnViewReset_clicked()
{
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto rotation = mUI.viewRotation->value();

    mState.camera_offset_x = width * 0.5f;
    mState.camera_offset_y = height * 0.5f;
    mViewTransformRotation = rotation;
    mViewTransformStartTime = mCurrentTime;
    // this is camera offset to the center of the widget.
    mUI.viewPosX->setValue(0);
    mUI.viewPosY->setValue(0);
    mUI.viewScaleX->setValue(1.0f);
    mUI.viewScaleY->setValue(1.0f);
    mUI.viewRotation->setValue(0);
}

void AnimationTrackWidget::SelectedItemChanged(const TimelineWidget::TimelineItem* item)
{
    SetEnabled(mUI.transformActuator, false);
    SetEnabled(mUI.setvalActuator,  false);
    SetEnabled(mUI.kinematicActuator, false);
    SetEnabled(mUI.setflagActuator,   false);

    if (item == nullptr)
    {
        const auto duration = mState.track->GetDuration();
        SetMinMax(mUI.actuatorStartTime, 0.0, duration);
        SetMinMax(mUI.actuatorEndTime, 0.0, duration);
        SetValue(mUI.actuatorType, game::ActuatorClass::Type::Transform);
        SetValue(mUI.actuatorStartTime, 0.0f);
        SetValue(mUI.actuatorEndTime, duration);
        SetValue(mUI.transformInterpolation, game::TransformActuatorClass::Interpolation::Cosine);
        SetValue(mUI.transformEndPosX, 0.0f);
        SetValue(mUI.transformEndPosY, 0.0f);
        SetValue(mUI.transformEndSizeX, 0.0f);
        SetValue(mUI.transformEndSizeY, 0.0f);
        SetValue(mUI.transformEndScaleX, 0.0f);
        SetValue(mUI.transformEndScaleY, 0.0f);
        SetValue(mUI.transformEndRotation, 0.0f);
        SetValue(mUI.setvalInterpolation, game::SetValueActuatorClass::Interpolation::Cosine);
        SetValue(mUI.setvalEndValue, 0.0f);
        SetValue(mUI.kinematicInterpolation, game::KinematicActuatorClass::Interpolation::Cosine);
        SetValue(mUI.kinematicEndVeloX, 0.0f);
        SetValue(mUI.kinematicEndVeloY, 0.0f);
        SetValue(mUI.kinematicEndVeloZ, 0.0f);
        mUI.actuatorNode->setCurrentIndex(-1);
        mUI.actuatorGroup->setTitle("Actuator");
        SetEnabled(mUI.actuatorNode, true);
        SetEnabled(mUI.actuatorType, false);
        SetEnabled(mUI.actuatorStartTime, false);
        SetEnabled(mUI.actuatorEndTime, false);
        SetEnabled(mUI.actuatorProperties, false);
        SetEnabled(mUI.btnAddActuator, false);
    }
    else
    {
        const auto* actuator = mState.track->FindActuatorById(app::ToUtf8(item->id));
        const auto duration = mState.track->GetDuration();
        const auto start = actuator->GetStartTime() * duration;
        const auto end   = actuator->GetDuration() * duration  + start;
        const auto node  = mEntity->FindNodeByClassId(actuator->GetNodeId());

        // figure out the hi/lo (left right) limits for the spinbox start
        // and time values for this actuator.
        float lo_bound = 0.0f;
        float hi_bound = 1.0f;
        for (size_t i=0; i<mState.track->GetNumActuators(); ++i)
        {
            const auto& klass = mState.track->GetActuatorClass(i);
            if (klass.GetId() == actuator->GetId())
                continue;
            else if (klass.GetNodeId() != actuator->GetNodeId())
                continue;
            const auto start = klass.GetStartTime();
            const auto end   = klass.GetStartTime() + klass.GetDuration();
            if (start >= actuator->GetStartTime())
                hi_bound = std::min(hi_bound, start);
            if (end <= actuator->GetStartTime())
                lo_bound = std::max(lo_bound, end);
        }
        const auto& len = QString::number(end-start, 'f', 2);
        SetMinMax(mUI.actuatorStartTime, lo_bound * duration, hi_bound * duration);
        SetMinMax(mUI.actuatorEndTime, lo_bound * duration, hi_bound * duration);
        SetValue(mUI.actuatorStartTime, start);
        SetValue(mUI.actuatorEndTime, end);
        SetValue(mUI.actuatorNode, ListItemId(app::FromUtf8(node->GetClassId())));
        SetValue(mUI.actuatorType, actuator->GetType());
        SetValue(mUI.actuatorGroup, tr("Actuator - %1, %2s").arg(item->text).arg(len));
        SetEnabled(mUI.btnAddActuator, false);
        SetEnabled(mUI.actuatorType, false);
        SetEnabled(mUI.actuatorNode, false);
        SetEnabled(mUI.actuatorStartTime, true);
        SetEnabled(mUI.actuatorEndTime, true);

        if (const auto* ptr = dynamic_cast<const game::TransformActuatorClass*>(actuator))
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
            SetEnabled(mUI.actuatorProperties, true);
            SetEnabled(mUI.transformActuator, true);
            mUI.actuatorProperties->setCurrentWidget(mUI.transformActuator);

            node->SetTranslation(pos);
            node->SetSize(size);
            node->SetScale(scale);
            node->SetRotation(rotation);
        }
        else if (const auto* ptr = dynamic_cast<const game::SetValueActuatorClass*>(actuator))
        {
            const auto name = ptr->GetParamName();
            using Name = game::SetValueActuatorClass::ParamName;
            if (name == Name::DrawableTimeScale)
                SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
            else if (name == Name::LinearVelocity)
                SetValue(mUI.setvalEndValue, *ptr->GetEndValue<glm::vec2>());
            else if (name == Name::LinearVelocityX)
                SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
            else if (name == Name::LinearVelocityY)
                SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
            else if (name == Name::AngularVelocity)
                SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>());
            else if (name == Name::TextItemColor)
                SetValue(mUI.setvalEndValue, *ptr->GetEndValue<base::Color4f>());
            else if (name == Name::TextItemText)
                SetValue(mUI.setvalEndValue, *ptr->GetEndValue<std::string>());
            else BUG("Unhandled set value actuator value type.");

            SetValue(mUI.setvalInterpolation, ptr->GetInterpolation());
            SetValue(mUI.setvalName, ptr->GetParamName());
            SetEnabled(mUI.actuatorProperties, true);
            SetEnabled(mUI.setvalActuator, true);
            mUI.actuatorProperties->setCurrentWidget(mUI.setvalActuator);
        }
        else if (const auto* ptr = dynamic_cast<const game::KinematicActuatorClass*>(actuator))
        {
            const auto& linear_velocity = ptr->GetEndLinearVelocity();
            SetValue(mUI.kinematicInterpolation, ptr->GetInterpolation());
            SetValue(mUI.kinematicEndVeloX, linear_velocity.x);
            SetValue(mUI.kinematicEndVeloY, linear_velocity.y);
            SetValue(mUI.kinematicEndVeloZ, ptr->GetEndAngularVelocity());
            SetEnabled(mUI.actuatorProperties, true);
            SetEnabled(mUI.kinematicActuator, true);
            mUI.actuatorProperties->setCurrentWidget(mUI.kinematicActuator);
        }
        else if (const auto* ptr = dynamic_cast<const game::SetFlagActuatorClass*>(actuator))
        {
            SetValue(mUI.itemFlags, ptr->GetFlagName());
            SetValue(mUI.flagAction, ptr->GetFlagAction());
            SetEnabled(mUI.actuatorProperties, true);
            SetEnabled(mUI.setflagActuator, true);
            mUI.actuatorProperties->setCurrentWidget(mUI.setflagActuator);
        }
        else
        {
            mUI.actuatorProperties->setEnabled(false);
        }
        DEBUG("Selected timeline item '%1' (%2)", item->text, item->id);
    }
}

void AnimationTrackWidget::SelectedItemDragged(const TimelineWidget::TimelineItem* item)
{
    auto* actuator = mState.track->FindActuatorById(app::ToUtf8(item->id));
    actuator->SetStartTime(item->starttime);
    actuator->SetDuration(item->duration);

    const auto duration = mState.track->GetDuration();
    const auto start = actuator->GetStartTime() * duration;
    const auto end   = actuator->GetDuration() * duration + start;
    const auto len   = QString::number(end-start, 'f', 2);
    SetValue(mUI.actuatorStartTime, start);
    SetValue(mUI.actuatorEndTime, end);
    SetValue(mUI.actuatorGroup, tr("Actuator - %1, %2s").arg(item->text).arg(len));
}

void AnimationTrackWidget::ToggleShowResource()
{
    QAction* action = qobject_cast<QAction*>(sender());
    const auto payload = action->data().toInt();
    const auto type = magic_enum::enum_cast<game::ActuatorClass::Type>(payload);
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
    QString text = action->text();
    text.remove("New ");
    text.remove(" Actuator");

    const auto type = magic_enum::enum_cast<game::ActuatorClass::Type>(app::ToUtf8(text));
    ASSERT(type.has_value());
    AddActuatorFromTimeline(type.value(), seconds);
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

void AnimationTrackWidget::InitScene(unsigned int width, unsigned int height)
{
    if (!mCameraWasLoaded)
    {
        // if the camera hasn't been loaded then compute now the
        // initial position for the camera.
        mState.camera_offset_x = mUI.widget->width()  * 0.5;
        mState.camera_offset_y = mUI.widget->height() * 0.5;
    }
    DisplayCurrentCameraLocation();
}

void AnimationTrackWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.viewScaleX);
    const auto ys     = (float)GetValue(mUI.viewScaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto view_rotation_time = math::clamp(0.0f, 1.0f,
        mCurrentTime - mViewTransformStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.viewRotation->value(),
        view_rotation_time, math::Interpolation::Cosine);

    painter.SetViewport(0, 0, width, height);
    painter.SetPixelRatio(glm::vec2(xs*zoom, ys*zoom));

    gfx::Transform view;
    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    view.Push();
    view.Scale(xs, ys);
    view.Scale(zoom, zoom);
    view.Rotate(qDegreesToRadians(view_rotation_angle));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    // render endless background grid.
    if (mUI.chkShowGrid->isChecked())
    {
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, width, height);
    }

    // begin the animation transformation space
    view.Push();
        mRenderer.BeginFrame();
        if (mPlaybackAnimation)
        {
            mRenderer.Draw(*mPlaybackAnimation, painter, view, nullptr);
        }
        else
        {
            DrawHook hook(GetCurrentNode(), mPlayState == PlayState::Playing);
            mRenderer.Draw(*mEntity, painter, view, &hook);
        }
        mRenderer.EndFrame();
    view.Pop();

    // right arrow
    if (GetValue(mUI.chkShowOrigin))
    {
        DrawBasisVectors(painter, view);
    }

    if (GetValue(mUI.chkShowViewport))
    {
        const auto& settings    = mWorkspace->GetProjectSettings();
        const float game_width  = settings.viewport_width;
        const float game_height = settings.viewport_height;
        DrawViewport(painter, view, game_width, game_height, width, height);
    }

    // pop view transformation
    view.Pop();
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

    glm::vec4 mickey_pos_in_entity;
    glm::vec4 mickey_pos_in_widget;

    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.viewScaleX) , GetValue(mUI.viewScaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.viewRotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        const auto& mat = glm::inverse(view.GetAsMatrix());
        mickey_pos_in_entity = mat * glm::vec4(mickey.x() , mickey.y() , 1.0f , 1.0f);
    }

    zoom_function();

    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.viewScaleX) , GetValue(mUI.viewScaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.viewRotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        const auto& mat = view.GetAsMatrix();
        mickey_pos_in_widget = mat * mickey_pos_in_entity;
    }
    mState.camera_offset_x += (mickey.x() - mickey_pos_in_widget.x);
    mState.camera_offset_y += (mickey.y() - mickey_pos_in_widget.y);
    DisplayCurrentCameraLocation();
}

void AnimationTrackWidget::MouseMove(QMouseEvent* mickey)
{
    if (mCurrentTool)
    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.viewScaleX), GetValue(mUI.viewScaleY));
        view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.viewRotation->value()));
        view.Translate(mState.camera_offset_x, mState.camera_offset_y);
        mCurrentTool->MouseMove(mickey, view);

        // update the properties that might have changed as the result of application
        // of the current tool.
        DisplayCurrentCameraLocation();
        UpdateTransformActuatorUI();
        SetSelectedActuatorProperties();
    }
}
void AnimationTrackWidget::MousePress(QMouseEvent* mickey)
{
    gfx::Transform view;
    view.Scale(GetValue(mUI.viewScaleX), GetValue(mUI.viewScaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.viewRotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (!mCurrentTool && mPlayState == PlayState::Stopped)
    {
        auto* current = GetCurrentNode();
        auto [hitnode, hitpos] = SelectNode(mickey->pos(), view, *mEntity, current);
        if (hitnode && hitnode == current)
        {
            view.Push(mEntity->FindNodeTransform(hitnode));
                const auto mat = view.GetAsMatrix();
                glm::vec3 scale;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::quat orientation;
                glm::decompose(mat, scale, orientation, translation, skew,  perspective);
            view.Pop();
            const auto& size = hitnode->GetSize();
            const auto& box_size = glm::vec2(10.0f/scale.x, 10.0f/scale.y);
            // check if any particular special area of interest is being hit
            const bool bottom_right_hitbox_hit = hitpos.x >= size.x - box_size.x &&
                                                 hitpos.y >= size.y - box_size.y;
            const bool top_left_hitbox_hit = hitpos.x >= 0 && hitpos.x <= box_size.x &&
                                             hitpos.y >= 0 && hitpos.y <= box_size.y;
            const auto snap = (bool)GetValue(mUI.chkSnap);
            const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
            const auto grid_size = (unsigned)grid;

            if (bottom_right_hitbox_hit)
                mCurrentTool.reset(new ResizeRenderTreeNodeTool(*mEntity, hitnode));
            else if (top_left_hitbox_hit)
                mCurrentTool.reset(new RotateRenderTreeNodeTool(*mEntity, hitnode));
            else mCurrentTool.reset(new MoveRenderTreeNodeTool(*mEntity, hitnode, snap, grid_size));
        }
        else if (!mUI.timeline->GetSelectedItem() && hitnode)
        {
            // pick a new node as the selected actuator node
            SetValue(mUI.actuatorNode, ListItemId(hitnode->GetClassId()));
            SetActuatorUIDefaults(hitnode->GetClassId());
            SetActuatorUIEnabled(true);
        }
        else if (!mUI.timeline->GetSelectedItem())
        {
            SetValue(mUI.actuatorNode, QString(""));
            SetActuatorUIDefaults("");
            SetActuatorUIEnabled(false);
        }
    }
    if (!mCurrentTool)
        mCurrentTool.reset(new MoveCameraTool(mState));

    mCurrentTool->MousePress(mickey, view);
}
void AnimationTrackWidget::MouseRelease(QMouseEvent* mickey)
{
    if (!mCurrentTool)
        return;
    gfx::Transform view;
    view.Scale(GetValue(mUI.viewScaleX), GetValue(mUI.viewScaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.viewRotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (mCurrentTool->MouseRelease(mickey, view))
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

    // list the nodes in the entity class.
    std::vector<ListItem> items;
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        ListItem item;
        item.name = app::FromUtf8(node.GetName());
        item.id   = app::FromUtf8(node.GetId());
        items.push_back(std::move(item));
    }
    SetList(mUI.actuatorNode, items);

    setWindowTitle(GetValue(mUI.trackName));
}

void AnimationTrackWidget::UpdateTransformActuatorUI()
{
    if (mPlayState != PlayState::Stopped)
        return;
    if (const auto* node = GetCurrentNode())
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

void AnimationTrackWidget::AddActuatorFromTimeline(game::ActuatorClass::Type type, float seconds)
{
    if (!mUI.timeline->GetCurrentTimeline())
        return;
    const auto timeline_index = mUI.timeline->GetCurrentTimelineIndex();
    if (timeline_index >= mState.timelines.size())
        return;

    const auto& timeline = mState.timelines[timeline_index];

    // get the node from the animation class object.
    // the class node's transform values are used for the
    // initial data for the actuator.
    SetActuatorUIDefaults(timeline.nodeId);

    const auto* node = mState.entity->FindNodeById(timeline.nodeId);
    const auto duration = mState.track->GetDuration();
    const auto position = seconds / duration;

    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i=0; i<mState.track->GetNumActuators(); ++i)
    {
        const auto& klass = mState.track->GetActuatorClass(i);
        if (mState.actuator_to_timeline[klass.GetId()] != timeline.selfId)
            continue;
        const auto start = klass.GetStartTime();
        const auto end   = klass.GetStartTime() + klass.GetDuration();
        if (start >= position)
            hi_bound = std::min(hi_bound, start);
        if (end <= position)
            lo_bound = std::max(lo_bound, end);
    }
    AddActuatorFromUI(timeline.selfId, node->GetId(), type, lo_bound, hi_bound - lo_bound);
}

void AnimationTrackWidget::AddActuatorFromUI(const std::string& timelineId, const std::string& nodeId,
                                             game::ActuatorClass::Type type,
                                             float start_time, float duration)
{
    if (type == game::ActuatorClass::Type::Transform)
    {
        game::TransformActuatorClass klass;
        klass.SetNodeId(nodeId);
        klass.SetStartTime(start_time);
        klass.SetDuration(duration);
        klass.SetEndPosition(GetValue(mUI.transformEndPosX), GetValue(mUI.transformEndPosY));
        klass.SetEndSize(GetValue(mUI.transformEndSizeX), GetValue(mUI.transformEndSizeY));
        klass.SetEndScale(GetValue(mUI.transformEndScaleX), GetValue(mUI.transformEndScaleY));
        klass.SetInterpolation(GetValue(mUI.transformInterpolation));
        klass.SetEndRotation(qDegreesToRadians((float) GetValue(mUI.transformEndRotation)));
        mState.actuator_to_timeline[klass.GetId()] = timelineId;
        mState.track->AddActuator(klass);
    }
    else if (type == game::ActuatorClass::Type::SetValue)
    {
        using Name = game::SetValueActuatorClass::ParamName;
        const auto name = (Name)GetValue(mUI.setvalName);
        game::SetValueActuatorClass klass;
        klass.SetNodeId(nodeId);
        klass.SetStartTime(start_time);
        klass.SetDuration(duration);
        klass.SetParamName(GetValue(mUI.setvalName));
        klass.SetInterpolation(GetValue(mUI.setvalInterpolation));
        if (name == Name::DrawableTimeScale)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (name == Name::LinearVelocityX)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (name == Name::LinearVelocityY)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (name == Name::AngularVelocity)
            klass.SetEndValue(mUI.setvalEndValue->GetAsFloat());
        else if (name == Name::LinearVelocity)
            klass.SetEndValue(mUI.setvalEndValue->GetAsVec2());
        else if (name == Name::TextItemText)
            klass.SetEndValue(app::ToUtf8(mUI.setvalEndValue->GetAsString()));
        else if (name == Name::TextItemColor)
            klass.SetEndValue(ToGfx(mUI.setvalEndValue->GetAsColor()));
        else BUG("Unhandled value actuator value type.");

        mState.actuator_to_timeline[klass.GetId()] = timelineId;
        mState.track->AddActuator(klass);
    }
    else if (type == game::ActuatorClass::Type::Kinematic)
    {
        game::KinematicActuatorClass klass;
        klass.SetNodeId(nodeId);
        klass.SetStartTime(start_time);
        klass.SetDuration(duration);
        klass.SetEndAngularVelocity(GetValue(mUI.kinematicEndVeloZ));
        glm::vec2 velocity;
        velocity.x = GetValue(mUI.kinematicEndVeloX);
        velocity.y = GetValue(mUI.kinematicEndVeloY);
        klass.SetEndLinearVelocity(velocity);
        mState.actuator_to_timeline[klass.GetId()] = timelineId;
        mState.track->AddActuator(klass);
    }
    else if (type == game::ActuatorClass::Type::SetFlag)
    {
        game::SetFlagActuatorClass klass;
        klass.SetNodeId(nodeId);
        klass.SetStartTime(start_time);
        klass.SetDuration(duration);
        klass.SetFlagName(GetValue(mUI.itemFlags));
        klass.SetFlagAction(GetValue(mUI.flagAction));
        mState.actuator_to_timeline[klass.GetId()] = timelineId;
        mState.track->AddActuator(klass);
    }
    mUI.timeline->Rebuild();

    const float end = start_time + duration;
    const float animation_duration = GetValue(mUI.duration);
    DEBUG("New %1 actuator for node '%2' from %3s to %4s", type, nodeId,
          start_time * animation_duration, end * animation_duration);
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
    // remove orphaned actuators.
    std::vector<std::string> dead_actuators;
    for (size_t i=0; i<mState.track->GetNumActuators(); ++i)
    {
        const auto& actuator = mState.track->GetActuatorClass(i);
        const auto& node = actuator.GetNodeId();
        if (mState.entity->FindNodeById(node))
            continue;
        dead_actuators.push_back(actuator.GetId());
    }
    for (const auto& id : dead_actuators)
    {
        mState.track->DeleteActuatorById(id);
        mState.actuator_to_timeline.erase(id);
    }

    // remove orphaned timelines.
    for (auto it = mState.timelines.begin();  it != mState.timelines.end();)
    {
        const auto& timeline = *it;
        if (mState.entity->FindNodeById(timeline.nodeId)) {
            ++it;
            continue;
        } else {
            it = mState.timelines.erase(it);
        }
    }
}

void AnimationTrackWidget::DisplayCurrentCameraLocation()
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto dist_x = mState.camera_offset_x - (width / 2.0f);
    const auto dist_y = mState.camera_offset_y - (height / 2.0f);
    SetValue(mUI.viewPosX, dist_x);
    SetValue(mUI.viewPosY, dist_y);
}

game::EntityNode* AnimationTrackWidget::GetCurrentNode()
{
    const auto index = mUI.actuatorNode->currentIndex();
    if (index == -1)
        return nullptr;
    return mEntity->FindNodeByClassId(GetItemId(mUI.actuatorNode));
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
