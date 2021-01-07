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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <cmath>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "gamelib/animation.h"
#include "gamelib/entity.h"

namespace game
{

size_t MaterialActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndAlpha);
    return hash;
}

nlohmann::json MaterialActuatorClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mId);
    base::JsonWrite(json, "node", mNodeId);
    base::JsonWrite(json, "method", mInterpolation);
    base::JsonWrite(json, "starttime", mStartTime);
    base::JsonWrite(json, "duration", mDuration);
    base::JsonWrite(json, "alpha", mEndAlpha);
    return json;
}

bool MaterialActuatorClass::FromJson(const nlohmann::json &json)
{
    return base::JsonReadSafe(json, "id", &mId) &&
           base::JsonReadSafe(json, "node", &mNodeId) &&
           base::JsonReadSafe(json, "method", &mInterpolation) &&
           base::JsonReadSafe(json, "starttime", &mStartTime) &&
           base::JsonReadSafe(json, "duration", &mDuration) &&
           base::JsonReadSafe(json, "alpha", &mEndAlpha);
}

nlohmann::json TransformActuatorClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mId);
    base::JsonWrite(json, "node",      mNodeId);
    base::JsonWrite(json, "method",    mInterpolation);
    base::JsonWrite(json, "starttime", mStartTime);
    base::JsonWrite(json, "duration",  mDuration);
    base::JsonWrite(json, "position",  mEndPosition);
    base::JsonWrite(json, "size",      mEndSize);
    base::JsonWrite(json, "scale",     mEndScale);
    base::JsonWrite(json, "rotation",  mEndRotation);
    return json;
}

bool TransformActuatorClass::FromJson(const nlohmann::json& json)
{
    return base::JsonReadSafe(json, "id", &mId) &&
           base::JsonReadSafe(json, "node", &mNodeId) &&
           base::JsonReadSafe(json, "starttime", &mStartTime) &&
           base::JsonReadSafe(json, "duration",  &mDuration) &&
           base::JsonReadSafe(json, "position",  &mEndPosition) &&
           base::JsonReadSafe(json, "size",      &mEndSize) &&
           base::JsonReadSafe(json, "scale",     &mEndScale) &&
           base::JsonReadSafe(json, "rotation",  &mEndRotation) &&
           base::JsonReadSafe(json, "method",    &mInterpolation);
}

std::size_t TransformActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndPosition);
    hash = base::hash_combine(hash, mEndSize);
    hash = base::hash_combine(hash, mEndScale);
    hash = base::hash_combine(hash, mEndRotation);
    return hash;
}

void MaterialActuator::Start(EntityNode& node)
{
    if (const auto* draw = node.GetDrawable())
    {
        mStartAlpha = draw->GetAlpha();
        if (!draw->TestFlag(DrawableItemClass::Flags::OverrideAlpha))
        {
            WARN("EntityNode '%1' doesn't set OverrideAlpha flag. ", node.GetName());
            WARN("Material actuator will have no effect.");
        }
    }
    else
    {
        WARN("EntityNode '%1' doesn't have a drawable item.", node.GetName());
        WARN("Material actuator will have no effect.");
    }
}
void MaterialActuator::Apply(EntityNode& node, float t)
{
    if (auto* draw = node.GetDrawable())
    {
        const auto method = mClass->GetInterpolation();
        const float value = math::interpolate(mStartAlpha, mClass->GetEndAlpha(), t, method);
        draw->SetAlpha(value);
    }
}

void MaterialActuator::Finish(EntityNode& node)
{
    if (auto* draw = node.GetDrawable())
    {
        draw->SetAlpha(mClass->GetEndAlpha());
    }
}

void TransformActuator::Start(EntityNode& node)
{
    mStartPosition = node.GetTranslation();
    mStartSize     = node.GetSize();
    mStartScale    = node.GetScale();
    mStartRotation = node.GetRotation();
}
void TransformActuator::Apply(EntityNode& node, float t)
{
    // apply interpolated state on the node.
    const auto method = mClass->GetInterpolation();
    const auto& p = math::interpolate(mStartPosition, mClass->GetEndPosition(), t, method);
    const auto& s = math::interpolate(mStartSize,     mClass->GetEndSize(),     t, method);
    const auto& r = math::interpolate(mStartRotation, mClass->GetEndRotation(), t, method);
    const auto& f = math::interpolate(mStartScale,    mClass->GetEndScale(),    t, method);
    node.SetTranslation(p);
    node.SetSize(s);
    node.SetRotation(r);
    node.SetScale(f);
}
void TransformActuator::Finish(EntityNode& node)
{
    node.SetTranslation(mClass->GetEndPosition());
    node.SetRotation(mClass->GetEndRotation());
    node.SetSize(mClass->GetEndSize());
    node.SetScale(mClass->GetEndScale());
}

AnimationTrackClass::AnimationTrackClass(const AnimationTrackClass& other)
{
    for (const auto& a : other.mActuators)
    {
        mActuators.push_back(a->Copy());
    }
    mId       = other.mId;
    mName     = other.mName;
    mDuration = other.mDuration;
    mLooping  = other.mLooping;
}
AnimationTrackClass::AnimationTrackClass(AnimationTrackClass&& other)
{
    mId        = std::move(other.mId);
    mActuators = std::move(other.mActuators);
    mName      = std::move(other.mName);
    mDuration  = other.mDuration;
    mLooping   = other.mLooping;
}

void AnimationTrackClass::DeleteActuator(size_t index)
{
    ASSERT(index < mActuators.size());
    auto it = mActuators.begin();
    std::advance(it, index);
    mActuators.erase(it);
}

bool AnimationTrackClass::DeleteActuatorById(const std::string& id)
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
AnimationActuatorClass* AnimationTrackClass::FindActuatorById(const std::string& id)
{
    for (auto& actuator : mActuators) {
        if (actuator->GetId() == id)
            return actuator.get();
    }
    return nullptr;
}
const AnimationActuatorClass* AnimationTrackClass::FindActuatorById(const std::string& id) const
{
    for (auto& actuator : mActuators) {
        if (actuator->GetId() == id)
            return actuator.get();
    }
    return nullptr;
}

std::unique_ptr<AnimationActuator> AnimationTrackClass::CreateActuatorInstance(size_t i) const
{
    const auto& klass = mActuators[i];
    if (klass->GetType() == AnimationActuatorClass::Type::Transform)
        return std::make_unique<TransformActuator>(std::static_pointer_cast<TransformActuatorClass>(klass));
    else if (klass->GetType() == AnimationActuatorClass::Type::Material)
        return std::make_unique<MaterialActuator>(std::static_pointer_cast<MaterialActuatorClass>(klass));
    BUG("Unknown actuator type");
    return {};
}

std::size_t AnimationTrackClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mLooping);
    for (const auto& actuator : mActuators)
        hash = base::hash_combine(hash, actuator->GetHash());
    return hash;
}

nlohmann::json AnimationTrackClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mId);
    base::JsonWrite(json, "name", mName);
    base::JsonWrite(json, "duration", mDuration);
    base::JsonWrite(json, "looping", mLooping);
    for (const auto &actuator : mActuators)
    {
        nlohmann::json js;
        base::JsonWrite(js, "type", actuator->GetType());
        base::JsonWrite(js, "actuator", *actuator);
        json["actuators"].push_back(std::move(js));
    }
    return json;
}

// static
std::optional<AnimationTrackClass> AnimationTrackClass::FromJson(const nlohmann::json& json)
{
    AnimationTrackClass ret;
    if (!base::JsonReadSafe(json, "id", &ret.mId) ||
        !base::JsonReadSafe(json, "name", &ret.mName) ||
        !base::JsonReadSafe(json, "duration", &ret.mDuration) ||
        !base::JsonReadSafe(json, "looping", &ret.mLooping))
        return std::nullopt;
    if (!json.contains("actuators"))
        return ret;
    for (const auto& json_actuator : json["actuators"].items())
    {
        const auto& obj = json_actuator.value();
        AnimationActuatorClass::Type type;
        if (!base::JsonReadSafe(obj, "type", &type))
            return std::nullopt;
        std::shared_ptr<AnimationActuatorClass> actuator;
        if (type == AnimationActuatorClass::Type::Transform)
            actuator = std::make_shared<TransformActuatorClass>();
        else if (type == AnimationActuatorClass::Type::Material)
            actuator = std::make_shared<MaterialActuatorClass>();
        else BUG("Unknown actuator type.");

        if (!actuator->FromJson(obj["actuator"]))
            return std::nullopt;
        ret.mActuators.push_back(actuator);
    }
    return ret;
}

AnimationTrackClass AnimationTrackClass::Clone() const
{
    AnimationTrackClass ret;
    ret.mName     = mName;
    ret.mDuration = mDuration;
    ret.mLooping  = mLooping;
    for (const auto& klass : mActuators)
        ret.mActuators.push_back(klass->Clone());
    return ret;
}

AnimationTrackClass& AnimationTrackClass::operator=(const AnimationTrackClass& other)
{
    if (this == &other)
        return *this;
    AnimationTrackClass copy(other);
    std::swap(mId, copy.mId);
    std::swap(mActuators, copy.mActuators);
    std::swap(mName, copy.mName);
    std::swap(mDuration, copy.mDuration);
    std::swap(mLooping, copy.mLooping);
    return *this;
}

AnimationTrack::AnimationTrack(const std::shared_ptr<const AnimationTrackClass>& klass)
    : mClass(klass)
{
    for (size_t i=0; i<mClass->GetNumActuators(); ++i)
    {
        NodeTrack track;
        track.actuator = mClass->CreateActuatorInstance(i);
        track.node     = track.actuator->GetNodeId();
        track.ended    = false;
        track.started  = false;
        mTracks.push_back(std::move(track));
    }
}
AnimationTrack::AnimationTrack(const AnimationTrackClass& klass)
    : AnimationTrack(std::make_shared<AnimationTrackClass>(klass))
    {}

AnimationTrack::AnimationTrack(const AnimationTrack& other) : mClass(other.mClass)
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
}
    // Move ctor.
AnimationTrack::AnimationTrack(AnimationTrack&& other)
{
    mClass       = other.mClass;
    mTracks      = std::move(other.mTracks);
    mCurrentTime = other.mCurrentTime;
}

void AnimationTrack::Update(float dt)
{
    const auto duration = mClass->GetDuration();

    mCurrentTime = math::clamp(0.0f, duration, mCurrentTime + dt);
}

void AnimationTrack::Apply(EntityNode& node) const
{
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

void AnimationTrack::Restart()
{
    for (auto& track : mTracks)
    {
        ASSERT(track.started);
        ASSERT(track.ended);
        track.started = false;
        track.ended   = false;
    }
    mCurrentTime = 0.0f;
}

bool AnimationTrack::IsComplete() const
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

std::unique_ptr<AnimationTrack> CreateAnimationTrackInstance(std::shared_ptr<const AnimationTrackClass> klass)
{
    return std::make_unique<AnimationTrack>(klass);
}

} // namespace
