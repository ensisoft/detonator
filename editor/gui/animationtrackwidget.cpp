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
#include "game/entity_class.h"
#include "game/timeline_animation.h"
#include "game/timeline_animator.h"
#include "game/timeline_animation_trigger.h"
#include "game/timeline_transform_animator.h"
#include "game/timeline_kinematic_animator.h"
#include "game/timeline_material_animator.h"
#include "game/timeline_property_animator.h"
#include "graphics/transform.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "graphics/material_class.h"
#include "graphics/texture_map.h"
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
    explicit TimelineModel(AnimationTrackWidget::State& state) : mState(state)
    {}
    void Fetch(std::vector<TimelineWidget::Timeline>* list) override
    {
        const auto& track = *mState.track;
        const auto& anim  = *mState.entity;
        // map node ids to indices in the animation's list of nodes.
        std::unordered_map<std::string, size_t> id_to_index_map;

        // setup all timelines with empty item vectors.
        for (const auto& item : mState.timelines)
        {
            const auto* node = mState.entity->FindNodeById(item.target_node_id);
            const auto& name = node->GetName();
            // map timeline objects to widget timeline indices
            id_to_index_map[item.timeline_id] = list->size();
            TimelineWidget::Timeline line;
            line.SetName(node->GetName());
            list->push_back(line);
        }
        // go over the existing actuators and create timeline items
        // for visual representation of each animator.
        for (size_t i=0; i< track.GetNumAnimators(); ++i)
        {
            const auto& animator = track.GetAnimatorClass(i);
            const auto animator_type  = animator.GetType();
            const auto& animator_name = animator.GetName();
            if (!mState.show_flags.test(animator_type))
                continue;

            const auto& target_node_id = animator.GetNodeId();
            const auto& timeline_id    = animator.GetTimelineId();
            const auto timeline_index = id_to_index_map[timeline_id];
            const auto* target_node = mState.entity->FindNodeById(target_node_id);
            const auto& target_name = target_node->GetName();

            // pastel color palette.
            // https://colorhunt.co/palette/226038

            TimelineWidget::TimelineItem item;
            item.type      = TimelineWidget::TimelineItem::Type::Span;
            item.text      = app::toString("%1 (%2)", target_name, animator_name);
            item.id        = animator.GetId();
            item.starttime = animator.GetStartTime();
            item.duration  = animator.GetDuration();
            if (animator_type == game::AnimatorClass::Type::BooleanPropertyAnimator)
                item.color = QColor(0xa3, 0xdd, 0xcb, 150);
            else if (animator_type == game::AnimatorClass::Type::TransformAnimator)
                item.color = QColor(0xe8, 0xe9, 0xa1, 150);
            else if (animator_type == game::AnimatorClass::Type::KinematicAnimator)
                item.color = QColor(0xe6, 0xb5, 0x66, 150);
            else if (animator_type == game::AnimatorClass::Type::PropertyAnimator)
                item.color = QColor(0xe5, 0x70, 0x7e, 150);
            else if (animator_type == game::AnimatorClass::Type::MaterialAnimator)
                item.color = QColor(0xe7, 0x80, 0x7e, 150);
            else BUG("Unhandled type for item colorization.");

            (*list)[timeline_index].AddItem(item);
        }

        // Go over the triggers and create timeline trigger items
        // for visually representing each trigger.
        for (size_t i=0; i<track.GetNumTriggers(); ++i)
        {
            const auto& trigger = track.GetTriggerClass(i);
            const auto& trigger_name = trigger.GetName();
            const auto trigger_type = trigger.GetType();

            const auto& target_node_id = trigger.GetNodeId();
            const auto& timeline_id    = trigger.GetTimelineId();
            const auto timeline_index  = id_to_index_map[timeline_id];
            const auto* target_node    = mState.entity->FindNodeById(target_node_id);
            const auto& target_name    = target_node->GetName();

            TimelineWidget::TimelineItem item;
            item.type      = TimelineWidget::TimelineItem::Type::Point;
            item.text      = app::toString("%1 (%2)", target_name, trigger_name);
            item.id        = trigger.GetId();
            item.starttime = trigger.GetTime();
            item.duration  = 0.0f;
            item.icon      = QPixmap("icons64:animation-trigger.png");
            item.color     = Qt::GlobalColor::transparent;
            if (trigger_type == game::AnimationTriggerClass::Type::EmitParticlesTrigger)
                item.icon = QPixmap("icons64:animation-trigger-particle.png");
            else if (trigger_type == game::AnimationTriggerClass::Type::RunSpriteCycle)
                item.icon = QPixmap("icons64:animation-trigger-sprite-cycle.png");
            else if (trigger_type == game::AnimationTriggerClass::Type::PlayAudio)
            {
                game::AnimationTriggerClass::AudioStreamType stream;
                ASSERT(trigger.GetParameter("audio-stream", &stream));
                if (stream == game::AnimationTriggerClass::AudioStreamType::Effect)
                    item.icon = QPixmap("icons64:animation-trigger-sound-effect.png");
                else if (stream == game::AnimationTriggerClass::AudioStreamType::Music)
                    item.icon = QPixmap("icons64:animation-trigger-music.png");
                else BUG("Unhandled audio trigger stream type.");
            }
            else if (trigger_type == game::AnimationTriggerClass::Type::SpawnEntity)
                item.icon = QPixmap("icons64:animation-trigger-spawn.png");
            else if (trigger_type == game::AnimationTriggerClass::Type::StartMeshEffect)
                item.icon = QPixmap("icons64:animation-trigger-mesh-effect.png");
            else if (trigger_type == game::AnimationTriggerClass::Type::CommitSuicide)
                item.icon = QPixmap("icons64:animation-trigger-suicide.png");
            (*list)[timeline_index].AddItem(item);
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
    PopulateFromEnum<game::SceneProjection>(mUI.cmbSceneProjection);
    PopulateFromEnum<game::TransformAnimatorClass::Interpolation>(mUI.transformInterpolation);
    PopulateFromEnum<game::PropertyAnimatorClass::Interpolation>(mUI.setvalInterpolation);
    PopulateFromEnum<game::PropertyAnimatorClass::PropertyName>(mUI.setvalName);
    PopulateFromEnum<game::KinematicAnimatorClass::Interpolation>(mUI.kinematicInterpolation);
    PopulateFromEnum<game::KinematicAnimatorClass::Target>(mUI.kinematicTarget);
    PopulateFromEnum<game::BooleanPropertyAnimatorClass::PropertyName>(mUI.itemFlags, true, true);
    PopulateFromEnum<game::BooleanPropertyAnimatorClass::PropertyAction>(mUI.flagAction, true, true);
    PopulateFromEnum<game::MaterialAnimatorClass::Interpolation >(mUI.materialInterpolation);
    PopulateFromEnum<game::AnimationTriggerClass::AudioStreamType>(mUI.audioTriggerStream);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.actionUsePhysics, settings.enable_physics);
    SetValue(mUI.zoom, 1.0f);
    SetVisible(mUI.transform, false);
    setFocusPolicy(Qt::StrongFocus);

    mUI.widget->onZoomIn       = [this]() { MouseZoom(std::bind(&AnimationTrackWidget::ZoomIn, this)); };
    mUI.widget->onZoomOut      = [this]() { MouseZoom(std::bind(&AnimationTrackWidget::ZoomOut, this)); };
    mUI.widget->onMouseMove    = std::bind(&AnimationTrackWidget::MouseMove,    this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&AnimationTrackWidget::MousePress,   this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&AnimationTrackWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onPaintScene   = std::bind(&AnimationTrackWidget::PaintScene,   this, std::placeholders::_1, std::placeholders::_2);

    connect(mUI.timeline, &TimelineWidget::SelectedItemChanged, this, &AnimationTrackWidget::SelectedItemChanged);
    connect(mUI.timeline, &TimelineWidget::SelectedItemDragged, this, &AnimationTrackWidget::SelectedItemDragged);
    connect(mUI.timeline, &TimelineWidget::DeleteSelectedItem,  this, &AnimationTrackWidget::DeleteSelectedItem);

    connect(mUI.btnHamburger, &QPushButton::clicked, this, [this]() {
        if (mHamburger == nullptr)
        {
            mHamburger = new QMenu(this);
            mHamburger->addAction(mUI.chkSnap);
            mHamburger->addAction(mUI.chkShowViewport);
            mHamburger->addAction(mUI.chkShowOrigin);
            mHamburger->addAction(mUI.chkShowGrid);
        }
        QPoint point;
        point.setX(0);
        point.setY(mUI.btnHamburger->width());
        mHamburger->popup(mUI.btnHamburger->mapToGlobal(point));
    });

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

    RemoveInvalidAnimationItems();
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
        Timeline timeline;
        timeline.timeline_id    = properties[QString("timeline_%1_self_id").arg(i)].toString();
        timeline.target_node_id = properties[QString("timeline_%1_node_id").arg(i)].toString();
        mState.timelines.push_back(std::move(timeline));
    }

    // we used to store the animator => timeline mapping in properties
    // but it got messy so the decision was made to simplify by adding
    // the timeline ID to the animator class itself. This code is here
    // just to migrate any existing property based mapping to the new
    // way of storing the mapping.
    for (size_t i = 0; i < mState.track->GetNumAnimators(); ++i)
    {
        auto& animator = mState.track->GetAnimatorClass(i);
        const auto& property_key = app::AnyString(animator.GetId());
        if (properties.contains(property_key))
        {
            const auto& property_val = app::AnyString(properties[property_key].toString());
            animator.SetTimelineId(property_val);
        }
    }

    if (properties.contains("main_splitter"))
        mUI.mainSplitter->restoreState(QByteArray::fromBase64(properties["main_splitter"].toString().toLatin1()));
    if (properties.contains("center_splitter"))
        mUI.centerSplitter->restoreState(QByteArray::fromBase64(properties["center_splitter"].toString().toLatin1()));
    if (properties.contains("timeline_splitter"))
        mUI.timelineSplitter->restoreState(QByteArray::fromBase64(properties["timeline_splitter"].toString().toLatin1()));

    RemoveInvalidAnimationItems();
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
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionPreview);
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
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionPreview);
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

    for (unsigned i=0; i<(unsigned)mState.timelines.size(); ++i)
    {
        const auto& timeline = mState.timelines[i];
        settings.SetValue("TrackWidget", QString("timeline_%1_self_id").arg(i), timeline.timeline_id);
        settings.SetValue("TrackWidget", QString("timeline_%1_node_id").arg(i), timeline.target_node_id);
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
    settings.SaveWidget("TrackWidget", mUI.cmbSceneProjection);
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
        QString timeline_id;
        QString target_node_id;
        settings.GetValue("TrackWidget", QString("timeline_%1_self_id").arg(i), &timeline_id);
        settings.GetValue("TrackWidget", QString("timeline_%1_node_id").arg(i), &target_node_id);

        Timeline timeline;
        timeline.timeline_id = timeline_id;
        timeline.target_node_id = target_node_id;
        mState.timelines.push_back(std::move(timeline));
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
    settings.LoadWidget("TrackWidget", mUI.cmbSceneProjection);

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
        auto& animator = mState.track->GetAnimatorClass(i);

        std::string timeline_id;
        if (settings.GetValue("TrackWidget", app::FromUtf8(animator.GetId()), &timeline_id))
            animator.SetTimelineId(timeline_id);
    }
    mUI.timeline->SetDuration(mState.track->GetDuration());
    mUI.timeline->Rebuild();

    RemoveInvalidAnimationItems();

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
    for (auto& event : mEventMessages)
    {
        event.time += secs;
    }
    base::EraseRemove(mEventMessages, [](const auto& event) {
        return event.time >= 2.0f;
    });

    mCurrentTime += secs;

    mAnimator.Update(mUI, mState);

    // Playing back an animation track ?
    if (mPlayState != PlayState::Playing)
    {
        if (mPlayState == PlayState::Stopped)
        {
            mRenderer.SetProjection(GetValue(mUI.cmbSceneProjection));
            mRenderer.UpdateRendererState(*mEntity);
        }
        return;
    }

    ASSERT(mPlayState == PlayState::Playing);

    std::vector<game::Entity::Event> events;
    mPlaybackAnimation->Update(static_cast<float>(secs), &events);

    for (const auto& event : events)
    {
        const auto* animation_event = std::get_if<game::Entity::AnimationEvent>(&event);
        if (!animation_event)
            continue;;

        if (const auto* ptr = std::get_if<game::AnimationAudioTriggerEvent>(&animation_event->event))
        {
            EventMessage msg;
            msg.message = "Audio trigger";
            mEventMessages.push_back(std::move(msg));
        }
        else if (const auto* ptr = std::get_if<game::AnimationSpawnEntityTriggerEvent>(&animation_event->event))
        {
            EventMessage msg;
            msg.message = "Spawn trigger";
            mEventMessages.push_back(std::move(msg));
        }
        else if (const auto* ptr = std::get_if<game::AnimationCommitEntitySuicideEvent>(&animation_event->event))
        {
            EventMessage msg;
            msg.message = "Commit Suicide";
            mEventMessages.push_back(std::move(msg));
        }
    }

    if (mPhysics.HaveWorld())
    {
        mPhysics.UpdateWorld(*mPlaybackAnimation);
        mPhysics.Step();
        mPhysics.UpdateEntity(*mPlaybackAnimation);
    }
    mRenderer.SetProjection(GetValue(mUI.cmbSceneProjection));
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

void AnimationTrackWidget::SetProjection(game::SceneProjection projection)
{
    SetValue(mUI.cmbSceneProjection, projection);
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

    RemoveInvalidAnimationItems();
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

void AnimationTrackWidget::on_widgetColor_colorChanged(const QColor& color)
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

    mEventMessages.clear();
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
    properties["use_physics"]       = (bool)GetValue(mUI.actionUsePhysics);
    properties["num_timelines"]     = (int)mState.timelines.size();
    for (int i=0; i<(int)mState.timelines.size(); ++i)
    {
        const auto& timeline = mState.timelines[i];
        properties[QString("timeline_%1_self_id").arg(i)] = timeline.timeline_id;
        properties[QString("timeline_%1_node_id").arg(i)] = timeline.target_node_id;
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

void AnimationTrackWidget::on_actionPreview_triggered()
{
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
    parent->PreviewAnimation(*mState.track);
}

void AnimationTrackWidget::on_actionDeleteItem_triggered()
{
    if (auto* animator = GetCurrentAnimator())
    {
        mState.track->DeleteAnimatorById(animator->GetId());
    }
    else if (auto* trigger = GetCurrentTrigger())
    {
        mState.track->DeleteTriggerById(trigger->GetId());
    }

    mUI.timeline->ClearSelection();
    mUI.timeline->Rebuild();
    SetActuatorUIDefaults();
    SetActuatorUIEnabled(false);
    ReturnToDefault();

}

void AnimationTrackWidget::on_actionDeleteItems_triggered()
{
    for (size_t i=0; i<mState.entity->GetNumAnimations(); ++i)
    {
        const auto& animation = mState.entity->GetAnimation(i);
        if (animation.GetNumAnimators() || animation.GetNumTriggers())
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Question);
            msg.setWindowTitle("Delete Timeline Items?");
            msg.setText("Are you sure you want to delete all timeline items?");
            msg.setStandardButtons(QMessageBox::StandardButton::Yes |
                                   QMessageBox::StandardButton::No);
            if (msg.exec() == QMessageBox::StandardButton::No)
                return;
        }
    }

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
    const auto timeline_index = mUI.timeline->GetCurrentTimelineIndex();
    ASSERT(timeline_index < mState.timelines.size());
    const auto& timeline = mState.timelines[timeline_index];

    size_t timeline_item_count = 0;
    for (size_t i=0; i< mState.track->GetNumAnimators(); ++i)
    {
        const auto& animator = mState.track->GetAnimatorClass(i);
        const auto& animator_timeline_id = animator.GetTimelineId();
        if (animator_timeline_id == timeline.timeline_id)
            ++timeline_item_count;
    }
    for (size_t i=0; i<mState.track->GetNumTriggers(); ++i)
    {
        const auto& trigger = mState.track->GetTriggerClass(i);
        const auto& trigger_timeline_id = trigger.GetTimelineId();
        if (trigger_timeline_id == timeline.timeline_id)
            ++timeline_item_count;
    }

    if (timeline_item_count > 0)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle("Delete Timeline ?");
        msg.setText("Timeline contains items. Are you sure you want to delete?");
        msg.setStandardButtons(QMessageBox::StandardButton::Yes |
                               QMessageBox::StandardButton::No);
        if (msg.exec() == QMessageBox::StandardButton::No)
            return;
    }

    for (size_t i=0; i< mState.track->GetNumAnimators();)
    {
        const auto& animator = mState.track->GetAnimatorClass(i);
        const auto& animator_timeline_id = animator.GetTimelineId();
        if (animator_timeline_id == timeline.timeline_id)
            mState.track->DeleteAnimator(i);
        else ++i;
    }

    for (size_t i=0; i<mState.track->GetNumTriggers();)
    {
        const auto& trigger = mState.track->GetTriggerClass(i);
        const auto& trigger_timeline_id = trigger.GetTimelineId();
        if (trigger_timeline_id == timeline.timeline_id)
            mState.track->DeleteTrigger(i);
        else ++i;
    }

    mState.timelines.erase(mState.timelines.begin() + timeline_index);
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
    if (auto* item = GetCurrentAnimator())
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
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_actuatorName_textChanged(const QString&)
{
    SetSelectedAnimatorProperties();
    SetSelectedTriggerProperties();
}

void AnimationTrackWidget::on_actuatorStartTime_valueChanged(double value)
{
    if (auto* animator = GetCurrentAnimator())
    {
        const auto duration = mState.track->GetDuration();
        const auto old_start = animator->GetStartTime();
        const auto new_start = value / duration;
        const auto change = old_start - new_start;
        animator->SetStartTime(new_start);
        animator->SetDuration(animator->GetDuration() + change);
        mUI.timeline->Rebuild();
    }
    else if (auto* trigger = GetCurrentTrigger())
    {
        const auto duration = mState.track->GetDuration();
        const auto time_point = value / duration;
        trigger->SetTime(time_point);
        mUI.timeline->Rebuild();
    }
}
void AnimationTrackWidget::on_actuatorEndTime_valueChanged(double value)
{
    if (auto* animator = GetCurrentAnimator())
    {
        const auto duration = mState.track->GetDuration();
        const auto start = animator->GetStartTime();
        const auto end = value / duration;
        animator->SetDuration(end - start);
        mUI.timeline->Rebuild();
    }
}

void AnimationTrackWidget::on_transformInterpolation_currentIndexChanged(int index)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_setvalInterpolation_currentIndexChanged(int index)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_setvalName_currentIndexChanged(int index)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_kinematicInterpolation_currentIndexChanged(int index)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_kinematicTarget_currentIndexChanged(int index)
{
    SetSelectedAnimatorProperties();
    UpdateKinematicUnits();
}

void AnimationTrackWidget::on_timeline_customContextMenuRequested(QPoint)
{
    const auto* selected_timeline_item = mUI.timeline->GetSelectedItem();
    const auto* current_timeline = mUI.timeline->GetCurrentTimeline();
    const auto current_timeline_index = mUI.timeline->GetCurrentTimelineIndex();
    mUI.actionDeleteItem->setEnabled(selected_timeline_item != nullptr);
    mUI.actionDeleteItems->setEnabled(mState.track->GetNumAnimators());
    mUI.actionDeleteTimeline->setEnabled(current_timeline != nullptr);

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
    QMenu animators(this);
    animators.setEnabled(current_timeline != nullptr);
    animators.setTitle("New Animator ...");
    animators.setIcon(QIcon("icons:add.png"));

    struct Animator {
        game::AnimatorClass::Type type;
        QString name;
    };
    std::vector<Animator> animator_type_list;
    animator_type_list.push_back( {game::AnimatorClass::Type::TransformAnimator,       "Transform Animator" });
    animator_type_list.push_back( {game::AnimatorClass::Type::TransformAnimator,       "Transform Animator on Position" });
    animator_type_list.push_back( {game::AnimatorClass::Type::TransformAnimator,       "Transform Animator on Size" });
    animator_type_list.push_back( {game::AnimatorClass::Type::TransformAnimator,       "Transform Animator on Scale" });
    animator_type_list.push_back( {game::AnimatorClass::Type::TransformAnimator,       "Transform Animator on Rotation" });
    animator_type_list.push_back( {game::AnimatorClass::Type::KinematicAnimator,       "Kinematic Animator on Rigid Body" });
    animator_type_list.push_back( {game::AnimatorClass::Type::KinematicAnimator,       "Kinematic Animator on Linear Mover" });
    animator_type_list.push_back( {game::AnimatorClass::Type::MaterialAnimator,        "Material Animator" });
    animator_type_list.push_back( {game::AnimatorClass::Type::PropertyAnimator,        "Property Animator" });
    animator_type_list.push_back( {game::AnimatorClass::Type::BooleanPropertyAnimator, "Property Animator (Boolean))" });

    // build menu for adding actuators.
    for (const auto& animator_type : animator_type_list)
    {
        auto* action = animators.addAction(animator_type.name);
        action->setIcon(QIcon("icons:add.png"));
        action->setProperty("animator-type", static_cast<int>(animator_type.type));
        action->setProperty("timeline-index", static_cast<int>(current_timeline_index));
        action->setEnabled(false);
        if (animator_type.name.contains("Rigid Body"))
            action->setProperty("kinematic_target", static_cast<int>(game::KinematicAnimatorClass::Target::RigidBody));
        else if (animator_type.name.contains("Linear Mover"))
            action->setProperty("kinematic_target", static_cast<int>(game::KinematicAnimatorClass::Target::LinearMover));

        connect(action, &QAction::triggered, this, &AnimationTrackWidget::AddAnimatorAction);
        if (current_timeline)
        {
            const auto widget_coord = mUI.timeline->mapFromGlobal(QCursor::pos());
            const auto seconds = mUI.timeline->MapToSeconds(widget_coord);
            const auto duration = mState.track->GetDuration();
            if (seconds > 0.0f && seconds < duration)
            {
                action->setEnabled(true);
            }
            action->setProperty("time-point", seconds);
        }
    }

    QMenu triggers(this);
    triggers.setEnabled(current_timeline != nullptr);
    triggers.setTitle("New Trigger...");
    triggers.setIcon(QIcon("icons:add.png"));

    struct Trigger {
        game::AnimationTriggerClass::Type type;
        QString name;
    };
    std::vector<Trigger> trigger_type_list;
    trigger_type_list.push_back( { game::AnimationTriggerClass::Type::EmitParticlesTrigger, "Emit Particles" });
    trigger_type_list.push_back( { game::AnimationTriggerClass::Type::RunSpriteCycle, "Run Sprite Cycle" });
    trigger_type_list.push_back( { game::AnimationTriggerClass::Type::PlayAudio, "Play Sound Effect" });
    trigger_type_list.push_back( { game::AnimationTriggerClass::Type::PlayAudio, "Play Music" });
    trigger_type_list.push_back( { game::AnimationTriggerClass::Type::SpawnEntity, "Spawn Entity" } );
    trigger_type_list.push_back( { game::AnimationTriggerClass::Type::StartMeshEffect, "Do Mesh Effect" });
    trigger_type_list.push_back( { game::AnimationTriggerClass::Type::CommitSuicide, "Commit Suicide" });

    for (const auto& trigger_type : trigger_type_list)
    {
        auto* action = triggers.addAction(trigger_type.name);
        action->setIcon(QIcon("icons:add.png"));
        action->setProperty("type", static_cast<int>(trigger_type.type));
        action->setEnabled(false);
        connect(action, &QAction::triggered, this, &AnimationTrackWidget::AddTriggerAction);
        if (current_timeline)
        {
            const auto widget_coord = mUI.timeline->mapFromGlobal(QCursor::pos());
            const auto seconds = mUI.timeline->MapToSeconds(widget_coord);
            action->setEnabled(true);
            action->setProperty("time", seconds);
        }
    }

    menu.addMenu(&animators);
    menu.addMenu(&triggers);
    menu.addSeparator();
    menu.addMenu(&add_timeline);
    menu.addSeparator();
    menu.addAction(mUI.actionDeleteItem);
    menu.addAction(mUI.actionDeleteItems);
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
        SetSelectedAnimatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndPosY_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto pos = node->GetTranslation();
        pos.y = value;
        node->SetTranslation(pos);
        SetSelectedAnimatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndSizeX_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto size = node->GetSize();
        size.x = value;
        node->SetSize(size);
        SetSelectedAnimatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndSizeY_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto size = node->GetSize();
        size.y = value;
        node->SetSize(size);
        SetSelectedAnimatorProperties();
    }
}
void AnimationTrackWidget::on_transformEndScaleX_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto scale = node->GetScale();
        scale.x = value;
        node->SetScale(scale);
        SetSelectedAnimatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndScaleY_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        auto scale = node->GetScale();
        scale.y = value;
        node->SetScale(scale);
        SetSelectedAnimatorProperties();
    }
}

void AnimationTrackWidget::on_transformEndRotation_valueChanged(double value)
{
    if (auto* node = GetCurrentEntityNode())
    {
        node->SetRotation(qDegreesToRadians(value));
        SetSelectedAnimatorProperties();
    }
}

void AnimationTrackWidget::on_translate_stateChanged(int)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_resize_stateChanged(int)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_rotate_stateChanged(int)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_scale_stateChanged(int)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_setvalEndValue_ValueChanged()
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_setvalJoint_currentIndexChanged(int index)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_actuatorIsStatic_toggled(bool on)
{
    SetSelectedAnimatorProperties();
    SetSelectedTriggerProperties();
}

void AnimationTrackWidget::on_kinematicEndVeloX_valueChanged(double value)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_kinematicEndVeloY_valueChanged(double value)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_kinematicEndVeloZ_valueChanged(double value)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_kinematicEndAccelX_valueChanged(double value)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_kinematicEndAccelY_valueChanged(double value)
{
    SetSelectedAnimatorProperties();
}
void AnimationTrackWidget::on_kinematicEndAccelZ_valueChanged(double value)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_itemFlags_currentIndexChanged(int)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_flagAction_currentIndexChanged(int)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_flagTime_valueChanged(double)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_flagJoint_currentIndexChanged(int)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_materialInterpolation_currentIndexChanged(int)
{
    SetSelectedAnimatorProperties();
}

void AnimationTrackWidget::on_btnMaterialParameters_clicked()
{
    auto* node = GetCurrentEntityNode();
    auto* klass = mState.entity->FindNodeById(node->GetClassId());
    auto* drawable = klass ? klass->GetDrawable() : nullptr;
    auto* animator = dynamic_cast<game::MaterialAnimatorClass*>(GetCurrentAnimator());
    if (!node || !drawable || !animator)
        return;

    const auto& material = mWorkspace->GetMaterialClassById(node->GetDrawable()->GetMaterialId());
    DlgMaterialParams dlg(this, drawable, animator);
    dlg.AdaptInterface(mWorkspace, material.get());
    dlg.exec();
}
void AnimationTrackWidget::on_emitCount_valueChanged(int)
{
    SetSelectedTriggerProperties();
}

void AnimationTrackWidget::on_spriteCycles_currentIndexChanged(int)
{
    SetSelectedTriggerProperties();
}

void AnimationTrackWidget::on_audioTriggerGraph_currentIndexChanged(int)
{
    SetSelectedTriggerProperties();
}
void AnimationTrackWidget::on_audioTriggerStream_currentIndexChanged(int)
{
    SetSelectedTriggerProperties();
}

void AnimationTrackWidget::on_entityTriggerList_currentIndexChanged(int)
{
    SetSelectedTriggerProperties();
}

void AnimationTrackWidget::on_entityRenderLayer_valueChanged(int)
{
    SetSelectedTriggerProperties();
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
    mUI.actuatorProperties->setCurrentWidget(mUI.blankPage);
}

void AnimationTrackWidget::SetActuatorUIDefaults()
{
    SetValue(mUI.actuatorGroup, QString("Item properties (Nothing selected)"));
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
    SetValue(mUI.emitCount, 0); // indicates default particle emission
    SetValue(mUI.spriteCycles, -1);
    SetValue(mUI.entityTriggerList, -1);
    mUI.actuatorProperties->setCurrentWidget(mUI.blankPage);

    mUI.curve->ClearFunction();

    if (!mState.track)
        return;

    const auto duration = mState.track->GetDuration();
    SetMinMax(mUI.actuatorStartTime, 0.0, duration);
    SetMinMax(mUI.actuatorEndTime,   0.0, duration);
}

void AnimationTrackWidget::SetSelectedTriggerProperties()
{
    if (mPlayState != PlayState::Stopped)
        return;

    auto* trigger = GetCurrentTrigger();
    if (!trigger)
        return;

    trigger->SetName(GetValue(mUI.actuatorName));
    if (trigger->GetType() == game::AnimationTriggerClass::Type::EmitParticlesTrigger)
        trigger->SetParameter("particle-emit-count", (int)GetValue(mUI.emitCount));
    else if (trigger->GetType() == game::AnimationTriggerClass::Type::RunSpriteCycle)
        trigger->SetParameter("sprite-cycle-id", GetItemId(mUI.spriteCycles));
    else if (trigger->GetType() == game::AnimationTriggerClass::Type::PlayAudio)
    {
        const game::AnimationTriggerClass::AudioStreamType stream = GetValue(mUI.audioTriggerStream);
        const game::AnimationTriggerClass::AudioStreamAction action = game::AnimationTriggerClass::AudioStreamAction::Play;
        trigger->SetParameter("audio-stream-action", action);
        trigger->SetParameter("audio-stream", stream);
        trigger->SetParameter("audio-graph-id", GetItemId(mUI.audioTriggerGraph));
    }
    else if (trigger->GetType() == game::AnimationTriggerClass::Type::SpawnEntity)
    {
        const int render_layer = GetValue(mUI.entityRenderLayer);
        trigger->SetParameter("entity-class-id", GetItemId(mUI.entityTriggerList));
        trigger->SetParameter("entity-render-layer", render_layer);
    }
    else if (trigger->GetType() == game::AnimationTriggerClass::Type::StartMeshEffect)
    {

    }
    else if (trigger->GetType() == game::AnimationTriggerClass::Type::CommitSuicide)
    {

    }
    else BUG("Unhandled animation trigger type.");

    mUI.timeline->Rebuild();
}

void AnimationTrackWidget::SetSelectedAnimatorProperties()
{
    if (mPlayState != PlayState::Stopped)
        return;

    auto* animator = GetCurrentAnimator();
    if (!animator)
        return;

    animator->SetName(GetValue(mUI.actuatorName));
    animator->SetFlag(game::AnimatorClass::Flags::StaticInstance, GetValue(mUI.actuatorIsStatic));

    if (auto* transform = dynamic_cast<game::TransformAnimatorClass*>(animator))
    {
        transform->SetInterpolation(GetValue(mUI.transformInterpolation));
        transform->SetEndPosition(GetValue(mUI.transformEndPosX), GetValue(mUI.transformEndPosY));
        transform->SetEndSize(GetValue(mUI.transformEndSizeX), GetValue(mUI.transformEndSizeY));
        transform->SetEndScale(GetValue(mUI.transformEndScaleX), GetValue(mUI.transformEndScaleY));
        transform->SetEndRotation(qDegreesToRadians((float) GetValue(mUI.transformEndRotation)));
        transform->EnableResize(GetValue(mUI.resize));
        transform->EnableRotation(GetValue(mUI.rotate));
        transform->EnableTranslation(GetValue(mUI.translate));
        transform->EnableScaling(GetValue(mUI.scale));

        mUI.curve->SetFunction(transform->GetInterpolation());
    }
    else if (auto* setter = dynamic_cast<game::PropertyAnimatorClass*>(animator))
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
        else if (name == Name::LinearMover_LinearVelocity)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Vec2, " u/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsVec2());
        }
        else if (name == Name::LinearMover_LinearVelocityX)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " u/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::LinearMover_LinearVelocityY)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " u/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::LinearMover_LinearAcceleration)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Vec2, " u/s²");
            setter->SetEndValue(mUI.setvalEndValue->GetAsVec2());
        }
        else if (name == Name::LinearMover_LinearAccelerationX)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " u/s²");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::LinearMover_LinearAccelerationY)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " u/s²");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::LinearMover_AngularVelocity)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float, " rad/s");
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::LinearMover_AngularAcceleration)
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
        else if (name == Name::SplineMover_LinearAcceleration)
        {
            mUI.setvalEndValue->SetType(Uniform::Type::Float);
            setter->SetEndValue(mUI.setvalEndValue->GetAsFloat());
        }
        else if (name == Name::SplineMover_LinearSpeed)
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
    else if (auto* kinematic = dynamic_cast<game::KinematicAnimatorClass*>(animator))
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
    else if (auto* setflag = dynamic_cast<game::BooleanPropertyAnimatorClass*>(animator))
    {
        setflag->SetFlagAction(GetValue(mUI.flagAction));
        setflag->SetFlagName(GetValue(mUI.itemFlags));
        setflag->SetTime(GetValue(mUI.flagTime));
        setflag->SetJointId(GetItemId(mUI.flagJoint));

        SetEnabled(mUI.flagJoint, setflag->RequiresJoint());

        mUI.curve->ClearFunction();
    }
    else if (auto* material = dynamic_cast<game::MaterialAnimatorClass*>(animator))
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
        SetSelectedAnimatorProperties();
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
    }
    else if (item->type == TimelineWidget::TimelineItem::Type::Span)
        TimelineAnimatorChanged(item);
    else if (item->type == TimelineWidget::TimelineItem::Type::Point)
        TimelineTriggerChanged(item);
}

void AnimationTrackWidget::TimelineAnimatorChanged(const TimelineWidget::TimelineItem* item)
{
    SetActuatorUIEnabled(true);

    const auto* selected_animator = mState.track->FindAnimatorById(item->id);
    const auto duration = mState.track->GetDuration();
    const auto start = selected_animator->GetStartTime() * duration;
    const auto end = selected_animator->GetDuration() * duration + start;
    const auto node = mEntity->FindNodeByClassId(selected_animator->GetNodeId());

    // figure out the hi/lo (left right) limits for the spinbox start
    // and time values for this actuator.
    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i = 0; i < mState.track->GetNumAnimators(); ++i)
    {
        const auto& other_animator = mState.track->GetAnimatorClass(i);
        if (other_animator.GetId() == selected_animator->GetId())
            continue;
        else if (other_animator.GetNodeId() != selected_animator->GetNodeId())
            continue;

        // if the animators are not on the same timeline then discard
        // from this check
        if (other_animator.GetTimelineId() != selected_animator->GetTimelineId())
            continue;

        const auto start = other_animator.GetStartTime();
        const auto end = other_animator.GetStartTime() + other_animator.GetDuration();
        if (start >= selected_animator->GetStartTime())
            hi_bound = std::min(hi_bound, start);
        if (end <= selected_animator->GetStartTime())
            lo_bound = std::max(lo_bound, end);
    }
    const auto& len = QString::number(end - start, 'f', 2);
    SetValue(mUI.actuatorName, selected_animator->GetName());
    SetValue(mUI.actuatorID, selected_animator->GetId());
    SetValue(mUI.actuatorNode, node->GetName());
    SetValue(mUI.actuatorType, app::toString(selected_animator->GetType()));
    SetValue(mUI.actuatorIsStatic, selected_animator->TestFlag(game::AnimatorClass::Flags::StaticInstance));
    SetMinMax(mUI.actuatorStartTime, lo_bound * duration, hi_bound * duration);
    SetMinMax(mUI.actuatorEndTime, lo_bound * duration, hi_bound * duration);
    SetValue(mUI.actuatorStartTime, start);
    SetValue(mUI.actuatorEndTime, end);
    SetValue(mUI.actuatorGroup, selected_animator->GetName());
    SetEnabled(mUI.actuatorIsStatic, false);
    SetEnabled(mUI.actuatorEndTime, true);

    if (const auto* ptr = dynamic_cast<const game::TransformAnimatorClass*>(selected_animator))
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
        SetValue(mUI.actuatorIsStatic, ptr->TestFlag(game::TransformAnimatorClass::Flags::StaticInstance));
        SetValue(mUI.translate, ptr->IsTranslationEnabled());
        SetValue(mUI.resize, ptr->IsResizeEnabled());
        SetValue(mUI.scale, ptr->IsScalingEnabled());
        SetValue(mUI.rotate, ptr->IsRotationEnable());
        SetEnabled(mUI.actuatorIsStatic, true);

        SetEnabled(mUI.transformEndPosX, ptr->IsTranslationEnabled());
        SetEnabled(mUI.transformEndPosY, ptr->IsTranslationEnabled());
        SetEnabled(mUI.transformEndSizeX, ptr->IsResizeEnabled());
        SetEnabled(mUI.transformEndSizeY, ptr->IsResizeEnabled());
        SetEnabled(mUI.transformEndScaleY, ptr->IsScalingEnabled());
        SetEnabled(mUI.transformEndScaleY, ptr->IsScalingEnabled());
        SetEnabled(mUI.transformEndRotation, ptr->IsRotationEnable());

        mUI.actuatorProperties->setCurrentWidget(mUI.transformActuator);
        mUI.curve->SetFunction(ptr->GetInterpolation());

        node->SetTranslation(pos);
        node->SetSize(size);
        node->SetScale(scale);
        node->SetRotation(rotation);
    }
    else if (const auto* ptr = dynamic_cast<const game::PropertyAnimatorClass*>(selected_animator))
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
        else if (name == Name::LinearMover_LinearVelocity)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<glm::vec2>(), " u/s");
        else if (name == Name::LinearMover_LinearVelocityX)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " u/s");
        else if (name == Name::LinearMover_LinearVelocityY)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " u/s");
        else if (name == Name::LinearMover_LinearAcceleration)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<glm::vec2>(), " u/s²");
        else if (name == Name::LinearMover_LinearAccelerationX)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " u/s²");
        else if (name == Name::LinearMover_LinearAccelerationY)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " u/s²");
        else if (name == Name::LinearMover_AngularVelocity)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), " rad/s");
        else if (name == Name::LinearMover_AngularAcceleration)
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
        else if (name == Name::SplineMover_LinearAcceleration)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), "u/s²");
        else if (name == Name::SplineMover_LinearSpeed)
            SetValue(mUI.setvalEndValue, *ptr->GetEndValue<float>(), "u/s");
        else BUG("Unhandled set value actuator value type.");

        SetValue(mUI.setvalInterpolation, ptr->GetInterpolation());
        SetValue(mUI.setvalName, ptr->GetPropertyName());
        SetValue(mUI.setvalJoint, ListItemId(ptr->GetJointId()));
        SetEnabled(mUI.setvalJoint, ptr->RequiresJoint());

        mUI.actuatorProperties->setCurrentWidget(mUI.setvalActuator);

        mUI.curve->SetFunction(ptr->GetInterpolation());
    }
    else if (const auto* ptr = dynamic_cast<const game::KinematicAnimatorClass*>(selected_animator))
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
    else if (const auto* ptr = dynamic_cast<const game::BooleanPropertyAnimatorClass*>(selected_animator))
    {
        SetValue(mUI.itemFlags, ptr->GetFlagName());
        SetValue(mUI.flagAction, ptr->GetFlagAction());
        SetValue(mUI.flagTime, ptr->GetTime());
        SetValue(mUI.flagJoint, ListItemId(ptr->GetJointId()));
        SetEnabled(mUI.flagJoint, ptr->RequiresJoint());

        mUI.actuatorProperties->setCurrentWidget(mUI.setflagActuator);

        mUI.curve->ClearFunction();
    }
    else if (const auto* ptr = dynamic_cast<const game::MaterialAnimatorClass*>(selected_animator))
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

void AnimationTrackWidget::TimelineTriggerChanged(const TimelineWidget::TimelineItem* item)
{
    SetActuatorUIEnabled(true);

    const auto* selected_trigger = mState.track->FindTriggerById(item->id);
    const auto* target_node = mEntity->FindNodeByClassId(selected_trigger->GetNodeId());
    const auto duration = mState.track->GetDuration();
    const auto time_point = selected_trigger->GetTime() * duration;

    SetValue(mUI.actuatorName, selected_trigger->GetName());
    SetValue(mUI.actuatorID, selected_trigger->GetId());
    SetValue(mUI.actuatorNode, target_node->GetName());
    SetValue(mUI.actuatorType, app::toString(selected_trigger->GetType()));
    SetValue(mUI.actuatorIsStatic, true);
    SetEnabled(mUI.actuatorIsStatic, false);
    SetMinMax(mUI.actuatorStartTime, 0.0f * duration, 1.0f * duration);
    SetMinMax(mUI.actuatorEndTime, 0.0f * duration, 1.0f * duration);
    SetValue(mUI.actuatorStartTime, time_point);
    SetValue(mUI.actuatorEndTime, time_point);
    SetEnabled(mUI.actuatorEndTime, false);
    SetValue(mUI.actuatorGroup, selected_trigger->GetName());

    if (selected_trigger->GetType() == game::AnimationTriggerClass::Type::EmitParticlesTrigger)
    {
        mUI.actuatorProperties->setCurrentWidget(mUI.emitParticlesTrigger);
        SetValue(mUI.emitCount, *selected_trigger->GetParameter<int>("particle-emit-count"));
    }
    else if (selected_trigger->GetType() == game::AnimationTriggerClass::Type::RunSpriteCycle)
    {
        std::vector<ListItem> sprite_cycles;
        if (const auto* drawable = target_node->GetDrawable())
        {
            const auto& materialId = drawable->GetMaterialId();
            const auto& material_class = mWorkspace->FindMaterialClassById(materialId);
            if (material_class && material_class->IsSprite())
            {
                for (unsigned i=0; i<material_class->GetNumTextureMaps(); ++i)
                {
                    const auto* map = material_class->GetTextureMap(i);
                    if (map->IsSpriteMap())
                    {
                        ListItem item;
                        item.id = map->GetId();
                        item.name = map->GetName();
                        sprite_cycles.push_back(std::move(item));
                    }
                }
            }
        }

        SetList(mUI.spriteCycles, sprite_cycles);

        mUI.actuatorProperties->setCurrentWidget(mUI.runSpriteCycleTrigger);
        if (const auto* cycle_id = selected_trigger->GetParameter<std::string>("sprite-cycle-id"))
        {
            SetValue(mUI.spriteCycles, ListItemId { *cycle_id });
        }
    }
    else if (selected_trigger->GetType() == game::AnimationTriggerClass::Type::PlayAudio)
    {
        mUI.actuatorProperties->setCurrentWidget(mUI.playAudioTrigger);

        auto action = game::AnimationTriggerClass::AudioStreamAction::Play;
        auto stream = game::AnimationTriggerClass::AudioStreamType::Effect;
        std::string audio_graph_id;
        selected_trigger->GetParameter("audio-stream-action", &action);
        selected_trigger->GetParameter("audio-stream", &stream);
        selected_trigger->GetParameter("audio-graph-id", &audio_graph_id);

        SetList(mUI.audioTriggerGraph, mWorkspace->ListAudioGraphs());
        SetValue(mUI.audioTriggerGraph, ListItemId { audio_graph_id });
        SetValue(mUI.audioTriggerStream, stream);

    }
    else if (selected_trigger->GetType() == game::AnimationTriggerClass::Type::SpawnEntity)
    {
        mUI.actuatorProperties->setCurrentWidget(mUI.spawnEntityTrigger);

        int render_layer = 0;
        std::string entity_class_id;
        selected_trigger->GetParameter("entity-class-id", &entity_class_id);
        selected_trigger->GetParameter("entity-render-layer", &render_layer);

        SetList(mUI.entityTriggerList, mWorkspace->ListUserDefinedEntities());
        SetValue(mUI.entityTriggerList, ListItemId { entity_class_id });
        SetValue(mUI.entityRenderLayer, render_layer);
    }
    else if (selected_trigger->GetType() == game::AnimationTriggerClass::Type::StartMeshEffect)
    {

    }
    else if (selected_trigger->GetType() == game::AnimationTriggerClass::Type::CommitSuicide)
    {

    }
    else BUG("Unhandled audio trigger type.");
}
void AnimationTrackWidget::SelectedItemDragged(const TimelineWidget::TimelineItem* item)
{
    if (item->type == TimelineWidget::TimelineItem::Type::Span)
    {
        auto* animator = mState.track->FindAnimatorById(item->id);
        animator->SetStartTime(item->starttime);
        animator->SetDuration(item->duration);

        const auto duration = mState.track->GetDuration();
        const auto start = animator->GetStartTime() * duration;
        const auto end = animator->GetDuration() * duration + start;
        const auto len = QString::number(end - start, 'f', 2);
        SetValue(mUI.actuatorStartTime, start);
        SetValue(mUI.actuatorEndTime, end);
        SetValue(mUI.actuatorGroup, animator->GetName());
    }
    else if (item->type == TimelineWidget::TimelineItem::Type::Point)
    {
        auto* trigger = mState.track->FindTriggerById(item->id);
        trigger->SetTime(item->starttime);

        const auto duration = mState.track->GetDuration();
        const auto time_point = item->starttime * duration;
        SetValue(mUI.actuatorStartTime, time_point);
        SetValue(mUI.actuatorEndTime, time_point);
    }
}

void AnimationTrackWidget::DeleteSelectedItem(const TimelineWidget::TimelineItem* item)
{
    on_actionDeleteItem_triggered();
}

void AnimationTrackWidget::ToggleShowResource()
{
    auto* action = qobject_cast<QAction*>(sender());
    const auto payload = action->data().toInt();
    const auto type = magic_enum::enum_cast<game::AnimatorClass::Type>(payload);
    ASSERT(type.has_value());
    mState.show_flags.set(type.value(), action->isChecked());
    mUI.timeline->Rebuild();
}

void AnimationTrackWidget::AddAnimatorAction()
{
    // Extract the data for adding a new actuator from the action
    // that is created and when the timeline custom context menu is opened.
    auto* action = qobject_cast<QAction*>(sender());
    // the seconds (seconds into the duration of the animation)
    // is set when the context menu with this QAction is opened.
    const auto time_point = action->property("time-point").toFloat();
    // the name of the action carries the type
    const auto type = static_cast<game::AnimatorClass::Type>(action->property("animator-type").toInt());

    const auto timeline_index = action->property("timeline-index").toUInt();
    ASSERT(timeline_index >= 0);
    ASSERT(timeline_index <= mState.timelines.size());

    if (!AddAnimatorFromTimeline(type, time_point, timeline_index))
        return;

    const auto& action_text = action->text();

    // hack for now
    if (type == game::AnimatorClass::Type::KinematicAnimator)
    {
        const auto target = static_cast<game::KinematicAnimatorClass::Target>(action->property("kinematic_target").toInt());

        if (auto* selected = GetCurrentAnimator())
        {
            auto* ptr = dynamic_cast<game::KinematicAnimatorClass*>(selected);
            ptr->SetTarget(target);
            SetValue(mUI.kinematicTarget, target);

            SelectedItemChanged(mUI.timeline->SelectItem(selected->GetId()));
        }
    }
    else if (type == game::AnimatorClass::Type::TransformAnimator)
    {
        if (auto* selected = GetCurrentAnimator())
        {
            auto* transform = dynamic_cast<game::TransformAnimatorClass*>(selected);
            if (action_text.contains("on Position"))
            {
                transform->ClearTransformBits();
                transform->EnableTranslation(true);
            }
            else if (action_text.contains("on Size"))
            {
                transform->ClearTransformBits();
                transform->EnableResize(true);
            }
            else if (action_text.contains("on Scale"))
            {
                transform->ClearTransformBits();
                transform->EnableScaling(true);
            }
            else if (action_text.contains("on Rotation"))
            {
                transform->ClearTransformBits();
                transform->EnableRotation(true);
            }

            SelectedItemChanged(mUI.timeline->SelectItem(selected->GetId()));
        }
    }
}

void AnimationTrackWidget::AddTriggerAction()
{
    auto* action = qobject_cast<QAction*>(sender());
    const auto type = static_cast<game::AnimationTriggerClass::Type>(action->property("type").toInt());
    const auto time = action->property("time").toFloat();
    const auto duration = mState.track->GetDuration();
    const auto time_point = time / duration;

    const auto timeline_index = mUI.timeline->GetCurrentTimelineIndex();
    if (timeline_index >= mState.timelines.size())
        return;

    const auto& timeline = mState.timelines[timeline_index];
    const auto* target_node = mState.entity->FindNodeById(timeline.target_node_id);

    const auto& action_text = action->text();

    game::AnimationTriggerClass trigger(type);
    trigger.SetName(base::FormatString("Trigger_%1", mState.track->GetNumTriggers()));
    trigger.SetTime(time_point);
    trigger.SetNodeId(timeline.target_node_id);
    trigger.SetTimelineId(timeline.timeline_id);

    // add default parameters here.
    if (type == game::AnimationTriggerClass::Type::EmitParticlesTrigger)
    {
        trigger.SetParameter("particle-emit-count", 0); // default emission.
    }
    else if (type == game::AnimationTriggerClass::Type::RunSpriteCycle)
    {
        std::vector<ListItem> sprite_cycles;

        const auto* target_node = mState.entity->FindNodeById(timeline.target_node_id);
        if (const auto* draw = target_node->GetDrawable())
        {
            const auto& materialId = draw->GetMaterialId();
            const auto& material_class = mWorkspace->FindMaterialClassById(materialId);
            if (material_class && material_class->IsSprite())
            {
                for (unsigned i=0; i<material_class->GetNumTextureMaps(); ++i)
                {
                    const auto* map = material_class->GetTextureMap(i);
                    if (map->IsSpriteMap())
                    {
                        ListItem item;
                        item.id = map->GetId();
                        item.name = map->GetName();
                        sprite_cycles.push_back(std::move(item));
                    }
                }
            }
        }
        SetList(mUI.spriteCycles, sprite_cycles);
        if (!sprite_cycles.empty())
        {
            SetValue(mUI.spriteCycles, ListItemId { sprite_cycles[0].id });
            trigger.SetParameter("sprite-cycle-id", sprite_cycles[0].id);
        }
        trigger.SetParameter("sprite-cycle-delay", 0.0f);
    }
    else if (type == game::AnimationTriggerClass::Type::PlayAudio)
    {
        const auto& list = mWorkspace->ListAudioGraphs();
        SetList(mUI.audioTriggerGraph, list);
        if (!list.empty())
        {
            trigger.SetParameter("audio-graph-id", list[0].id);
        }
        trigger.SetParameter("audio-stream-action", game::AnimationTriggerClass::AudioStreamAction::Play);
        trigger.SetParameter("audio-stream", game::AnimationTriggerClass::AudioStreamType::Effect);

        if (action_text.contains("Music"))
            trigger.SetParameter("audio-stream", game::AnimationTriggerClass::AudioStreamType::Music);

    }
    else if (type == game::AnimationTriggerClass::Type::SpawnEntity)
    {
        const auto& list = mWorkspace->ListUserDefinedEntities();
        SetList(mUI.entityTriggerList, list);
        if (!list.empty())
        {
            trigger.SetParameter("entity-class-id", list[0].id);
        }
        trigger.SetParameter("entity-render-layer", 0);
    }
    else if (type == game::AnimationTriggerClass::Type::StartMeshEffect)
    {
    }
    else if (type == game::AnimationTriggerClass::Type::CommitSuicide)
    {
    }
    else BUG("Unhandled animation trigger type.");

    mState.track->AddTrigger(trigger);
    mUI.timeline->Rebuild();
    SelectedItemChanged(mUI.timeline->SelectItem(trigger.GetId()));

    DEBUG("New timeline trigger on target node. [type=%1, node=%2, time=%3s] ", type,
          target_node->GetName(), time);
}

void AnimationTrackWidget::AddNodeTimelineAction()
{
    const auto* action = qobject_cast<QAction*>(sender());
    const auto& nodeId = action->data().toString();

    size_t insert_index = 0;
    if (!mUI.timeline->GetCurrentTimeline())
    {
        if (!mState.timelines.empty())
            insert_index = mState.timelines.size();
    }
    else
    {
        insert_index = mUI.timeline->GetCurrentTimelineIndex();
    }

    Timeline timeline;
    timeline.timeline_id    = base::RandomString(10);
    timeline.target_node_id = nodeId;
    mState.timelines.insert(mState.timelines.begin() + insert_index, timeline);
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
    mRenderer.SetProjection(GetValue(mUI.cmbSceneProjection));

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

    if (mPlayState == PlayState::Playing)
    {
        gfx::FRect rect(10.0f, 10.0f, 500.0f, 20.0f);
        for (const auto& print: mEventMessages)
        {
            gfx::FillRect(painter, rect, gfx::Color4f(gfx::Color::Black, 0.4f));
            gfx::DrawTextRect(painter, print.message, "app://fonts/orbitron-medium.otf", 14, rect,
                              gfx::Color::HotPink, gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
            rect.Translate(0.0f, 20.0f);
        }
    }
    else
    {
        PrintMousePos(mUI, mState, painter,
                      engine::GameView::AxisAligned,
                      engine::Projection::Orthographic);
    }
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
        SetSelectedAnimatorProperties();
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

bool AnimationTrackWidget::AddAnimatorFromTimeline(game::AnimatorClass::Type type, float seconds, unsigned timeline_index)
{
    ASSERT(timeline_index < mState.timelines.size());

    const auto& timeline = mState.timelines[timeline_index];
    const auto* node     = mState.entity->FindNodeById(timeline.target_node_id);
    const auto duration  = mState.track->GetDuration();
    const auto position  = seconds / duration;

    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i=0; i< mState.track->GetNumAnimators(); ++i)
    {
        const auto& animator = mState.track->GetAnimatorClass(i);
        if (animator.GetTimelineId() != timeline.timeline_id)
            continue;

        const auto start = animator.GetStartTime();
        const auto end   = animator.GetStartTime() + animator.GetDuration();
        // don't let overlapping animators to be added.
        if (position >= start && position <= end)
            return false;

        if (start >= position)
            hi_bound = std::min(hi_bound, start);
        if (end <= position)
            lo_bound = std::max(lo_bound, end);
    }
    if (hi_bound <= lo_bound || hi_bound - lo_bound < 0.01)
    {
        NOTE("No animator time slot available.");
        return false;
    }

    const auto& name = base::FormatString("Animator_%1", mState.track->GetNumAnimators());
    const auto node_duration   = hi_bound - lo_bound;
    const auto node_start_time = lo_bound;
    const float node_end_time  = node_start_time + node_duration;

    if (type == game::AnimatorClass::Type::TransformAnimator)
    {
        game::TransformAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(timeline.target_node_id);
        klass.SetTimelineId(timeline.timeline_id);
        klass.SetFlag(game::AnimatorClass::Flags::StaticInstance, true);
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        klass.SetEndPosition(node->GetTranslation());
        klass.SetEndSize(node->GetSize());
        klass.SetEndScale(node->GetScale());
        klass.SetEndRotation(node->GetRotation());
        klass.SetInterpolation(game::TransformAnimatorClass::Interpolation::Linear);
        mState.track->AddAnimator(klass);

        mUI.timeline->Rebuild();
        SelectedItemChanged(mUI.timeline->SelectItem(klass.GetId()));
    }
    else if (type == game::AnimatorClass::Type::PropertyAnimator)
    {
        game::PropertyAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(timeline.target_node_id);
        klass.SetTimelineId(timeline.timeline_id);
        klass.SetFlag(game::AnimatorClass::Flags::StaticInstance, true);
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        klass.SetPropertyName(game::PropertyAnimatorClass::PropertyName::Drawable_TimeScale);
        klass.SetInterpolation(game::PropertyAnimatorClass::Interpolation::Linear);
        klass.SetJointId("");
        mState.track->AddAnimator(klass);

        mUI.timeline->Rebuild();
        SelectedItemChanged(mUI.timeline->SelectItem(klass.GetId()));
    }
    else if (type == game::AnimatorClass::Type::KinematicAnimator)
    {
        game::KinematicAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(timeline.target_node_id);
        klass.SetTimelineId(timeline.timeline_id);
        klass.SetFlag(game::AnimatorClass::Flags::StaticInstance, true);
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        klass.SetEndAngularVelocity(0.0f);
        mState.track->AddAnimator(klass);

        mUI.timeline->Rebuild();
        SelectedItemChanged(mUI.timeline->SelectItem(klass.GetId()));
    }
    else if (type == game::AnimatorClass::Type::BooleanPropertyAnimator)
    {
        game::BooleanPropertyAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(timeline.target_node_id);
        klass.SetTimelineId(timeline.timeline_id);
        klass.SetFlag(game::AnimatorClass::Flags::StaticInstance, true);
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        klass.SetFlagName(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_VisibleInGame);
        klass.SetFlagAction(game::BooleanPropertyAnimatorClass::PropertyAction::On);
        klass.SetJointId("");
        klass.SetTime((position - lo_bound) / node_duration);
        mState.track->AddAnimator(klass);

        mUI.timeline->Rebuild();
        SelectedItemChanged(mUI.timeline->SelectItem(klass.GetId()));
    }
    else if (type ==game::AnimatorClass::Type::MaterialAnimator)
    {
        game::MaterialAnimatorClass klass;
        klass.SetName(name);
        klass.SetNodeId(node->GetId());
        klass.SetTimelineId(timeline.timeline_id);
        klass.SetStartTime(node_start_time);
        klass.SetDuration(node_duration);
        mState.track->AddAnimator(klass);

        mUI.timeline->Rebuild();
        SelectedItemChanged(mUI.timeline->SelectItem(klass.GetId()));
    }
    DEBUG("New timeline animator on target node. [type=%1, node=%2, start=%3s, end=%4s]", type, node->GetName(),
          node_start_time * duration, node_end_time * duration);
    return true;
}

void AnimationTrackWidget::CreateTimelines()
{
    // create timelines for nodes that don't have a timeline yet.
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& id = mState.entity->GetNode(i).GetId();
        auto it = std::find_if(mState.timelines.begin(),
                               mState.timelines.end(), [&id](const auto& timeline) {
            return timeline.target_node_id == id;
        });
        if (it != mState.timelines.end())
            continue;

        Timeline timeline;
        timeline.timeline_id = base::RandomString(10);
        timeline.target_node_id = id;
        mState.timelines.push_back(std::move(timeline));
    }
}

void AnimationTrackWidget::RemoveInvalidAnimationItems()
{
    // delete animators that refer to a node/joint that no longer exists
    std::vector<std::string> dead_animators;
    std::vector<std::string> dead_triggers;

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

    for (size_t i=0; i<mState.track->GetNumTriggers(); ++i)
    {
        const auto& trigger = mState.track->GetTriggerClass(i);
        const auto& target_node = trigger.GetNodeId();
        if (!mState.entity->FindNodeById(target_node))
        {
            dead_triggers.push_back(trigger.GetId());
            DEBUG("Deleting trigger because the entity node is no longer available. [trigger='%1']", trigger.GetName());
        }
    }

    for (const auto& id : dead_animators)
    {
        mState.track->DeleteAnimatorById(id);
    }
    for (const auto& id : dead_triggers)
    {
        mState.track->DeleteTriggerById(id);
    }

    // remove orphaned timelines.
    for (auto it = mState.timelines.begin();  it != mState.timelines.end();)
    {
        const auto& timeline = *it;
        if (mState.entity->FindNodeById(timeline.target_node_id))
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
    else if (target == game::KinematicAnimatorClass::Target::LinearMover)
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
    if (const auto* animator = GetCurrentAnimator())
    {
        return mEntity->FindNodeByClassId(animator->GetNodeId());
    }
    return nullptr;
}

game::AnimatorClass* AnimationTrackWidget::GetCurrentAnimator()
{
    if (auto* item = mUI.timeline->GetSelectedItem())
    {
        if (item->type == TimelineWidget::TimelineItem::Type::Span)
            return mState.track->FindAnimatorById(item->id);
    }
    return nullptr;
}

game::AnimationTriggerClass* AnimationTrackWidget::GetCurrentTrigger()
{
    if (auto* item = mUI.timeline->GetSelectedItem())
    {
        if (item->type == TimelineWidget::TimelineItem::Type::Point)
            return mState.track->FindTriggerById(item->id);
    }
    return nullptr;
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
