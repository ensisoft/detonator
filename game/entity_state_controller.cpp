// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/entity_state_controller.h"

namespace game
{

EntityStateControllerClass::EntityStateControllerClass(std::string id)
  : mId(std::move(id))
{}

void EntityStateControllerClass::DeleteTransitionById(const std::string& id)
{
    base::EraseRemove(mTransitions, [&id](const auto& trans) {
        return trans.GetId() == id;
    });
}

void EntityStateControllerClass::DeleteStateById(const std::string& id)
{
    base::EraseRemove(mStates, [&id](const auto& state) {
        return state.GetId() == id;
    });
    base::EraseRemove(mTransitions, [&id](const auto& trans) {
        return trans.GetDstStateId() == id ||
               trans.GetSrcStateId() == id;
    });
}

const EntityStateClass* EntityStateControllerClass::FindStateById(const std::string& id) const noexcept
{
    return base::SafeFind(mStates, [&id](const auto& state) {
        return state.GetId() == id;
    });
}
const EntityStateClass* EntityStateControllerClass::FindStateByName(const std::string& name) const noexcept
{
    return base::SafeFind(mStates, [&name](const auto& state) {
        return state.GetName() == name;
    });
}
const EntityStateTransitionClass* EntityStateControllerClass::FindTransitionByName(const std::string& name) const noexcept
{
    return base::SafeFind(mTransitions, [&name](const auto& trans) {
        return trans.GetName() == name;
    });
}
const EntityStateTransitionClass* EntityStateControllerClass::FindTransitionById(const std::string& id) const noexcept
{
    return base::SafeFind(mTransitions, [&id](const auto& trans) {
        return trans.GetId() == id;
    });
}

EntityStateClass* EntityStateControllerClass::FindStateById(const std::string& id) noexcept
{
    return base::SafeFind(mStates, [&id](const auto& state) {
        return state.GetId() == id;
    });
}
EntityStateClass* EntityStateControllerClass::FindStateByName(const std::string& name) noexcept
{
    return base::SafeFind(mStates, [&name](const auto& state) {
        return state.GetName() == name;
    });
}
EntityStateTransitionClass* EntityStateControllerClass::FindTransitionByName(const std::string& name) noexcept
{
    return base::SafeFind(mTransitions, [&name](const auto& trans) {
        return trans.GetName() == name;
    });
}
EntityStateTransitionClass* EntityStateControllerClass::FindTransitionById(const std::string& id) noexcept
{
    return base::SafeFind(mTransitions, [&id](const auto& trans) {
        return trans.GetId() == id;
    });
}

std::size_t EntityStateControllerClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mScriptId);
    hash = base::hash_combine(hash, mInitState);
    hash = base::hash_combine(hash, mTransitionMode);

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

void EntityStateControllerClass::IntoJson(data::Writer& data) const
{
    data.Write("name", mName);
    data.Write("id", mId);
    data.Write("initial_state", mInitState);
    data.Write("script_id", mScriptId);
    data.Write("transition_mode", mTransitionMode);

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
bool EntityStateControllerClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);
    ok &= data.Read("initial_state", &mInitState);
    ok &= data.Read("script_id", &mScriptId);
    ok &= data.Read("transition_mode", &mTransitionMode);

    for (unsigned i=0; i<data.GetNumChunks("states"); ++i)
    {
        const auto& chunk = data.GetReadChunk("states", i);
        EntityStateClass state;
        ok &= state.FromJson(*chunk);
        mStates.push_back(std::move(state));
    }
    for (unsigned i=0; i<data.GetNumChunks("transitions"); ++i)
    {
        const auto& chunk = data.GetReadChunk("transitions", i);
        EntityStateTransitionClass transition;
        ok &= transition.FromJson(*chunk);
        mTransitions.push_back(std::move(transition));
    }
    return ok;
}

EntityStateControllerClass EntityStateControllerClass::Clone() const
{
    EntityStateControllerClass dolly;
    dolly.mId = base::RandomString(10);
    dolly.mName = mName;
    dolly.mScriptId = mScriptId;
    dolly.mTransitionMode = mTransitionMode;

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

EntityStateController::EntityStateController(std::shared_ptr<const EntityStateControllerClass> klass) noexcept
  : mClass(std::move(klass))
{}

EntityStateController::EntityStateController(const EntityStateControllerClass& klass)
  : EntityStateController(std::make_shared<EntityStateControllerClass>(klass))
{}

EntityStateController::EntityStateController(EntityStateControllerClass&& klass)
  : EntityStateController(std::make_shared<EntityStateControllerClass>(std::move(klass)))
{}

void EntityStateController::Update(float dt, std::vector<StateUpdate>* updates)
{
    if (!mCurrent && !mTransition)
    {
        mCurrent = mClass->FindStateById(mClass->GetInitialStateId());
        if (!mCurrent)
            return;

        updates->emplace_back( EnterState { mCurrent } );
    }

    if (mTransition)
    {
        // this is safe because we explicitly set to 0.0 on start of transition.
        if (mTime == 0.0f)
        {
            ASSERT(mPrev);
            ASSERT(mNext);
            updates->emplace_back( LeaveState { mPrev } );
            updates->emplace_back( StartTransition { mPrev, mNext, mTransition } );
        }

        const auto transition_time = mTransition->GetDuration();
        if (mTime + dt >= transition_time)
        {
            dt = transition_time - mTime;
            updates->emplace_back( UpdateTransition { mPrev, mNext, mTransition, mTime, dt });
            updates->emplace_back( FinishTransition { mPrev, mNext, mTransition } );
            updates->emplace_back( EnterState { mNext } );
            mCurrent = mNext;
            mTime = 0.0f;
            mNext = nullptr;
            mPrev = nullptr;
            mTransition = nullptr;
        }
        else
        {
            updates->emplace_back( UpdateTransition { mPrev, mNext, mTransition, mTime, dt });
            mTime += dt;
        }
        return;
    }

    updates->emplace_back( UpdateState { mCurrent, mTime, dt } );
    // update the current state time, i.e. how long have been at this state.
    mTime += dt;

    // if we're currently in transition then another transition
    // cannot be started (and no point to evaluate) so return early.
    if (mTransition)
        return;

    const auto state_transition_mode = mClass->GetTransitionMode();
    if (state_transition_mode == StateTransitionMode::OnTrigger && !mTriggerTransitionEvaluation)
        return;

    mTriggerTransitionEvaluation = false;

    for (size_t i=0; i<mClass->GetNumTransitions(); ++i)
    {
        const auto& transition = mClass->GetTransition(i);
        if (transition.GetSrcStateId() != mCurrent->GetId())
            continue;
        const auto* next = mClass->FindStateById(transition.GetDstStateId());
        updates->emplace_back( EvalTransition { mCurrent, next, &transition });
    }
}
bool EntityStateController::BeginStateTransition(const EntityStateTransition* transition, const EntityState* next)
{
    ASSERT(transition);
    ASSERT(next);

    // already have a pending transition ?
    if (mTransition)
        return false;

    ASSERT(mCurrent);

    mTime       = 0.0f;
    mTransition = transition;
    mNext       = next;
    mPrev       = mCurrent;
    mCurrent    = nullptr;
    return true;
}

EntityStateController::State EntityStateController::GetControllerState() const noexcept
{
    if (mTransition)
        return State::InTransition;
    return State::InState;
}


std::unique_ptr<EntityStateController> CreateStateControllerInstance(const std::shared_ptr<const EntityStateControllerClass>& klass)
{
    return std::make_unique<EntityStateController>(klass);
}

std::unique_ptr<EntityStateController> CreateStateControllerInstance(const EntityStateControllerClass& klass)
{
    return std::make_unique<EntityStateController>(klass);
}
std::unique_ptr<EntityStateController> CreateStateControllerInstance(EntityStateControllerClass&& klass)
{
    return std::make_unique<EntityStateController>(std::move(klass));
}
} // namespace