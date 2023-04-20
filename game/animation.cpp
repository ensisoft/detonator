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

void AnimationClass::DeleteActuator(size_t index) noexcept
{
    ASSERT(index < mActuators.size());
    auto it = mActuators.begin();
    std::advance(it, index);
    mActuators.erase(it);
}

bool AnimationClass::DeleteActuatorById(const std::string& id) noexcept
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
ActuatorClass* AnimationClass::FindActuatorById(const std::string& id) noexcept
{
    for (auto& actuator : mActuators) {
        if (actuator->GetId() == id)
            return actuator.get();
    }
    return nullptr;
}
const ActuatorClass* AnimationClass::FindActuatorById(const std::string& id) const noexcept
{
    for (auto& actuator : mActuators) {
        if (actuator->GetId() == id)
            return actuator.get();
    }
    return nullptr;
}

std::unique_ptr<Actuator> AnimationClass::CreateActuatorInstance(size_t index) const
{
    const auto& klass = mActuators[index];
    if (klass->GetType() == ActuatorClass::Type::Transform)
        return std::make_unique<TransformActuator>(std::static_pointer_cast<TransformActuatorClass>(klass));
    else if (klass->GetType() == ActuatorClass::Type::SetValue)
        return std::make_unique<SetValueActuator>(std::static_pointer_cast<SetValueActuatorClass>(klass));
    else if (klass->GetType() == ActuatorClass::Type::Kinematic)
        return std::make_unique<KinematicActuator>(std::static_pointer_cast<KinematicActuatorClass>(klass));
    else if (klass->GetType() == ActuatorClass::Type::SetFlag)
        return std::make_unique<SetFlagActuator>(std::static_pointer_cast<SetFlagActuatorClass>(klass));
    else if (klass->GetType() == ActuatorClass::Type::Material)
        return std::make_unique<MaterialActuator>(std::static_pointer_cast<MaterialActuatorClass>(klass));
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
        ActuatorClass::Type type;
        const auto& meta_chunk = data.GetReadChunk("actuators", i);
        const auto& data_chunk = meta_chunk->GetReadChunk("actuator");
        if (data_chunk && meta_chunk->Read("type", &type))
        {
            std::shared_ptr<ActuatorClass> actuator;
            if (type == ActuatorClass::Type::Transform)
                actuator = std::make_shared<TransformActuatorClass>();
            else if (type == ActuatorClass::Type::SetValue)
                actuator = std::make_shared<SetValueActuatorClass>();
            else if (type == ActuatorClass::Type::Kinematic)
                actuator = std::make_shared<KinematicActuatorClass>();
            else if (type == ActuatorClass::Type::SetFlag)
                actuator = std::make_shared<SetFlagActuatorClass>();
            else if (type == ActuatorClass::Type::Material)
                actuator = std::make_shared<MaterialActuatorClass>();
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
    for (size_t i=0; i<mClass->GetNumActuators(); ++i)
    {
        NodeTrack track;
        track.actuator = mClass->CreateActuatorInstance(i);
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

Actuator* Animation::FindActuatorById(const std::string& id) noexcept
{
    for (auto& item : mTracks)
    {
        if (item.actuator->GetClassId() == id)
            return item.actuator.get();
    }
    return nullptr;
}
Actuator* Animation::FindActuatorByName(const std::string& name) noexcept
{
    for (auto& item : mTracks)
    {
        if (item.actuator->GetClassName() == name)
            return item.actuator.get();
    }
    return nullptr;
}

const Actuator* Animation::FindActuatorById(const std::string& id) const noexcept
{
    for (auto& item : mTracks)
    {
        if (item.actuator->GetClassId() == id)
            return item.actuator.get();
    }
    return nullptr;
}
const Actuator* Animation::FindActuatorByName(const std::string& name) const noexcept
{
    for (auto& item : mTracks)
    {
        if (item.actuator->GetClassName() == name)
            return item.actuator.get();
    }
    return nullptr;
}

AnimationStateClass::AnimationStateClass(std::string id)
  : mId(std::move(id))
{}

std::size_t AnimationStateClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mId);
    return hash;
}

void AnimationStateClass::IntoJson(data::Writer& data) const
{
    data.Write("name", mName);
    data.Write("id", mId);
}
bool AnimationStateClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);
    return ok;
}

AnimationStateClass AnimationStateClass::Clone() const
{
    AnimationStateClass dolly(base::RandomString(10));
    dolly.mName = mName;
    return dolly;
}

AnimationStateTransitionClass::AnimationStateTransitionClass(std::string id)
  : mId(std::move(id))
{}

std::size_t AnimationStateTransitionClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mDstStateId);
    hash = base::hash_combine(hash, mSrcStateId);
    hash = base::hash_combine(hash, mDuration);
    return hash;
}

void AnimationStateTransitionClass::IntoJson(data::Writer& data) const
{
    data.Write("name", mName);
    data.Write("id", mId);
    data.Write("src_state_id", mSrcStateId);
    data.Write("dst_state_id", mDstStateId);
    data.Write("duration", mDuration);
}
bool AnimationStateTransitionClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);
    ok &= data.Read("src_state_id", &mSrcStateId);
    ok &= data.Read("dst_state_id", &mDstStateId);
    ok &= data.Read("duration", &mDuration);
    return ok;
}

AnimationStateTransitionClass AnimationStateTransitionClass::Clone() const
{
    AnimationStateTransitionClass dolly(base::RandomString(10));
    dolly.mName = mName;
    dolly.mDstStateId = mDstStateId;
    dolly.mSrcStateId = mSrcStateId;
    dolly.mDuration   = mDuration;
    return dolly;
}

AnimatorClass::AnimatorClass(std::string id)
  : mId(std::move(id))
{}

const AnimationStateClass* AnimatorClass::FindStateById(const std::string& id) const noexcept
{
    return base::SafeFind(mStates, [&id](const auto& state) {
        return state.GetId() == id;
    });
}
const AnimationStateClass* AnimatorClass::FindStateByName(const std::string& name) const noexcept
{
    return base::SafeFind(mStates, [&name](const auto& state) {
        return state.GetName() == name;
    });
}
const AnimationStateTransitionClass* AnimatorClass::FindTransitionByName(const std::string& name) const noexcept
{
    return base::SafeFind(mTransitions, [&name](const auto& trans) {
        return trans.GetName() == name;
    });
}
const AnimationStateTransitionClass* AnimatorClass::FindTransitionById(const std::string& id) const noexcept
{
    return base::SafeFind(mTransitions, [&id](const auto& trans) {
        return trans.GetId() == id;
    });
}

AnimationStateClass* AnimatorClass::FindStateById(const std::string& id) noexcept
{
    return base::SafeFind(mStates, [&id](const auto& state) {
        return state.GetId() == id;
    });
}
AnimationStateClass* AnimatorClass::FindStateByName(const std::string& name) noexcept
{
    return base::SafeFind(mStates, [&name](const auto& state) {
        return state.GetName() == name;
    });
}
AnimationStateTransitionClass* AnimatorClass::FindTransitionByName(const std::string& name) noexcept
{
    return base::SafeFind(mTransitions, [&name](const auto& trans) {
        return trans.GetName() == name;
    });
}
AnimationStateTransitionClass* AnimatorClass::FindTransitionById(const std::string& id) noexcept
{
    return base::SafeFind(mTransitions, [&id](const auto& trans) {
        return trans.GetId() == id;
    });
}

std::size_t AnimatorClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mScriptId);
    hash = base::hash_combine(hash, mInitState);

    for (const auto& state : mStates)
    {
        hash = base::hash_combine(hash, state.GetHash());
    }
    for (const auto& transition : mTransitions)
    {
        hash = base::hash_combine(hash, transition.GetHash());
    }
    return hash;
}

void AnimatorClass::IntoJson(data::Writer& data) const
{
    data.Write("name", mName);
    data.Write("id", mId);
    data.Write("initial_state", mInitState);
    data.Write("script_id", mScriptId);

    for (const auto& state : mStates)
    {
        auto chunk = data.NewWriteChunk();
        state.IntoJson(*chunk);
        data.AppendChunk("states", std::move(chunk));
    }
    for (const auto& transition : mTransitions)
    {
        auto chunk = data.NewWriteChunk();
        transition.IntoJson(*chunk);
        data.AppendChunk("transitions", std::move(chunk));
    }
}
bool AnimatorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);
    ok &= data.Read("initial_state", &mInitState);
    ok &= data.Read("script_id", &mScriptId);

    for (unsigned i=0; i<data.GetNumChunks("states"); ++i)
    {
        const auto& chunk = data.GetReadChunk("states", i);
        AnimationStateClass state;
        ok &= state.FromJson(*chunk);
        mStates.push_back(std::move(state));
    }
    for (unsigned i=0; i<data.GetNumChunks("transitions"); ++i)
    {
        const auto& chunk = data.GetReadChunk("transitions", i);
        AnimationStateTransitionClass transition;
        ok &= transition.FromJson(*chunk);
        mTransitions.push_back(std::move(transition));
    }
    return ok;
}

AnimatorClass AnimatorClass::Clone() const
{
    AnimatorClass dolly;
    dolly.mId = base::RandomString(10);
    dolly.mName = mName;
    dolly.mScriptId = mScriptId;

    std::unordered_map<std::string, std::string> state_map;
    for (const auto& state : mStates)
    {
        auto state_dolly = state.Clone();
        state_map[state.GetId()] = state_dolly.GetId();
        dolly.mStates.push_back(std::move(state_dolly));
    }
    for (const auto& link : mTransitions)
    {
        auto link_dolly = link.Clone();
        const auto src_state_old_id = link_dolly.GetSrcStateId();
        const auto dst_state_old_id = link_dolly.GetDstStateId();
        const auto src_state_new_id = state_map[src_state_old_id];
        const auto dst_state_new_id = state_map[dst_state_old_id];
        link_dolly.SetDstStateId(dst_state_new_id);
        link_dolly.SetSrcStateId(src_state_new_id);
        dolly.mTransitions.push_back(std::move(link_dolly));
    }

    dolly.mInitState = state_map[mInitState];
    return dolly;
}

Animator::Animator(const std::shared_ptr<const AnimatorClass>& klass)
  : mClass(klass)
{}

Animator::Animator(const AnimatorClass& klass)
  : Animator(std::make_shared<AnimatorClass>(klass))
{}

Animator::Animator(AnimatorClass&& klass)
  : Animator(std::make_shared<AnimatorClass>(std::move(klass)))
{}

void Animator::Update(float dt, std::vector<Action>* actions)
{
    if (!mCurrent && !mTransition)
    {
        mCurrent = mClass->FindStateById(mClass->GetInitialStateId());
        if (!mCurrent)
            return;

        actions->push_back( EnterState { mCurrent } );
    }

    if (mTransition)
    {
        // this is safe because we explicitly set to 0.0 on start of transition.
        if (mTime == 0.0f)
        {
            actions->push_back( LeaveState { mPrev } );
            actions->push_back( StartTransition { mPrev, mNext, mTransition } );
        }

        const auto transition_time = mTransition->GetDuration();
        if (mTime + dt >= transition_time)
        {
            dt = transition_time - mTime;
            actions->push_back( UpdateTransition { mPrev, mNext, mTransition, mTime, dt });
            actions->push_back( FinishTransition { mPrev, mNext, mTransition } );
            actions->push_back( EnterState { mNext } );
            mCurrent = mNext;
            mTime = 0.0f;
            mNext = nullptr;
            mPrev = nullptr;
            mTransition = nullptr;
        }
        else
        {
            actions->push_back( UpdateTransition { mPrev, mNext, mTransition, mTime, dt });
            mTime += dt;
        }
        return;
    }

    actions->push_back( UpdateState { mCurrent, mTime, dt } );
    // update the current state time, i.e. how long have been at this state.
    mTime += dt;

    for (size_t i=0; i<mClass->GetNumTransitions(); ++i)
    {
        const auto& transition = mClass->GetTransition(i);
        if (transition.GetSrcStateId() != mCurrent->GetId())
            continue;
        const auto* next = mClass->FindStateById(transition.GetDstStateId());
        actions->push_back( EvalTransition { mCurrent, next, &transition });
    }
}
void Animator::Update(const AnimationTransition* transition, const AnimationState* next)
{
    mTime       = 0.0f;
    mTransition = transition;
    mNext       = next;
    mPrev       = mCurrent;
    mCurrent    = nullptr;
}

Animator::State Animator::GetAnimatorState() const noexcept
{
    if (mTransition)
        return State::InTransition;
    return State::InState;
}

std::unique_ptr<Animation> CreateAnimationInstance(const std::shared_ptr<const AnimationClass>& klass)
{
    return std::make_unique<Animation>(klass);
}

std::unique_ptr<Animator> CreateAnimatorInstance(const std::shared_ptr<const AnimatorClass>& klass)
{
    return std::make_unique<Animator>(klass);
}

std::unique_ptr<Animator> CreateAnimatorInstance(const AnimatorClass& klass)
{
    return std::make_unique<Animator>(klass);
}
std::unique_ptr<Animator> CreateAnimatorInstance(AnimatorClass&& klass)
{
    return std::make_unique<Animator>(std::move(klass));
}

} // namespace
