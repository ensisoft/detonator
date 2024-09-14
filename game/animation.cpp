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
#include "game/animation.h"
#include "game/animator.h"
#include "game/transform_animator.h"
#include "game/kinematic_animator.h"
#include "game/material_animator.h"
#include "game/property_animator.h"
#include "game/entity.h"

namespace game
{

AnimationClass::AnimationClass()
{
    mId = base::RandomString(10);
}

AnimationClass::AnimationClass(const AnimationClass& other)
{
    for (const auto& a : other.mActuators)
    {
        mActuators.push_back(a->Copy());
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
    mActuators = std::move(other.mActuators);
    mName      = std::move(other.mName);
    mDuration  = other.mDuration;
    mLooping   = other.mLooping;
    mDelay     = other.mDelay;
}

void AnimationClass::DeleteAnimator(std::size_t index) noexcept
{
    ASSERT(index < mActuators.size());
    auto it = mActuators.begin();
    std::advance(it, index);
    mActuators.erase(it);
}

bool AnimationClass::DeleteAnimatorById(const std::string& id) noexcept
{
    for (auto it = mActuators.begin(); it != mActuators.end(); ++it)
    {
        if ((*it)->GetId() == id) {
            mActuators.erase(it);
            return true;
        }
    }
    return false;
}
AnimatorClass* AnimationClass::FindAnimatorById(const std::string& id) noexcept
{
    for (auto& actuator : mActuators) {
        if (actuator->GetId() == id)
            return actuator.get();
    }
    return nullptr;
}
const AnimatorClass* AnimationClass::FindAnimatorById(const std::string& id) const noexcept
{
    for (auto& actuator : mActuators) {
        if (actuator->GetId() == id)
            return actuator.get();
    }
    return nullptr;
}

std::unique_ptr<Animator> AnimationClass::CreateAnimatorInstance(std::size_t index) const
{
    const auto& klass = mActuators[index];
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
    else BUG("Unknown actuator type");
    return {};
}

std::size_t AnimationClass::GetHash() const noexcept
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mLooping);
    hash = base::hash_combine(hash, mDelay);
    for (const auto& actuator : mActuators)
        hash = base::hash_combine(hash, actuator->GetHash());
    return hash;
}

void AnimationClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("duration", mDuration);
    data.Write("delay", mDelay);
    data.Write("looping", mLooping);
    for (const auto &actuator : mActuators)
    {
        auto meta = data.NewWriteChunk();
        auto act  = data.NewWriteChunk();
        actuator->IntoJson(*act);
        meta->Write("type", actuator->GetType());
        meta->Write("actuator", std::move(act));
        data.AppendChunk("actuators", std::move(meta));
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

    for (unsigned i=0; i<data.GetNumChunks("actuators"); ++i)
    {
        AnimatorClass::Type type;
        const auto& meta_chunk = data.GetReadChunk("actuators", i);
        const auto& data_chunk = meta_chunk->GetReadChunk("actuator");
        if (data_chunk && meta_chunk->Read("type", &type))
        {
            std::shared_ptr<AnimatorClass> actuator;
            if (type == AnimatorClass::Type::TransformAnimator)
                actuator = std::make_shared<TransformAnimatorClass>();
            else if (type == AnimatorClass::Type::PropertyAnimator)
                actuator = std::make_shared<PropertyAnimatorClass>();
            else if (type == AnimatorClass::Type::KinematicAnimator)
                actuator = std::make_shared<KinematicAnimatorClass>();
            else if (type == AnimatorClass::Type::BooleanPropertyAnimator)
                actuator = std::make_shared<BooleanPropertyAnimatorClass>();
            else if (type == AnimatorClass::Type::MaterialAnimator)
                actuator = std::make_shared<MaterialAnimatorClass>();
            else BUG("Unknown actuator type.");

            mActuators.push_back(actuator);
            if (actuator->FromJson(*data_chunk))
                continue;

            WARN("Animation actuator failed to load completely. [animation='%1']", mName);
        }
        else if (!data_chunk)
            WARN("Missing actuator data chunk. [animation='%1']", mName);
        else WARN("Unrecognized animation actuator type. [animation='%1']", mName);

        ok = false;
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
    for (const auto& klass : mActuators)
        ret.mActuators.push_back(klass->Clone());
    return ret;
}

AnimationClass& AnimationClass::operator=(const AnimationClass& other)
{
    if (this == &other)
        return *this;
    AnimationClass copy(other);
    std::swap(mId, copy.mId);
    std::swap(mActuators, copy.mActuators);
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
        NodeTrack track;
        track.actuator = mClass->CreateAnimatorInstance(i);
        track.node     = track.actuator->GetNodeId();
        track.ended    = false;
        track.started  = false;
        mTracks.push_back(std::move(track));
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
    for (size_t i=0; i<other.mTracks.size(); ++i)
    {
        NodeTrack track;
        track.node     = other.mTracks[i].node;
        track.actuator = other.mTracks[i].actuator->Copy();
        track.ended    = other.mTracks[i].ended;
        track.started  = other.mTracks[i].started;
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
    const auto pos = mCurrentTime / duration;

    // todo: keep the tracks in some smarter data structure or perhaps
    // in a sorted vector and then binary search.
    for (auto& track : mTracks)
    {
        if (track.node != node.GetClassId())
            continue;

        const auto start = track.actuator->GetStartTime();
        const auto len   = track.actuator->GetDuration();
        const auto end   = math::clamp(0.0f, 1.0f, start + len);
        if (pos < start)
            continue;
        else if (pos >= end)
        {
            if (!track.ended)
            {
                track.actuator->Finish(node);
                track.ended = true;
            }
            continue;
        }
        if (!track.started)
        {
            track.actuator->Start(node);
            track.started = true;
        }
        const auto t = math::clamp(0.0f, 1.0f, (pos - start) / len);
        track.actuator->Apply(node, t);
    }
}

void Animation::Restart() noexcept
{
    for (auto& track : mTracks)
    {
        ASSERT(track.started);
        ASSERT(track.ended);
        track.started = false;
        track.ended   = false;
    }
    mCurrentTime = -mDelay;
}

bool Animation::IsComplete() const noexcept
{
    for (const auto& track : mTracks)
    {
        if (!track.ended)
            return false;
    }
    if (mCurrentTime >= mClass->GetDuration())
        return true;
    return false;
}

Animator* Animation::FindActuatorById(const std::string& id) noexcept
{
    for (auto& item : mTracks)
    {
        if (item.actuator->GetClassId() == id)
            return item.actuator.get();
    }
    return nullptr;
}
Animator* Animation::FindActuatorByName(const std::string& name) noexcept
{
    for (auto& item : mTracks)
    {
        if (item.actuator->GetClassName() == name)
            return item.actuator.get();
    }
    return nullptr;
}

const Animator* Animation::FindActuatorById(const std::string& id) const noexcept
{
    for (auto& item : mTracks)
    {
        if (item.actuator->GetClassId() == id)
            return item.actuator.get();
    }
    return nullptr;
}
const Animator* Animation::FindActuatorByName(const std::string& name) const noexcept
{
    for (auto& item : mTracks)
    {
        if (item.actuator->GetClassName() == name)
            return item.actuator.get();
    }
    return nullptr;
}

std::unique_ptr<Animation> CreateAnimationInstance(const std::shared_ptr<const AnimationClass>& klass)
{
    return std::make_unique<Animation>(klass);
}

} // namespace
