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

#define LOGTAG "gui"

#include "warnpush.h"
#  include <QMessageBox>
#  include <QVector2D>
#  include <QMenu>
#  include <QMessageBox>
#  include <base64/base64.h>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include <unordered_map>

#include "base/math.h"
#include "base/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/tool.h"
#include "gamelib/animation.h"
#include "graphics/transform.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/painter.h"

namespace {
    // shared entity class objects.
    // shared between entity widget and this track widget
    // whenever an editor session is restored.
    std::unordered_map<size_t,
            std::weak_ptr<game::EntityClass>> SharedAnimations;
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
        for (size_t i=0; i<anim.GetNumNodes(); ++i)
        {
            const auto& node = anim.GetNode(i);
            const auto& name = node.GetName();
            const auto& id   = node.GetClassId();
            id_to_index_map[id] = list->size();
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
            if (type == game::AnimationActuatorClass::Type::Transform && !mState.show_transform_actuators)
                continue;
            else if (type == game::AnimationActuatorClass::Type::Material && !mState.show_material_actuators)
                continue;

            const auto& id   = actuator.GetNodeId();
            const auto& node = anim.FindNodeById(id);
            const auto& name = app::FromUtf8(node->GetName());
            const auto id_hash = base::hash_combine(0, id);
            const auto type_hash = base::hash_combine(0, (int)type*7575);
            const auto index = id_to_index_map[id];
            const auto num = (*list)[index].GetNumItems();

            TimelineWidget::TimelineItem item;
            item.text      = QString("%1 (%2)").arg(name).arg(num+1);
            item.id        = app::FromUtf8(actuator.GetId());
            item.starttime = actuator.GetStartTime();
            item.duration  = actuator.GetDuration();
            item.color = QColor(
                    (id_hash >> 24) & 0xff,
                    (id_hash >> 16) & 0xff,
                    (type_hash) & 0xff,
                    120);
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
    mRenderer.SetLoader(workspace);

    mUI.setupUi(this);
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.timeline->SetDuration(10.0f);
    mUI.timeline->SetModel(mTimelineModel.get());
    mUI.actuatorStartTime->setMinimum(0.0f);
    mUI.actuatorStartTime->setMaximum(10.0f);
    mUI.actuatorEndTime->setMinimum(0.0f);
    mUI.actuatorEndTime->setMaximum(10.0f);
    mUI.btnAddActuator->setEnabled(false);

    PopulateFromEnum<game::AnimationActuatorClass::Type>(mUI.actuatorType);
    PopulateFromEnum<game::TransformActuatorClass::Interpolation>(mUI.transformInterpolation);
    PopulateFromEnum<game::MaterialActuatorClass::Interpolation>(mUI.materialInterpolation);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);

    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.actuatorStartTime, 0.0f);
    SetValue(mUI.actuatorEndTime, 10.0f);
    SetValue(mUI.trackName, QString("My Track"));

    setFocusPolicy(Qt::StrongFocus);
    setWindowTitle("My Track");

    mUI.widget->onZoomIn       = std::bind(&AnimationTrackWidget::ZoomIn, this);
    mUI.widget->onZoomOut      = std::bind(&AnimationTrackWidget::ZoomOut, this);
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
}

AnimationTrackWidget::AnimationTrackWidget(app::Workspace* workspace, const std::shared_ptr<game::EntityClass>& entity)
    : AnimationTrackWidget(workspace)
{
    // create a new animation track for the given entity.
    mState.entity = entity;
    mState.track  = std::make_shared<game::AnimationTrackClass>();
    mState.track->SetLooping(GetValue(mUI.looping));
    mState.track->SetDuration(GetValue(mUI.duration));
    mEntity = game::CreateEntityInstance(mState.entity);
    SetValue(mUI.trackID, mState.track->GetId());

    // Put the nodes in the list.
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        mUI.actuatorNode->addItem(app::FromUtf8(node.GetName()));
    }
    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();
    mUI.timeline->Rebuild();
}

AnimationTrackWidget::AnimationTrackWidget(app::Workspace* workspace,
        const std::shared_ptr<game::EntityClass>& entity,
        const game::AnimationTrackClass& track)
        : AnimationTrackWidget(workspace)
{
    // Edit an existing animation track for the given animation.
    mState.entity = entity;
    mState.track  = std::make_shared<game::AnimationTrackClass>(track); // edit a copy.
    SetValue(mUI.trackID, track.GetId());
    SetValue(mUI.trackName, track.GetName());
    SetValue(mUI.looping, track.IsLooping());
    SetValue(mUI.duration, track.GetDuration());
    mEntity = game::CreateEntityInstance(mState.entity);
    mOriginalHash = track.GetHash();

    // put the nodes in the node list
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        mUI.actuatorNode->addItem(app::FromUtf8(node.GetName()));
    }
    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();
    mUI.timeline->SetDuration(track.GetDuration());
    mUI.timeline->Rebuild();
}
AnimationTrackWidget::~AnimationTrackWidget()
{
    DEBUG("Destroy AnimationTrackWidget");
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
    menu.addAction(mUI.actionReset);
}
bool AnimationTrackWidget::SaveState(Settings& settings) const
{
    settings.saveWidget("TrackWidget", mUI.trackID);
    settings.saveWidget("TrackWidget", mUI.trackName);
    settings.saveWidget("TrackWidget", mUI.duration);
    settings.saveWidget("TrackWidget", mUI.delay);
    settings.saveWidget("TrackWidget", mUI.looping);
    settings.saveWidget("TrackWidget", mUI.zoom);
    settings.saveWidget("TrackWidget", mUI.cmbGrid);
    settings.saveWidget("TrackWidget", mUI.actuatorNode);
    settings.saveWidget("TrackWidget", mUI.actuatorType);
    settings.saveWidget("TrackWidget", mUI.actuatorStartTime);
    settings.saveWidget("TrackWidget", mUI.actuatorEndTime);
    settings.saveWidget("TrackWidget", mUI.transformInterpolation);
    settings.saveWidget("TrackWidget", mUI.transformEndPosX);
    settings.saveWidget("TrackWidget", mUI.transformEndPosY);
    settings.saveWidget("TrackWidget", mUI.transformEndSizeX);
    settings.saveWidget("TrackWidget", mUI.transformEndSizeY);
    settings.saveWidget("TrackWidget", mUI.transformEndScaleX);
    settings.saveWidget("TrackWidget", mUI.transformEndScaleY);
    settings.saveWidget("TrackWidget", mUI.transformEndRotation);
    settings.saveWidget("TrackWidget", mUI.materialInterpolation);
    settings.saveWidget("TrackWidget", mUI.materialEndAlpha);
    settings.saveWidget("TrackWidget", mUI.chkShowOrigin);
    settings.saveWidget("TrackWidget", mUI.chkShowGrid);
    settings.saveWidget("TrackWidget", mUI.chkSnap);
    settings.saveWidget("TrackWidget", mUI.chkShowTransformActuators);
    settings.saveWidget("TrackWidget", mUI.chkShowMaterialActuators);
    settings.setValue("TrackWidget", "original_hash", mOriginalHash);

    // use the animation's JSON serialization to save the state.
    {
        const auto& json = mState.entity->ToJson();
        const auto& base64 = base64::Encode(json.dump(2));
        settings.setValue("TrackWidget", "entity", base64);
    }

    {
        const auto& json = mState.track->ToJson();
        const auto& base64 = base64::Encode(json.dump(2));
        settings.setValue("TrackWidget", "track", base64);
    }
    return true;
}
bool AnimationTrackWidget::LoadState(const Settings& settings)
{
    settings.loadWidget("TrackWidget", mUI.trackID);
    settings.loadWidget("TrackWidget", mUI.trackName);
    settings.loadWidget("TrackWidget", mUI.duration);
    settings.loadWidget("TrackWidget", mUI.delay);
    settings.loadWidget("TrackWidget", mUI.looping);
    settings.loadWidget("TrackWidget", mUI.zoom);
    settings.loadWidget("TrackWidget", mUI.cmbGrid);
    settings.loadWidget("TrackWidget", mUI.actuatorNode);
    settings.loadWidget("TrackWidget", mUI.actuatorType);
    settings.loadWidget("TrackWidget", mUI.actuatorStartTime);
    settings.loadWidget("TrackWidget", mUI.actuatorEndTime);
    settings.loadWidget("TrackWidget", mUI.transformInterpolation);
    settings.loadWidget("TrackWidget", mUI.transformEndPosX);
    settings.loadWidget("TrackWidget", mUI.transformEndPosY);
    settings.loadWidget("TrackWidget", mUI.transformEndSizeX);
    settings.loadWidget("TrackWidget", mUI.transformEndSizeY);
    settings.loadWidget("TrackWidget", mUI.transformEndScaleX);
    settings.loadWidget("TrackWidget", mUI.transformEndScaleY);
    settings.loadWidget("TrackWidget", mUI.transformEndRotation);
    settings.loadWidget("TrackWidget", mUI.materialInterpolation);
    settings.loadWidget("TrackWidget", mUI.materialEndAlpha);
    settings.loadWidget("TrackWidget", mUI.chkShowOrigin);
    settings.loadWidget("TrackWidget", mUI.chkShowGrid);
    settings.loadWidget("TrackWidget", mUI.chkSnap);
    settings.loadWidget("TrackWidget", mUI.chkShowTransformActuators);
    settings.loadWidget("TrackWidget", mUI.chkShowMaterialActuators);
    settings.getValue("TrackWidget", "original_hash", &mOriginalHash);

    // try to restore the shared animation class object
    {
        const auto& base64 = settings.getValue("TrackWidget", "entity", std::string(""));
        const auto& json = nlohmann::json::parse(base64::Decode(base64));
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
            mState.entity = std::make_shared<game::EntityClass>(std::move(klass));
            ShareEntity(mState.entity);
        }
    }

    // restore the track state.
    {
        const auto& base64 = settings.getValue("TrackWidget", "track", std::string(""));
        const auto& json = nlohmann::json::parse(base64::Decode(base64));
        auto ret = game::AnimationTrackClass::FromJson(json);
        if (!ret.has_value())
        {
            ERROR("Failed to load animation track state.");
            return false;
        }
        auto klass = std::move(ret.value());
        mState.track = std::make_shared<game::AnimationTrackClass>(std::move(klass));
    }
    // put the nodes in the node list
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        mUI.actuatorNode->addItem(app::FromUtf8(node.GetName()));
    }

    mEntity = game::CreateEntityInstance(mState.entity);

    mState.show_transform_actuators = GetValue(mUI.chkShowTransformActuators);
    mState.show_material_actuators  = GetValue(mUI.chkShowMaterialActuators);
    SetValue(mUI.looping, mState.track->IsLooping());
    SetValue(mUI.duration, mState.track->GetDuration());
    mTreeModel.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();
    mUI.timeline->SetDuration(mState.track->GetDuration());
    mUI.timeline->Rebuild();
    return true;
}

bool AnimationTrackWidget::CanZoomIn() const
{
    const auto max = mUI.zoom->maximum();
    const auto val = mUI.zoom->value();
    return val < max;
}
bool AnimationTrackWidget::CanZoomOut() const
{
    const auto min = mUI.zoom->minimum();
    const auto val = mUI.zoom->value();
    return val > min;
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
    mRenderer.Update(*mPlaybackAnimation, mCurrentTime, secs);

    if (!mPlaybackAnimation->IsPlaying())
    {
        mPlaybackAnimation.reset();
        mUI.timeline->SetCurrentTime(0.0f);
        mUI.timeline->Update();
        mUI.timeline->SetFreezeItems(false);
        mUI.actionPlay->setEnabled(true);
        mUI.actionPause->setEnabled(false);
        mUI.actionStop->setEnabled(false);
        mUI.actionReset->setEnabled(true);
        mUI.time->clear();
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
        mUI.timeline->SetCurrentTime(time);
        mUI.timeline->Repaint();
        mUI.time->setText(QString::number(time));
    }
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

void AnimationTrackWidget::on_actionPlay_triggered()
{
    if (mPlayState == PlayState::Paused)
    {
        mPlayState = PlayState::Playing;
        mUI.actionPause->setEnabled(true);
        return;
    }

    // create new animation instance and play the animation track.
    auto track = game::CreateAnimationTrackInstance(mState.track);
    mPlaybackAnimation = game::CreateEntityInstance(mState.entity);
    mPlaybackAnimation->Play(std::move(track));
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
    mUI.time->clear();
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

    for (size_t i=0; i<mState.entity->GetNumTracks(); ++i)
    {
        auto& track = mState.entity->GetAnimationTrack(i);
        if (track.GetId() != mState.track->GetId())
            continue;

        // copy it over.
        track = *mState.track;
        return;
    }
    // add a copy
    mState.entity->AddAnimationTrack(*mState.track);
}

void AnimationTrackWidget::on_actionReset_triggered()
{
    if (mPlayState != PlayState::Stopped)
        return;
    mEntity = game::CreateEntityInstance(mState.entity);
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

void AnimationTrackWidget::on_actionClearActuators_triggered()
{
    mState.track->Clear();
    mUI.timeline->ClearSelection();
    mUI.timeline->Rebuild();
    SelectedItemChanged(nullptr);
}

void AnimationTrackWidget::on_actionAddTransformActuator_triggered()
{
    const auto* timeline = mUI.timeline->GetCurrentTimeline();
    if (timeline == nullptr)
        return;
    const auto index = mUI.timeline->GetCurrentTimelineIndex();
    if (index >= mEntity->GetNumNodes())
        return;
    // get the node from the animation class object.
    // the class node's transform values are used for the
    // initial data for the actuator.
    const auto& node = mState.entity->GetNode(index);

    // the seconds (seconds into the duration of the animation)
    // is set when the context menu with this QAction is opened.
    const auto seconds = mUI.actionAddTransformActuator->data().toFloat();
    const auto duration = mState.track->GetDuration();
    const auto position = seconds / duration;

    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i=0; i<mState.track->GetNumActuators(); ++i)
    {
        const auto& klass = mState.track->GetActuatorClass(i);
        if (klass.GetNodeId() != node.GetClassId())
            continue;
        const auto start = klass.GetStartTime();
        const auto end   = klass.GetStartTime() + klass.GetDuration();
        if (start >= position)
            hi_bound = std::min(hi_bound, start);
        if (end <= position)
            lo_bound = std::max(lo_bound, end);
    }

    game::TransformActuatorClass klass;
    klass.SetNodeId(node.GetClassId());
    klass.SetStartTime(lo_bound);
    klass.SetDuration(hi_bound - lo_bound);
    klass.SetEndPosition(node.GetTranslation());
    klass.SetEndSize(node.GetSize());
    klass.SetEndScale(node.GetScale());
    klass.SetInterpolation(game::TransformActuatorClass::Interpolation ::Linear);
    klass.SetEndRotation(node.GetRotation());

    mState.track->AddActuator(std::move(klass));
    mUI.timeline->Rebuild();

    DEBUG("New transform actuator for node '%1' from %2s to %3s.",
          node.GetName(), lo_bound * duration, hi_bound * duration);
}

void AnimationTrackWidget::on_actionAddMaterialActuator_triggered()
{
    const auto* timeline = mUI.timeline->GetCurrentTimeline();
    if (timeline == nullptr)
        return;
    const auto index = mUI.timeline->GetCurrentTimelineIndex();
    if (index >= mEntity->GetNumNodes())
        return;
    // get the node from the animation class object.
    // the class node's transform values are used for the
    // initial data for the actuator.
    const auto& node = mState.entity->GetNode(index);

    // the seconds (seconds into the duration of the animation)
    // is set when the context menu with this QAction is opened.
    const auto seconds = mUI.actionAddTransformActuator->data().toFloat();
    const auto duration = mState.track->GetDuration();
    const auto position = seconds / duration;

    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i=0; i<mState.track->GetNumActuators(); ++i)
    {
        const auto& klass = mState.track->GetActuatorClass(i);
        if (klass.GetNodeId() != node.GetClassId())
            continue;
        const auto start = klass.GetStartTime();
        const auto end   = klass.GetStartTime() + klass.GetDuration();
        if (start >= position)
            hi_bound = std::min(hi_bound, start);
        if (end <= position)
            lo_bound = std::max(lo_bound, end);
    }
    game::MaterialActuatorClass klass;
    klass.SetNodeId(node.GetClassId());
    klass.SetStartTime(lo_bound);
    klass.SetDuration(hi_bound - lo_bound);
    klass.SetEndAlpha(GetValue(mUI.materialEndAlpha));
    klass.SetInterpolation(GetValue(mUI.materialInterpolation));

    mState.track->AddActuator(std::move(klass));
    mUI.timeline->Rebuild();

    DEBUG("New material actuator for node '%1' from %2s to %3s.",
          node.GetName(), lo_bound * duration, hi_bound * duration);
}

void AnimationTrackWidget::on_duration_valueChanged(double value)
{
    QSignalBlocker s(mUI.actuatorStartTime);
    QSignalBlocker e(mUI.actuatorEndTime);
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
    mUI.actuatorStartTime->setMinimum(start_lo_bound / duration * value);
    mUI.actuatorStartTime->setMaximum(start_hi_bound / duration * value);
    mUI.actuatorEndTime->setMinimum(end_lo_bound / duration * value);
    mUI.actuatorEndTime->setMaximum(end_hi_bound / duration * value);
    SetValue(mUI.actuatorStartTime, start / duration * value);
    SetValue(mUI.actuatorEndTime, end / duration * value);

    mUI.timeline->SetDuration(value);
    mUI.timeline->Update();
    mState.track->SetDuration(value);
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
    if (index == -1)
    {
        mUI.actuatorType->setEnabled(false);
        mUI.actuatorStartTime->setEnabled(false);
        mUI.actuatorEndTime->setEnabled(false);
        mUI.actuatorProperties->setEnabled(false);
        mUI.btnAddActuator->setEnabled(false);
        SetValue(mUI.actuatorType, game::AnimationActuatorClass::Type::Transform);
        SetValue(mUI.transformInterpolation, game::TransformActuatorClass::Interpolation::Cosine);
        SetValue(mUI.transformEndPosX, 0.0f);
        SetValue(mUI.transformEndPosY, 0.0f);
        SetValue(mUI.transformEndSizeX, 0.0f);
        SetValue(mUI.transformEndSizeY, 0.0f);
        SetValue(mUI.transformEndScaleX, 0.0f);
        SetValue(mUI.transformEndScaleY, 0.0f);
        SetValue(mUI.transformEndRotation, 0.0f);
        SetValue(mUI.materialInterpolation, game::MaterialActuatorClass::Interpolation::Cosine);
        SetValue(mUI.materialEndAlpha, 0.0f);
        SetValue(mUI.actuatorStartTime, 0.0f);
        SetValue(mUI.actuatorEndTime, mState.track->GetDuration());
    }
    else
    {
        // using the node's current transformation data
        // as the default end transformation. I.e. "no transformation"
        const auto& node    = mEntity->GetNode(index);
        const auto& pos     = node.GetTranslation();
        const auto& size    = node.GetSize();
        const auto& scale   = node.GetScale();
        const auto rotation = node.GetRotation();
        SetValue(mUI.actuatorType, game::AnimationActuatorClass::Type::Transform);
        SetValue(mUI.transformInterpolation, game::TransformActuatorClass::Interpolation::Cosine);
        SetValue(mUI.transformEndPosX, pos.x);
        SetValue(mUI.transformEndPosY, pos.y);
        SetValue(mUI.transformEndSizeX, size.x);
        SetValue(mUI.transformEndSizeY, size.y);
        SetValue(mUI.transformEndScaleX, scale.x);
        SetValue(mUI.transformEndScaleY, scale.y);
        SetValue(mUI.transformEndRotation, qRadiansToDegrees(rotation));
        SetValue(mUI.materialInterpolation, game::MaterialActuatorClass::Interpolation::Cosine);

        if (const auto* draw = node.GetDrawable())
        {
            const auto &material_id = draw->GetMaterialId();
            if (!material_id.empty())
            {
                const auto &material = mWorkspace->FindMaterialClass(material_id);
                SetValue(mUI.materialEndAlpha, material->GetBaseAlpha());
            }
            if (draw->TestFlag(game::DrawableItemClass::Flags::OverrideAlpha))
                SetValue(mUI.materialEndAlpha, draw->GetAlpha());
        }

        // there could be multiple slots where the next actuator is
        // to placed. the limits would be difficult to express
        // with the spinboxes, so we just reset the min/max to the
        // whole animation duration and then clamp on add if needed.
        const auto duration = mState.track->GetDuration();
        mUI.actuatorStartTime->setMinimum(0.0f);
        mUI.actuatorStartTime->setMaximum(duration);
        mUI.actuatorEndTime->setMinimum(0.0f);
        mUI.actuatorEndTime->setMaximum(duration);

        mUI.actuatorType->setEnabled(true);
        mUI.actuatorStartTime->setEnabled(true);
        mUI.actuatorEndTime->setEnabled(true);
        mUI.actuatorProperties->setEnabled(true);
        mUI.actuatorProperties->setCurrentIndex(0);
        mUI.btnAddActuator->setEnabled(true);
    }
}

void AnimationTrackWidget::on_actuatorType_currentIndexChanged(int index)
{
    const game::AnimationActuator::Type type = GetValue(mUI.actuatorType);
    if (type == game::AnimationActuator::Type::Transform)
    {
        mUI.transformActuator->setEnabled(true);
        mUI.materialActuator->setEnabled(false);
        mUI.actuatorProperties->setCurrentIndex(0);
    }
    else if (type == game::AnimationActuator::Type::Material)
    {
        mUI.materialActuator->setEnabled(true);
        mUI.transformActuator->setEnabled(false);
        mUI.actuatorProperties->setCurrentIndex(1);
    }
}

void AnimationTrackWidget::on_transformInterpolation_currentIndexChanged(int index)
{
    SetSelectedActuatorProperties();
}
void AnimationTrackWidget::on_materialInterpolation_currentIndexChanged(int index)
{
    SetSelectedActuatorProperties();
}

void AnimationTrackWidget::on_timeline_customContextMenuRequested(QPoint)
{
    const auto* selected = mUI.timeline->GetSelectedItem();
    mUI.actionDeleteActuator->setEnabled(selected != nullptr);
    mUI.actionClearActuators->setEnabled(mState.track->GetNumActuators());

    mUI.actionAddTransformActuator->setEnabled(false);
    mUI.actionAddMaterialActuator->setEnabled(false);
    // map the click point to a position in the timeline.
    const auto* timeline = mUI.timeline->GetCurrentTimeline();
    if (timeline)
    {
        const auto widget_coord = mUI.timeline->mapFromGlobal(QCursor::pos());
        const auto seconds = mUI.timeline->MapToSeconds(widget_coord);
        const auto duration = mState.track->GetDuration();
        if (seconds > 0.0f && seconds < duration)
        {
            mUI.actionAddTransformActuator->setEnabled(true);
            mUI.actionAddMaterialActuator->setEnabled(true);
        }
        mUI.actionAddTransformActuator->setData(seconds);
        mUI.actionAddMaterialActuator->setData(seconds);
    }

    QMenu menu(this);
    QMenu add(this);
    add.setIcon(QIcon("icons:add.png"));
    add.setTitle(tr("Add Actuator..."));
    add.addAction(mUI.actionAddTransformActuator);
    add.addAction(mUI.actionAddMaterialActuator);
    add.setEnabled(timeline != nullptr);
    menu.addMenu(&add);
    menu.addSeparator();
    menu.addAction(mUI.actionDeleteActuator);
    menu.addAction(mUI.actionClearActuators);
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

void AnimationTrackWidget::on_materialEndAlpha_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* draw = node->GetDrawable())
        {
            draw->SetAlpha(value);
            SetSelectedActuatorProperties();
        }
    }
}

void AnimationTrackWidget::on_chkShowMaterialActuators_stateChanged(int)
{
    mState.show_material_actuators = GetValue(mUI.chkShowMaterialActuators);
    mUI.timeline->Rebuild();
}
void AnimationTrackWidget::on_chkShowTransformActuators_stateChanged(int)
{
    mState.show_transform_actuators = GetValue(mUI.chkShowTransformActuators);
    mUI.timeline->Rebuild();
}

void AnimationTrackWidget::on_btnAddActuator_clicked()
{
    const auto& name = app::ToUtf8(GetValue(mUI.actuatorNode));
    const auto* node = mState.entity->FindNodeByName(name);
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

    const auto& id = node->GetClassId();
    float lo_bound = 0.0f;
    float hi_bound = 1.0f;
    for (size_t i=0; i<mState.track->GetNumActuators(); ++i)
    {
        const auto& klass = mState.track->GetActuatorClass(i);
        if (klass.GetNodeId() != id)
            continue;
        const auto start = klass.GetStartTime();
        const auto end   = klass.GetStartTime() + klass.GetDuration();
        if (start >= norm_start)
            hi_bound = std::min(hi_bound, start);
        if (end <= norm_start)
            lo_bound = std::max(lo_bound, end);
        // this isn't a free slot actually.
        if (norm_start >= start && norm_start <= end)
            return;
    }
    const auto start = std::max(lo_bound, norm_start);
    const auto end   = std::min(hi_bound, norm_end);
    const game::AnimationActuator::Type type = GetValue(mUI.actuatorType);
    if (type == game::AnimationActuatorClass::Type::Transform)
    {
        game::TransformActuatorClass klass;
        klass.SetNodeId(node->GetClassId());
        klass.SetStartTime(start);
        klass.SetDuration(end - start);
        klass.SetEndPosition(GetValue(mUI.transformEndPosX), GetValue(mUI.transformEndPosY));
        klass.SetEndSize(GetValue(mUI.transformEndSizeX), GetValue(mUI.transformEndSizeY));
        klass.SetEndScale(GetValue(mUI.transformEndScaleX), GetValue(mUI.transformEndScaleY));
        klass.SetInterpolation(GetValue(mUI.transformInterpolation));
        klass.SetEndRotation(qDegreesToRadians((float) GetValue(mUI.transformEndRotation)));
        mState.track->AddActuator(klass);
    }
    else if (type == game::AnimationActuatorClass::Type::Material)
    {
        game::MaterialActuatorClass klass;
        klass.SetNodeId(node->GetClassId());
        klass.SetStartTime(start);
        klass.SetDuration(end - start);
        klass.SetEndAlpha(GetValue(mUI.materialEndAlpha));
        klass.SetInterpolation(GetValue(mUI.materialInterpolation));
        mState.track->AddActuator(klass);
    }
    mUI.timeline->Rebuild();

    DEBUG("New actuator for node '%1' from %2s to %3s", node->GetName(),
          start * animation_duration, end * animation_duration);
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
    else if (auto* material = dynamic_cast<game::MaterialActuatorClass*>(klass))
    {
        material->SetInterpolation(GetValue(mUI.materialInterpolation));
        material->SetEndAlpha(GetValue(mUI.materialEndAlpha));
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
    const auto& klass   = mState.entity->GetNode(index);
    const auto& pos     = klass.GetTranslation();
    const auto& size    = klass.GetSize();
    const auto& scale   = klass.GetScale();
    const auto rotation = klass.GetRotation();
    SetValue(mUI.transformEndPosX, pos.x);
    SetValue(mUI.transformEndPosY, pos.y);
    SetValue(mUI.transformEndSizeX, size.x);
    SetValue(mUI.transformEndSizeY, size.y);
    SetValue(mUI.transformEndScaleX, scale.x);
    SetValue(mUI.transformEndScaleY, scale.y);
    SetValue(mUI.transformEndRotation, qRadiansToDegrees(rotation));

    auto& node = mEntity->GetNode(index);
    node.SetTranslation(pos);
    node.SetSize(size);
    node.SetScale(scale);
    node.SetRotation(rotation);

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
    if (item == nullptr)
    {
        const auto duration = mState.track->GetDuration();
        mUI.actuatorStartTime->setMinimum(0.0f);
        mUI.actuatorStartTime->setMaximum(duration);
        mUI.actuatorEndTime->setMinimum(0.0f);
        mUI.actuatorEndTime->setMaximum(duration);
        SetValue(mUI.actuatorType, game::AnimationActuatorClass::Type::Transform);
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
        SetValue(mUI.materialInterpolation, game::MaterialActuatorClass::Interpolation::Cosine);
        SetValue(mUI.materialEndAlpha, 0.0f);
        mUI.actuatorNode->setCurrentIndex(-1);
        mUI.actuatorGroup->setTitle("Actuator");
        mUI.actuatorNode->setEnabled(true);
        mUI.actuatorType->setEnabled(false);
        mUI.actuatorStartTime->setEnabled(false);
        mUI.actuatorEndTime->setEnabled(false);
        mUI.actuatorProperties->setEnabled(false);
        mUI.btnAddActuator->setEnabled(false);
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
        QSignalBlocker foo(mUI.actuatorStartTime);
        QSignalBlocker bar(mUI.actuatorEndTime);
        mUI.actuatorStartTime->setMinimum(lo_bound * duration);
        mUI.actuatorStartTime->setMaximum(hi_bound * duration);
        mUI.actuatorEndTime->setMinimum(lo_bound * duration);
        mUI.actuatorEndTime->setMaximum(hi_bound * duration);

        SetValue(mUI.actuatorStartTime, start);
        SetValue(mUI.actuatorEndTime, end);
        SetValue(mUI.actuatorNode, app::FromUtf8(node->GetClassName()));
        if (const auto* ptr = dynamic_cast<const game::TransformActuatorClass*>(actuator))
        {
            SetValue(mUI.actuatorType, game::AnimationActuatorClass::Type::Transform);
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
            mUI.actuatorProperties->setEnabled(true);
            mUI.actuatorProperties->setCurrentIndex(0);
            mUI.transformActuator->setEnabled(true);
            mUI.materialActuator->setEnabled(false);

            node->SetTranslation(pos);
            node->SetSize(size);
            node->SetScale(scale);
            node->SetRotation(rotation);
        }
        else if (const auto* ptr = dynamic_cast<const game::MaterialActuatorClass*>(actuator))
        {
            SetValue(mUI.actuatorType, game::AnimationActuatorClass::Type::Material);
            SetValue(mUI.materialInterpolation, ptr->GetInterpolation());
            SetValue(mUI.materialEndAlpha, ptr->GetEndAlpha());
            mUI.actuatorProperties->setEnabled(true);
            mUI.actuatorProperties->setCurrentIndex(1);
            mUI.transformActuator->setEnabled(false);
            mUI.materialActuator->setEnabled(true);
        }
        else
        {
            mUI.actuatorProperties->setEnabled(false);
        }
        mUI.actuatorGroup->setTitle(QString("Actuator - %1").arg(item->text));
        mUI.btnAddActuator->setEnabled(true);
        mUI.actuatorType->setEnabled(false);
        mUI.actuatorNode->setEnabled(false);
        mUI.actuatorStartTime->setEnabled(true);
        mUI.actuatorEndTime->setEnabled(true);

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
    SetValue(mUI.actuatorStartTime, start);
    SetValue(mUI.actuatorEndTime, end);
}

void AnimationTrackWidget::InitScene(unsigned int width, unsigned int height)
{
    mState.camera_offset_x = width * 0.5;
    mState.camera_offset_y = height * 0.5;

    // offset the viewport so that the origin of the 2d space is in the middle of the viewport
    const auto dist_x = mState.camera_offset_x  - (width / 2.0f);
    const auto dist_y = mState.camera_offset_y  - (height / 2.0f);
    mUI.viewPosX->setValue(dist_x);
    mUI.viewPosY->setValue(dist_y);
}

void AnimationTrackWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto view_rotation_time = math::clamp(0.0f, 1.0f,
        mCurrentTime - mViewTransformStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.viewRotation->value(),
        view_rotation_time, math::Interpolation::Cosine);

    gfx::Transform view;
    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    view.Push();
    view.Scale(GetValue(mUI.viewScaleX), GetValue(mUI.viewScaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(view_rotation_angle));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    // render endless background grid.
    if (mUI.chkShowGrid->isChecked())
    {
        const float zoom = GetValue(mUI.zoom);
        const float xs = GetValue(mUI.viewScaleX);
        const float ys = GetValue(mUI.viewScaleY);
        const GridDensity grid = GetValue(mUI.cmbGrid);
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

    // pop view transformation
    view.Pop();
}

void AnimationTrackWidget::MouseMove(QMouseEvent* mickey)
{
    if (mCurrentTool) {
        gfx::Transform view;
        view.Scale(GetValue(mUI.viewScaleX), GetValue(mUI.viewScaleY));
        view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.viewRotation->value()));
        view.Translate(mState.camera_offset_x, mState.camera_offset_y);
        mCurrentTool->MouseMove(mickey, view);
    }
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();

    // update the properties that might have changed as the result of application
    // of the current tool.

    // update the distance to center.
    const auto dist_x = mState.camera_offset_x - (width / 2.0f);
    const auto dist_y = mState.camera_offset_y - (height / 2.0f);
    mUI.viewPosX->setValue(dist_x);
    mUI.viewPosY->setValue(dist_y);

    UpdateTransformActuatorUI();
    SetSelectedActuatorProperties();
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
            const auto& size = hitnode->GetSize();
            // check if any particular special area of interest is being hit
            const bool bottom_right_hitbox_hit = hitpos.x >= size.x - 10.0f &&
                                                 hitpos.y >= size.y - 10.0f;
            const bool top_left_hitbox_hit     = hitpos.x >= 0 && hitpos.x <= 10.0f &&
                                                 hitpos.y >= 0 && hitpos.y <= 10.0f;
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
            const auto& name = hitnode->GetClassName();
            const auto index = mUI.actuatorNode->findText(app::FromUtf8(name));
            SetValue(mUI.actuatorNode, app::FromUtf8(name));
            on_actuatorNode_currentIndexChanged(index);
        }
        else if (!mUI.timeline->GetSelectedItem())
        {
            SetValue(mUI.actuatorNode, QString(""));
            on_actuatorNode_currentIndexChanged(-1);
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

    mCurrentTool->MouseRelease(mickey, view);
    mCurrentTool.reset();
}
bool AnimationTrackWidget::KeyPress(QKeyEvent* key)
{
    return false;
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

game::EntityNode* AnimationTrackWidget::GetCurrentNode()
{
    const auto index = mUI.actuatorNode->currentIndex();
    if (index == -1)
        return nullptr;
    return &mEntity->GetNode(index);
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

} // namespace
