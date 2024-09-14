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
    for (const auto& a : other.mAnimators)
    {
        mAnimators.push_back(a->Copy());
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
    return ret;
}

AnimationClass& AnimationClass::operator=(const AnimationClass& other)
{
    if (this == &other)
        return *this;
    AnimationClass copy(other);
    std::swap(mId, copy.mId);
    std::swap(mAnimators, copy.mAnimators);
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
        AnimatorState track;
        track.animator = mClass->CreateAnimatorInstance(i);
        track.node     = track.animator->GetNodeId();
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
        AnimatorState track;
        track.node     = other.mTracks[i].node;
        track.animator = other.mTracks[i].animator->Copy();
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

        const auto start = track.animator->GetStartTime();
        const auto len   = track.animator->GetDuration();
        const auto end   = math::clamp(0.0f, 1.0f, start + len);
        if (pos < start)
            continue;
        else if (pos >= end)
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
        const auto t = math::clamp(0.0f, 1.0f, (pos - start) / len);
        track.animator->Apply(node, t);
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
