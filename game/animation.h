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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include <string>
#include <memory>
#include <unordered_map>

#include "base/utility.h"
#include "game/actuator.h"

namespace game
{

    // AnimationClass defines a new type of animation that includes the
    // static state of the animation such as the modified ending results
    // of the nodes involved.
    class AnimationClass
    {
    public:
        AnimationClass();
        // Create a deep copy of the class object.
        AnimationClass(const AnimationClass& other);
        AnimationClass(AnimationClass&& other) noexcept;

        // Set the human-readable name for the animation track.
        inline void SetName(const std::string& name) noexcept
        { mName = name; }
        // Set the animation duration in seconds.
        inline void SetDuration(float duration) noexcept
        { mDuration = duration; }
        // Set animation delay in seconds.
        inline void SetDelay(float delay) noexcept
        { mDelay = delay; }
        // Enable/disable looping flag. A looping animation will never end
        // and will reset after the reaching the end. I.e. all the actuators
        // involved will have their states reset to the initial state which
        // will be re-applied to the node instances. For an animation without
        // any perceived jumps or discontinuity it's important that the animation
        // should transform nodes back to their initial state before the end
        // of the animation track.
        inline void SetLooping(bool looping) noexcept
        { mLooping = looping; }
        // Get the human-readable name of the animation track.
        inline const std::string& GetName() const noexcept
        { return mName; }
        // Get the id of this animation class object.
        inline const std::string& GetId() const noexcept
        { return mId; }
        // Get the normalized duration of the animation track.
        inline float GetDuration() const noexcept
        { return mDuration; }
        // Get the delay in seconds until the animation begins to play.
        inline float GetDelay() const noexcept
        { return mDelay; }
        // Returns true if the animation track is looping or not.
        inline bool IsLooping() const noexcept
        { return mLooping; }

        // Add a new actuator that applies state update/action on some animation node.
        template<typename Actuator>
        void AddActuator(const Actuator& actuator)
        {
            std::shared_ptr<ActuatorClass> foo(new Actuator(actuator));
            mActuators.push_back(std::move(foo));
        }
        // Add a new actuator that applies state update/action on some animation node.
        void AddActuator(std::shared_ptr<ActuatorClass> actuator)
        { mActuators.push_back(std::move(actuator)); }

        // Delete the actuator at the given actuator index.
        void DeleteActuator(std::size_t index) noexcept;
        // Delete the actuator by the given ID. Returns true if actuator was deleted
        // or false to indicate that nothing was done.
        bool DeleteActuatorById(const std::string& id) noexcept;
        // Find an actuator by the given ID. Returns nullptr if no such actuator exists.
        ActuatorClass* FindActuatorById(const std::string& id) noexcept;
        // Find an actuator by the given ID. Returns nullptr if no such actuator exists.
        const ActuatorClass* FindActuatorById(const std::string& id) const noexcept;
        // Clear (and delete) all actuators previously added to the animation.
        inline void Clear() noexcept
        { mActuators.clear(); }
        // Get the number of actuators currently added to this animation track.
        inline std::size_t GetNumActuators() const noexcept
        { return mActuators.size(); }
        // Get the animation actuator class object at the given index.
        inline ActuatorClass& GetActuatorClass(std::size_t index) noexcept
        { return *mActuators[index]; }
        // Get the animation actuator class object at the given index.
        inline const ActuatorClass& GetActuatorClass(std::size_t index) const noexcept
        { return *mActuators[index]; }
        // Create an instance of some actuator class type at the given index.
        // For example if the type of actuator class at index N is
        // TransformActuatorClass then the returned object will be an
        // instance of TransformActuator.
        std::unique_ptr<Actuator> CreateActuatorInstance(std::size_t index) const;
        // Get the hash value based on the static data.
        std::size_t GetHash() const noexcept;
        // Serialize actuator state into JSON.
        void IntoJson(data::Writer& json) const;
        // Load actuator state from JSON.
        // Returns true on success or false to indicate that some data failed to load.
        bool FromJson(const data::Reader& data);
        // Make a complete bitwise clone of this actuator excluding the ID. I.e. create
        // a new animation class object with similar animation state.
        AnimationClass Clone() const;
        // Do a deep copy on the assignment of a new object.
        AnimationClass& operator=(const AnimationClass& other);
    private:
        std::string mId;
        // The list of animation actuators that apply transforms
        std::vector<std::shared_ptr<ActuatorClass>> mActuators;
        // Human-readable name of the animation.
        std::string mName;
        // One time delay before starting the playback.
        float mDelay = 0.0f;
        // the duration of this track.
        float mDuration = 1.0f;
        // Loop animation or not. If looping then never completes.
        bool mLooping = false;
    };


    // Animation is an instance of some type of AnimationClass.
    // It contains the per instance data of the animation track which is
    // modified over time through updates to the track and its actuators states.
    class Animation
    {
    public:
        // Create a new animation instance based on the given class object.
        explicit Animation(const std::shared_ptr<const AnimationClass>& klass);
        // Create a new animation based on the given class object.
        // Makes a copy of the klass object.
        explicit Animation(const AnimationClass& klass);
        // Create a new animation track based on the given class object.
        // Makes a copy of the klass object.
        Animation(const Animation& other);
        // Move ctor.
        Animation(Animation&& other) noexcept;

        // Update the animation track state.
        void Update(float dt) noexcept;
        // Apply animation actions, such as transformations or material changes
        // onto the given entity node.
        void Apply(EntityNode& node) const;
        // Prepare the animation track to restart.
        void Restart() noexcept;
        // Returns true if the animation is complete, i.e. all the
        // actions have been performed.
        bool IsComplete() const noexcept;

        // Find an actuator instance by on its class ID.
        // If there's no such actuator with such class ID then nullptr is returned.
        Actuator* FindActuatorById(const std::string& id) noexcept;
        // Find an actuator instance by its class name.
        // If there's no such actuator with such class ID then nullptr is returned.
        Actuator* FindActuatorByName(const std::string& name) noexcept;
        // Find an actuator instance by on its class ID.
        // If there's no such actuator with such class ID then nullptr is returned.
        const Actuator* FindActuatorById(const std::string& id) const noexcept;
        // Find an actuator instance by its class name.
        // If there's no such actuator with such class ID then nullptr is returned.
        const Actuator* FindActuatorByName(const std::string& name) const noexcept;

        // Set one time animation delay that takes place
        // before the animation starts. The delay is in seconds.
        inline void SetDelay(float delay) noexcept
        { mDelay = delay; }
        // Returns whether the animation is looping or not.
        inline bool IsLooping() const noexcept
        { return mClass->IsLooping(); }
        // Get the human-readable name of the animation track.
        inline const std::string& GetClassName() const noexcept
        { return mClass->GetName(); }
        // Get the animation class ID.
        inline const std::string& GetClassId() const noexcept
        { return mClass->GetId(); }
        // Get the current animation (aka local) time. The time is accumulated
        // on each call to Update(dt).
        inline float GetCurrentTime() const noexcept
        { return mCurrentTime; }
        // Get the delay that must be spent before the animation begins to play.
        inline float GetDelay() const noexcept
        { return mDelay; }
        // Get the animation clip duration in seconds.
        inline float GetDuration() const noexcept
        { return mClass->GetDuration(); }
        // Access for the tracks class object.
        inline const AnimationClass& GetClass() const noexcept
        { return *mClass; }
        // Shortcut operator for accessing the members of the track's class object.
        inline const AnimationClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        // the class object
        std::shared_ptr<const AnimationClass> mClass;
        // For each node we keep a list of actions that are to be performed
        // at specific times.
        struct NodeTrack {
            std::string node;
            std::unique_ptr<Actuator> actuator;
            mutable bool started = false;
            mutable bool ended   = false;
        };
        std::vector<NodeTrack> mTracks;
        // One time delay before starting the animation.
        float mDelay = 0.0f;
        // current play back time for this track.
        float mCurrentTime = 0.0f;
    };

    class AnimationStateClass
    {
    public:
        explicit AnimationStateClass(std::string id = base::RandomString(10));

        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }

        std::size_t GetHash() const noexcept;

        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        AnimationStateClass Clone() const;
    private:
        std::string mName;
        std::string mId;
    };

    class AnimationStateTransitionClass
    {
    public:
        explicit AnimationStateTransitionClass(std::string id = base::RandomString(10));

        inline void SetDuration(float duration) noexcept
        { mDuration = duration; }
        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }
        inline std::string GetDstStateId() const noexcept
        { return mDstStateId; }
        inline std::string GetSrcStateId() const noexcept
        { return mSrcStateId; }
        inline void SetSrcStateId(std::string id) noexcept
        { mSrcStateId = std::move(id); }
        inline void SetDstStateId(std::string id) noexcept
        { mDstStateId = std::move(id); }
        inline float GetDuration() const noexcept
        { return mDuration; }

        std::size_t GetHash() const noexcept;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        AnimationStateTransitionClass Clone() const;
    private:
        std::string mName;
        std::string mId;
        std::string mSrcStateId;
        std::string mDstStateId;
        float mDuration = 0.0f;
    };


    class AnimatorClass
    {
    public:
        explicit AnimatorClass(std::string id = base::RandomString(10));

        void AddState(const AnimationStateClass& state)
        { mStates.push_back(state); }
        void AddState(AnimationStateClass&& state)
        { mStates.push_back(std::move(state)); }
        void AddTransition(const AnimationStateTransitionClass& transition)
        { mTransitions.push_back(transition); }
        void AddTransition(AnimationStateTransitionClass&& transition)
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
        inline const AnimationStateClass& GetState(size_t index) const noexcept
        { return base::SafeIndex(mStates, index); }
        inline const AnimationStateTransitionClass& GetTransition(size_t index) const noexcept
        { return base::SafeIndex(mTransitions, index); }
        inline AnimationStateClass& GetState(size_t index) noexcept
        { return base::SafeIndex(mStates, index); }
        inline AnimationStateTransitionClass& GetTransition(size_t index) noexcept
        { return base::SafeIndex(mTransitions, index); }
        inline void ClearStates() noexcept
        { mStates.clear(); }
        inline void ClearTransitions() noexcept
        { mTransitions.clear(); }

        void DeleteTransitionById(const std::string& id);
        void DeleteStateById(const std::string& id);
        const AnimationStateClass* FindStateById(const std::string& id) const noexcept;
        const AnimationStateClass* FindStateByName(const std::string& name) const noexcept;
        const AnimationStateTransitionClass* FindTransitionByName(const std::string& name) const noexcept;
        const AnimationStateTransitionClass* FindTransitionById(const std::string& id) const noexcept;

        AnimationStateClass* FindStateById(const std::string& id) noexcept;
        AnimationStateClass* FindStateByName(const std::string& name) noexcept;
        AnimationStateTransitionClass* FindTransitionByName(const std::string& name) noexcept;
        AnimationStateTransitionClass* FindTransitionById(const std::string& id) noexcept;

        std::size_t GetHash() const noexcept;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        AnimatorClass Clone() const;
    private:
        std::string mName;
        std::string mId;
        std::string mInitState;
        std::string mScriptId;
        std::vector<AnimationStateClass> mStates;
        std::vector<AnimationStateTransitionClass> mTransitions;
    };

    using AnimationState = AnimationStateClass;
    using AnimationTransition = AnimationStateTransitionClass;

    class Animator
    {
    public:
        struct EnterState {
            const AnimationState* state;
        };
        struct LeaveState {
            const AnimationState* state;
        };
        struct UpdateState {
            const AnimationState* state;
            float time;
            float dt;
        };
        struct StartTransition {
            const AnimationState* from;
            const AnimationState* to;
            const AnimationTransition* transition;
        };
        struct FinishTransition {
            const AnimationState* from;
            const AnimationState* to;
            const AnimationTransition* transition;
        };
        struct UpdateTransition {
            const AnimationState* from;
            const AnimationState* to;
            const AnimationTransition* transition;
            float time;
            float dt;
        };
        struct EvalTransition {
            const AnimationState* from;
            const AnimationState* to;
            const AnimationTransition* transition;
        };

        using Action = std::variant<std::monostate,
           EnterState, LeaveState, UpdateState,
           StartTransition, FinishTransition, UpdateTransition, EvalTransition>;

        using Value = std::variant<bool, int, float,
            std::string, glm::vec2>;

        explicit Animator(AnimatorClass&& klass);
        explicit Animator(const AnimatorClass& klass);
        explicit Animator(const std::shared_ptr<const AnimatorClass>& klass);

        // Update the animator state machine state.
        // When the current animator state is updated the resulting actions that should
        // be taken  as a result are stored into the actions vector. The caller can then
        // perform some actions (such as call some evaluation code) or choose do nothing.
        // The only action that should be handled is the response to a state transition
        // evaluation, otherwise no state transitions are possible.
        void Update(float dt, std::vector<Action>* actions);
        // Update the animator state machine by starting a transition from the current
        // state towards the next state.
        void Update(const AnimationTransition* transition, const AnimationState* next);

        enum class State {
            InTransition, InState
        };
        // Get the current animator state state, i.e. whether in state or in transition.
        State GetAnimatorState() const noexcept;

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
        inline const AnimationState* GetCurrentState() const noexcept
        { return mCurrent; }
        // Get the next state (the state we're transitioning to) when doing a transition.
        // The next state only exists during transition.
        inline const AnimationState* GetNextState() const noexcept
        { return mNext; }
        // Get the prev state (the state we're transitioning from) when doing a transition.
        // The prev state only exists during transition.
        inline const AnimationState* GetPrevState() const noexcept
        { return mPrev; }
        // Get the current transition if any.
        inline const AnimationTransition* GetTransition() const noexcept
        { return mTransition; }
        // Get the currently accumulated time. If there's a current transition
        // then the time value measures the time elapsed in the transition, other
        // it measures the time spent in some particular state.
        inline float GetTime() const noexcept
        { return mTime; }
        // Class object
        inline const AnimatorClass& GetClass() const
        { return *mClass; }
        inline const AnimatorClass* operator ->() const
        { return mClass.get(); }

        inline std::string GetName() const noexcept
        { return mClass->GetName(); }
        inline std::string GetId() const noexcept
        { return mClass->GetId(); }
    public:
        std::shared_ptr<const AnimatorClass> mClass;
        // Current state if any. no state during transition
        const AnimationState* mCurrent = nullptr;
        // previous state when doing a transition (if any)
        const AnimationState * mPrev = nullptr;
        // next state when doing a transition (if any)
        const AnimationState* mNext = nullptr;
        // the current transition (if any)
        const AnimationTransition* mTransition = nullptr;
        // the current transition or state time.
        float mTime = 0.0f;
        // some values that can be routed through the animator
        // from one part of the system (the entity script) to another
        // part of the system (the animator script)
        std::unordered_map<std::string, Value> mValues;
    };

    std::unique_ptr<Animation> CreateAnimationInstance(const std::shared_ptr<const AnimationClass>& klass);
    std::unique_ptr<Animator> CreateAnimatorInstance(const std::shared_ptr<const AnimatorClass>& klass);
    std::unique_ptr<Animator> CreateAnimatorInstance(const AnimatorClass& klass);
    std::unique_ptr<Animator> CreateAnimatorInstance(AnimatorClass&& klass);

} // namespace
