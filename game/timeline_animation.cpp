// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "base/hash.h"
#include "base/logging.h"
#include "base/utility.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity.h"
#include "game/timeline_animation.h"
#include "game/timeline_animator.h"
#include "game/timeline_animation_trigger.h"
#include "game/timeline_transform_animator.h"
#include "game/timeline_kinematic_animator.h"
#include "game/timeline_material_animator.h"
#include "game/timeline_property_animator.h"

namespace game
{

AnimationClass::AnimationClass()
{
    mId = base::RandomString(10);
}

AnimationClass::AnimationClass(const AnimationClass& other)
{
    for (const auto& animator : other.mAnimators)
    {
        mAnimators.push_back(animator->Copy());
    }
    for (const auto& trigger : other.mTriggers)
    {
        mTriggers.push_back(trigger->Copy());
    }
    mId       = other.mId;
    mName     = other.mName;
    mDuration = other.mDuration;
    mLooping  = other.mLooping;
    mDelay    = other.mDelay;
}
AnimationClass::AnimationClass(AnimationClass&& other) noexcept
{
    mId        = std::move(other.mId);
    mAnimators = std::move(other.mAnimators);
    mTriggers  = std::move(other.mTriggers);
    mName      = std::move(other.mName);
    mDuration  = other.mDuration;
    mLooping   = other.mLooping;
    mDelay     = other.mDelay;
}

void AnimationClass::DeleteAnimator(std::size_t index) noexcept
{
    ASSERT(index < mAnimators.size());
    auto it = mAnimators.begin();
    std::advance(it, index);
    mAnimators.erase(it);
}

void AnimationClass::DeleteTrigger(std::size_t index) noexcept
{
    ASSERT(index < mTriggers.size());
    auto it = mTriggers.begin();
    std::advance(it, index);
    mTriggers.erase(it);
}

bool AnimationClass::DeleteAnimatorById(const std::string& id) noexcept
{
    for (auto it = mAnimators.begin(); it != mAnimators.end(); ++it)
    {
        if ((*it)->GetId() == id) {
            mAnimators.erase(it);
            return true;
        }
    }
    return false;
}

bool AnimationClass::DeleteTriggerById(const std::string& id) noexcept
{
    for (auto it = mTriggers.begin(); it != mTriggers.end(); ++it)
    {
        if ((*it)->GetId() == id) {
            mTriggers.erase(it);
            return true;
        }
    }
    return false;
}

AnimatorClass* AnimationClass::FindAnimatorById(const std::string& id) noexcept
{
    for (auto& animator : mAnimators) {
        if (animator->GetId() == id)
            return animator.get();
    }
    return nullptr;
}
const AnimatorClass* AnimationClass::FindAnimatorById(const std::string& id) const noexcept
{
    for (auto& animator : mAnimators) {
        if (animator->GetId() == id)
            return animator.get();
    }
    return nullptr;
}

AnimationTriggerClass* AnimationClass::FindTriggerById(const std::string& id)
{
    for (auto& trigger : mTriggers)
    {
        if (trigger->GetId() == id)
            return trigger.get();
    }
    return nullptr;
}

 const AnimationTriggerClass* AnimationClass::FindTriggerById(const std::string& id) const
 {
     for (auto& trigger : mTriggers)
     {
         if (trigger->GetId() == id)
             return trigger.get();
     }
     return nullptr;
 }

std::unique_ptr<Animator> AnimationClass::CreateAnimatorInstance(std::size_t index) const
{
    const auto& klass = mAnimators[index];
    if (klass->GetType() == AnimatorClass::Type::TransformAnimator)
        return std::make_unique<TransformAnimator>(std::static_pointer_cast<TransformAnimatorClass>(klass));
    else if (klass->GetType() == AnimatorClass::Type::PropertyAnimator)
        return std::make_unique<PropertyAnimator>(std::static_pointer_cast<PropertyAnimatorClass>(klass));
    else if (klass->GetType() == AnimatorClass::Type::KinematicAnimator)
        return std::make_unique<KinematicAnimator>(std::static_pointer_cast<KinematicAnimatorClass>(klass));
    else if (klass->GetType() == AnimatorClass::Type::BooleanPropertyAnimator)
        return std::make_unique<BooleanPropertyAnimator>(std::static_pointer_cast<BooleanPropertyAnimatorClass>(klass));
    else if (klass->GetType() == AnimatorClass::Type::MaterialAnimator)
        return std::make_unique<MaterialAnimator>(std::static_pointer_cast<MaterialAnimatorClass>(klass));
    else BUG("Unknown animator type");
    return {};
}

std::unique_ptr<AnimationTrigger> AnimationClass::CreateTriggerInstance(std::size_t index) const
{
    return std::make_unique<AnimationTrigger>(mTriggers[index]);
}

std::size_t AnimationClass::GetHash() const noexcept
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mLooping);
    hash = base::hash_combine(hash, mDelay);
    for (const auto& animator : mAnimators)
        hash = base::hash_combine(hash, animator->GetHash());

    for (const auto& trigger : mTriggers)
        hash = base::hash_combine(hash, trigger->GetHash());

    return hash;
}

void AnimationClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("duration", mDuration);
    data.Write("delay", mDelay);
    data.Write("looping", mLooping);
    for (const auto &animator : mAnimators)
    {
        auto meta = data.NewWriteChunk();
        auto act  = data.NewWriteChunk();
        animator->IntoJson(*act);
        meta->Write("type", animator->GetType());
        meta->Write("animator", std::move(act));
        data.AppendChunk("animators", std::move(meta));
    }

    for (const auto& trigger : mTriggers)
    {
        auto chunk = data.NewWriteChunk();
        trigger->IntoJson(*chunk);
        data.AppendChunk("triggers", std::move(chunk));
    }
}

bool AnimationClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",       &mId);
    ok &= data.Read("name",     &mName);
    ok &= data.Read("duration", &mDuration);
    ok &= data.Read("delay",    &mDelay);
    ok &= data.Read("looping",  &mLooping);

    // legacy name before animators became animators.
    for (unsigned i=0; i<data.GetNumChunks("animators"); ++i)
    {
        AnimatorClass::Type type;
        const auto& meta_chunk = data.GetReadChunk("animators", i);
        const auto& data_chunk = meta_chunk->GetReadChunk("animator");
        if (data_chunk && meta_chunk->Read("type", &type))
        {
            std::shared_ptr<AnimatorClass> animator;
            if (type == AnimatorClass::Type::TransformAnimator)
                animator = std::make_shared<TransformAnimatorClass>();
            else if (type == AnimatorClass::Type::PropertyAnimator)
                animator = std::make_shared<PropertyAnimatorClass>();
            else if (type == AnimatorClass::Type::KinematicAnimator)
                animator = std::make_shared<KinematicAnimatorClass>();
            else if (type == AnimatorClass::Type::BooleanPropertyAnimator)
                animator = std::make_shared<BooleanPropertyAnimatorClass>();
            else if (type == AnimatorClass::Type::MaterialAnimator)
                animator = std::make_shared<MaterialAnimatorClass>();
            else BUG("Unknown animator type.");

            mAnimators.push_back(animator);
            if (animator->FromJson(*data_chunk))
                continue;

            WARN("Animator failed to load completely. [animation='%1']", mName);
        }
        else if (!data_chunk)
            WARN("Missing animator data chunk. [animation='%1']", mName);
        else WARN("Unrecognized animator type. [animation='%1']", mName);

        ok = false;
    }

    for (unsigned i=0; i<data.GetNumChunks("triggers"); ++i)
    {
        const auto& chunk = data.GetReadChunk("triggers", i);
        auto trigger = std::make_unique<AnimationTriggerClass>();
        ok &= trigger->FromJson(*chunk);
        mTriggers.push_back(std::move(trigger));
    }
    return ok;
}

AnimationClass AnimationClass::Clone() const
{
    AnimationClass ret;
    ret.mName     = mName;
    ret.mDuration = mDuration;
    ret.mLooping  = mLooping;
    ret.mDelay    = mDelay;
    for (const auto& klass : mAnimators)
        ret.mAnimators.push_back(klass->Clone());
    for (const auto& trigger : mTriggers)
        ret.mTriggers.push_back(trigger->Clone());
    return ret;
}

AnimationClass& AnimationClass::operator=(const AnimationClass& other)
{
    if (this == &other)
        return *this;
    AnimationClass copy(other);
    std::swap(mId, copy.mId);
    std::swap(mAnimators, copy.mAnimators);
    std::swap(mTriggers, copy.mTriggers);
    std::swap(mName, copy.mName);
    std::swap(mDuration, copy.mDuration);
    std::swap(mLooping, copy.mLooping);
    std::swap(mDelay, copy.mDelay);
    return *this;
}

Animation::Animation(const std::shared_ptr<const AnimationClass>& klass)
    : mClass(klass)
{
    for (size_t i=0; i< mClass->GetNumAnimators(); ++i)
    {
        AnimatorState state;
        state.animator = mClass->CreateAnimatorInstance(i);
        state.node     = state.animator->GetNodeId();
        state.ended    = false;
        state.started  = false;
        mTracks.push_back(std::move(state));
    }
    for (size_t i=0; i<mClass->GetNumTriggers(); ++i)
    {
        AnimatorState state;
        state.trigger   = mClass->CreateTriggerInstance(i);
        state.node      = state.trigger->GetNodeId();
        state.triggered = false;
        mTracks.push_back(std::move(state));
    }

    mDelay = klass->GetDelay();
    // start at negative delay time, then the actual animation playback
    // starts after the current time reaches 0 and all delay has been "consumed".
    mCurrentTime = -mDelay;
}
Animation::Animation(const AnimationClass& klass)
    : Animation(std::make_shared<AnimationClass>(klass))
{}

Animation::Animation(const Animation& other) : mClass(other.mClass)
{
    for (const auto& other_state : other.mTracks)
    {
        AnimatorState track;
        track.node     = other_state.node;
        track.ended    = other_state.ended;
        track.started  = other_state.started;
        if (other_state.animator)
            track.animator = other_state.animator->Copy();
        if (other_state.trigger)
            track.trigger = other_state.trigger->Copy();

        mTracks.push_back(std::move(track));
    }

    mCurrentTime = other.mCurrentTime;
    mDelay       = other.mDelay;
}
    // Move ctor.
Animation::Animation(Animation&& other) noexcept
{
    mClass       = other.mClass;
    mCurrentTime = other.mCurrentTime;
    mDelay       = other.mDelay;
    mTracks      = std::move(other.mTracks);
}

Animation::~Animation() = default;

void Animation::Update(float dt) noexcept
{
    const auto duration = mClass->GetDuration();

    mCurrentTime = math::clamp(-mDelay, duration, mCurrentTime + dt);
}

void Animation::Apply(EntityNode& node) const
{
    // if we're delaying then skip until delay is consumed.
    if (mCurrentTime < 0)
        return;
    const auto duration = mClass->GetDuration();
    const auto animation_time = mCurrentTime / duration;

    // todo: keep the tracks in some smarter data structure or perhaps
    // in a sorted vector and then binary search.
    for (auto& track : mTracks)
    {
        if (track.node != node.GetClassId())
            continue;

        auto* trigger = track.trigger.get();
        auto* animator = track.animator.get();
        if (trigger)
        {
            if (!track.triggered)
            {
                const auto& trigger_time_point = trigger->GetTime();
                if (animation_time >= trigger_time_point)
                {
                    trigger->Trigger(node);
                    track.triggered = true;
                }
            }
        }
        else if (animator)
        {
            const auto start_time_point  = track.animator->GetStartTime();
            const auto animator_duration = track.animator->GetDuration();
            const auto end_time_point    =  math::clamp(0.0f, 1.0f, start_time_point + animator_duration);
            if (animation_time < start_time_point)
                continue;
            else if (animation_time >= end_time_point)
            {
                if (!track.ended)
                {
                    track.animator->Finish(node);
                    track.ended = true;
                }
                continue;
            }
            if (!track.started)
            {
                track.animator->Start(node);
                track.started = true;
            }
            const auto t = math::clamp(0.0f, 1.0f, (animation_time - start_time_point) / animator_duration);
            track.animator->Apply(node, t);
        }
    }
}

void Animation::Restart() noexcept
{
    for (auto& track : mTracks)
    {
        if (track.animator)
        {
            ASSERT(track.started);
            ASSERT(track.ended);
        }
        if (track.trigger)
        {
            ASSERT(track.triggered);
        }
        track.started   = false;
        track.ended     = false;
        track.triggered = false;
    }
    mCurrentTime = -mDelay;
}

bool Animation::IsComplete() const noexcept
{
    for (const auto& track : mTracks)
    {
        if (track.animator && !track.ended)
            return false;
        if (track.trigger && !track.triggered)
            return false;
    }
    if (mCurrentTime >= mClass->GetDuration())
        return true;
    return false;
}

Animator* Animation::FindAnimatorById(const std::string& id) noexcept
{
    for (auto& item : mTracks)
    {
        if (item.animator->GetClassId() == id)
            return item.animator.get();
    }
    return nullptr;
}
Animator* Animation::FindAnimatorByName(const std::string& name) noexcept
{
    for (auto& item : mTracks)
    {
        if (item.animator->GetClassName() == name)
            return item.animator.get();
    }
    return nullptr;
}

const Animator* Animation::FindAnimatorById(const std::string& id) const noexcept
{
    for (auto& item : mTracks)
    {
        if (item.animator->GetClassId() == id)
            return item.animator.get();
    }
    return nullptr;
}
const Animator* Animation::FindAnimatorByName(const std::string& name) const noexcept
{
    for (auto& item : mTracks)
    {
        if (item.animator->GetClassName() == name)
            return item.animator.get();
    }
    return nullptr;
}

std::unique_ptr<Animation> CreateAnimationInstance(const std::shared_ptr<const AnimationClass>& klass)
{
    return std::make_unique<Animation>(klass);
}

} // namespace
