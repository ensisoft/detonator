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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include <string>
#include <variant>
#include <memory>

#include "base/utility.h"
#include "data/fwd.h"
#include "game/entity_state.h"

namespace game
{
    class EntityStateControllerClass
    {
    public:
        explicit EntityStateControllerClass(std::string id = base::RandomString(10));

        void AddState(const EntityStateClass& state)
        { mStates.push_back(state); }
        void AddState(EntityStateClass&& state)
        { mStates.push_back(std::move(state)); }
        void AddTransition(const EntityStateTransitionClass& transition)
        { mTransitions.push_back(transition); }
        void AddTransition(EntityStateTransitionClass&& transition)
        { mTransitions.push_back(std::move(transition)); }

        inline void SetInitialStateId(std::string id) noexcept
        { mInitState = std::move(id); }
        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline void SetScriptId(std::string id) noexcept
        { mScriptId = id; }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }
        inline std::string GetInitialStateId() const noexcept
        { return mInitState; }
        inline std::string GetScriptId() const noexcept
        { return mScriptId; }
        inline bool HasScriptId() const noexcept
        { return !mScriptId.empty(); }

        inline std::size_t GetNumStates() const noexcept
        { return mStates.size(); }
        inline std::size_t GetNumTransitions() const noexcept
        { return mTransitions.size(); }
        inline const EntityStateClass& GetState(size_t index) const noexcept
        { return base::SafeIndex(mStates, index); }
        inline const EntityStateTransitionClass& GetTransition(size_t index) const noexcept
        { return base::SafeIndex(mTransitions, index); }
        inline EntityStateClass& GetState(size_t index) noexcept
        { return base::SafeIndex(mStates, index); }
        inline EntityStateTransitionClass& GetTransition(size_t index) noexcept
        { return base::SafeIndex(mTransitions, index); }
        inline void ClearStates() noexcept
        { mStates.clear(); }
        inline void ClearTransitions() noexcept
        { mTransitions.clear(); }

        void DeleteTransitionById(const std::string& id);
        void DeleteStateById(const std::string& id);
        const EntityStateClass* FindStateById(const std::string& id) const noexcept;
        const EntityStateClass* FindStateByName(const std::string& name) const noexcept;
        const EntityStateTransitionClass* FindTransitionByName(const std::string& name) const noexcept;
        const EntityStateTransitionClass* FindTransitionById(const std::string& id) const noexcept;

        EntityStateClass* FindStateById(const std::string& id) noexcept;
        EntityStateClass* FindStateByName(const std::string& name) noexcept;
        EntityStateTransitionClass* FindTransitionByName(const std::string& name) noexcept;
        EntityStateTransitionClass* FindTransitionById(const std::string& id) noexcept;

        std::size_t GetHash() const noexcept;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        EntityStateControllerClass Clone() const;
    private:
        std::string mName;
        std::string mId;
        std::string mInitState;
        std::string mScriptId;
        std::vector<EntityStateClass> mStates;
        std::vector<EntityStateTransitionClass> mTransitions;
    };

    class EntityStateController
    {
    public:
        struct EnterState {
            const EntityState* state;
        };
        struct LeaveState {
            const EntityState* state;
        };
        struct UpdateState {
            const EntityState* state;
            float time;
            float dt;
        };
        struct StartTransition {
            const EntityState* from;
            const EntityState* to;
            const EntityStateTransition* transition;
        };
        struct FinishTransition {
            const EntityState* from;
            const EntityState* to;
            const EntityStateTransition* transition;
        };
        struct UpdateTransition {
            const EntityState* from;
            const EntityState* to;
            const EntityStateTransition* transition;
            float time;
            float dt;
        };
        struct EvalTransition {
            const EntityState* from;
            const EntityState* to;
            const EntityStateTransition* transition;
        };

        using Action = std::variant<std::monostate,
                EnterState, LeaveState, UpdateState,
                StartTransition, FinishTransition, UpdateTransition, EvalTransition>;

        using Value = std::variant<bool, int, float,
                std::string, glm::vec2>;

        explicit EntityStateController(EntityStateControllerClass&& klass);
        explicit EntityStateController(const EntityStateControllerClass& klass);
        explicit EntityStateController(const std::shared_ptr<const EntityStateControllerClass>& klass);

        // Update the animator state machine state.
        // When the current animator state is updated the resulting actions that should
        // be taken  as a result are stored into the actions vector. The caller can then
        // perform some actions (such as call some evaluation code) or choose do nothing.
        // The only action that should be handled is the response to a state transition
        // evaluation, otherwise no state transitions are possible.
        void Update(float dt, std::vector<Action>* actions);
        // Update the animator state machine by starting a transition from the current
        // state towards the next state.
        void Update(const EntityStateTransition* transition, const EntityState* next);

        enum class State {
            InTransition, InState
        };
        // Get the current animator state state, i.e. whether in state or in transition.
        State GetControllerState() const noexcept;

        // Check whether the animator has a value by the given name or not.
        inline bool HasValue(const std::string& name) const noexcept
        { return base::SafeFind(mValues, name) != nullptr; }
        // Find an animator value by the given name. Returns nullptr if no such value.
        inline const Value* FindValue(const std::string& name) const noexcept
        { return base::SafeFind(mValues, name); }
        // Set an animator value by the given name. Overwrites any previous values if any.
        inline void SetValue(std::string name, Value value) noexcept
        { mValues[std::move(name)] = std::move(value); }
        // Clear all animator values.
        inline void ClearValues() noexcept
        { mValues.clear(); }
        // Get current state object. This only exists when the animator is InState.
        // During transitions there's no current state.
        inline const EntityState* GetCurrentState() const noexcept
        { return mCurrent; }
        // Get the next state (the state we're transitioning to) when doing a transition.
        // The next state only exists during transition.
        inline const EntityState* GetNextState() const noexcept
        { return mNext; }
        // Get the prev state (the state we're transitioning from) when doing a transition.
        // The prev state only exists during transition.
        inline const EntityState* GetPrevState() const noexcept
        { return mPrev; }
        // Get the current transition if any.
        inline const EntityStateTransition* GetTransition() const noexcept
        { return mTransition; }
        // Get the currently accumulated time. If there's a current transition
        // then the time value measures the time elapsed in the transition, other
        // it measures the time spent in some particular state.
        inline float GetTime() const noexcept
        { return mTime; }
        // Class object
        inline const EntityStateControllerClass& GetClass() const
        { return *mClass; }
        inline const EntityStateControllerClass* operator ->() const
        { return mClass.get(); }

        inline std::string GetName() const noexcept
        { return mClass->GetName(); }
        inline std::string GetId() const noexcept
        { return mClass->GetId(); }
    public:
        std::shared_ptr<const EntityStateControllerClass> mClass;
        // Current state if any. no state during transition
        const EntityState* mCurrent = nullptr;
        // previous state when doing a transition (if any)
        const EntityState * mPrev = nullptr;
        // next state when doing a transition (if any)
        const EntityState* mNext = nullptr;
        // the current transition (if any)
        const EntityStateTransition* mTransition = nullptr;
        // the current transition or state time.
        float mTime = 0.0f;
        // some values that can be routed through the animator
        // from one part of the system (the entity script) to another
        // part of the system (the animator script)
        std::unordered_map<std::string, Value> mValues;
    };

    std::unique_ptr<EntityStateController> CreateStateControllerInstance(const std::shared_ptr<const EntityStateControllerClass>& klass);
    std::unique_ptr<EntityStateController> CreateStateControllerInstance(const EntityStateControllerClass& klass);
    std::unique_ptr<EntityStateController> CreateStateControllerInstance(EntityStateControllerClass&& klass);

} // namespace