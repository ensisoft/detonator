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

#include <string>
#include <memory>

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

    std::unique_ptr<Animation> CreateAnimationInstance(const std::shared_ptr<const AnimationClass>& klass);

} // namespace
